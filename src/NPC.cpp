#define CPPHTTPLIB_OPENSSL_SUPPORT // 开启 HTTPS 支持
#include "ConfigManager.h"
#include "httplib.h"
#include "json.hpp" 
#include "Player.h"
#include "NPC.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <regex>
#include <future>

using json = nlohmann::json;

NPC::NPC(std::string n, std::string persona) : name(n), basePersona(persona), affection(20) {}

std::string NPC::getName() const { return name; }

std::string NPC::generateDynamicSystemPrompt(const Player& player) const {
    // basePersona 里面包含了我们在 GameManager 传进来的 外貌、性格和隐藏执念
    
    std::string prompt = 
        "【系统设定】\n"
        "你现在正在扮演一个恋爱模拟游戏中的女性角色。请完全沉浸在这个角色中，绝对不要承认自己是AI或者语言模型。\n\n"
        "【你的身份】\n"
        "名字：" + name + "\n"
        "设定：" + basePersona + "\n"
        "当前对玩家的好感度：" + std::to_string(affection) + "/100\n\n"
        "【与你对话的人】\n"
        "名字：" + player.getName() + "，他的背景是：" + player.getBackstory() + "\n\n"
        "【扮演准则】\n"
        "1. 你的说话口吻、用词习惯必须绝对符合你的设定。\n"
        "2. 如果你的设定是普通高中生，请像一个真实的、有血有肉的女高中生一样说话。绝对不要像客服一样刻板，可以使用年轻人的语气词（如：哎、呢、嘛、哦、笨蛋）。\n"
        "3. 玩家的回复可能会采取具体的“【行动】”。当玩家采取行动时，你需要根据你的性格做出真实的反应。\n"
        "4. 每次回复请控制在 50 字以内，保持对话的来回节奏。\n\n"
        "【输出格式要求】\n"
        "你必须且只能返回合法的 JSON 格式：\n"
        "{\n"
        "  \"reply\": \"你说出的话，或者附带的动作描写（用括号括起来）\",\n"
        "  \"trigger_event\": false (如果你觉得气氛到位，需要GM介入推进关系或发生突发事件，请设为 true，否则平时都是 false)\n"
        "}";

    return prompt;
}

void NPC::injectSceneMemory(const std::string& sceneDescription) {
    // 以 system 的身份插入上下文，这样 NPC 就知道自己当前所处的环境了
    chatHistory.push_back({
        {"role", "system"}, 
        {"content", "【当前场景/剧情上下文】\n" + sceneDescription + "\n（请牢记目前所处的场景氛围，并在此情境下做出反应）"}
    });
}

NPCResponse NPC::interact(const std::string& playerInput, const Player& player) {
    std::string systemPrompt = generateDynamicSystemPrompt(player);
    
    // 1. 构建 messages 数组
    json messages = json::array();
    
    // 1.1 压入 System Prompt
    messages.push_back({{"role", "system"}, {"content", systemPrompt}});
    
    // 1.2 压入历史记忆 (Context)
    for (const auto& msg : chatHistory) {
        messages.push_back(msg);
    }
    
    // 1.3 压入玩家本次输入
    messages.push_back({{"role", "user"}, {"content", playerInput}});

    json requestBody = {
        {"model", "deepseek-chat"},
        {"messages", messages},
        // Deepseek 支持 response_format，强制其校验 json 输出
        {"response_format", {{"type", "json_object"}}}, 
        {"temperature", 0.7}
    };

    std::string bodyStr = requestBody.dump();

    // 发起 HTTP 请求...
    std::string apiKey = ConfigManager::getInstance().getApiKey();
    httplib::Client cli("https://api.deepseek.com");
    httplib::Headers headers = {
        {"Authorization", "Bearer " + apiKey},
        {"Content-Type", "application/json"}
    };

    std::cout << "[System] 正在等待 " << name << " 的思考..." << std::endl;
    cli.set_read_timeout(15, 0); 
    auto res = cli.Post("/chat/completions", headers, bodyStr, "application/json");

    // 3. 解析返回结果
    if (res && res->status == 200) {
        try {
            json responseJson = json::parse(res->body);
            std::string aiContentStr = responseJson["choices"][0]["message"]["content"];
            
            // 有时候 AI 会返回包裹着 ```json ``` 的字符串，简单做一个清理
            size_t jsonStart = aiContentStr.find('{');
            size_t jsonEnd = aiContentStr.rfind('}');
            if (jsonStart != std::string::npos && jsonEnd != std::string::npos && jsonEnd > jsonStart) {
                aiContentStr = aiContentStr.substr(jsonStart, jsonEnd - jsonStart + 1);
            }

            // 解析 AI 返回的 JSON 字符串
            json aiResult = json::parse(aiContentStr);
            std::string reply = aiResult.value("reply", "……（沉默）");
            int affectionChange = aiResult.value("affection_change", 0);
            bool triggerEvent = aiResult.value("trigger_event", false);

            // 将本次对话存入历史记录
            chatHistory.push_back({{"role", "user"}, {"content", playerInput}});
            // 注意：存入历史记录的是纯文本 reply，不要把 json 存进去干扰后面的对话
            chatHistory.push_back({{"role", "assistant"}, {"content", reply}}); 

            // 维护滑动窗口，防止 Token 爆炸
            while (chatHistory.size() > MAX_HISTORY) {
                chatHistory.pop_front();
            }

            // 动态触发好感度变化
            if (affectionChange != 0) {
                changeAffection(affectionChange);
            }

            return {reply, triggerEvent};

        } catch (const std::exception& e) {
            return {"[Error] API JSON 解析失败: " + std::string(e.what()), false};
        }
    } else {
        std::string errorMsg = res ? std::to_string(res->status) : "网络连接失败/超时";
        if(res && res->status != 200) {
            errorMsg += " - Body: " + res->body; 
        }
        return {"[Error] AI 通信失败: " + errorMsg, false};
    }
}
void NPC::generatePortraitAPI() const {
    std::cout << "[System] " << name << " 害羞地转过了头。(生图功能暂未开启)" << std::endl;
}

void NPC::changeAffection(int amount) {
    affection += amount;
    if(affection > 100) affection = 100;
    if(affection < 0) affection = 0;
    std::cout << "[System] " << name << " 的好感度变化了: " << (amount > 0 ? "+" : "") << amount << " (当前: " << affection << ")" << std::endl;
}

std::future<bool> NPC::generatePortraitAsync() {
    // 捕获当前的 NPC 外貌描述
    std::string appearanceDesc = this->basePersona; 
    
    return std::async(std::launch::async, [appearanceDesc, this]() {
        // ==========================================
        // 1. 请求生图 API (以标准 OpenAI DALL-E 为例)
        // ==========================================
        std::string apiKey = ConfigManager::getInstance().getImageApiKey();
        
        apiKey.erase(std::remove(apiKey.begin(), apiKey.end(), '\n'), apiKey.end());
        apiKey.erase(std::remove(apiKey.begin(), apiKey.end(), '\r'), apiKey.end());
        apiKey.erase(apiKey.find_last_not_of(" ") + 1);

        // 构造生图提示词
        std::string prompt = "二次元动漫风格，精美视觉小说角色立绘，杰作，高质量。" + appearanceDesc;

        json requestBody = {
            {"model", "cogview-4-250304"}, // 目前智谱最新的高质量生图模型
            {"prompt", prompt}
            // 智谱默认就是 1024x1024，可以不用额外传 size
        };

        httplib::Client cli("https://open.bigmodel.cn"); 
        cli.enable_server_certificate_verification(false);
        cli.set_read_timeout(120, 0); // 生图比较慢，给足 120 秒等待

        httplib::Headers headers = { {"Authorization", "Bearer " + apiKey} };
        
        // 智谱的 v4 接口路径
        auto res = cli.Post("/api/paas/v4/images/generations", headers, requestBody.dump(), "application/json");

        if (!res || res->status != 200) {
            if (res) std::cerr << "智谱生图失败: " << res->body << std::endl;
            return false; 
        }

        try {
            // ==========================================
            // 2. 解析 JSON 拿到图片的下载 URL
            // ==========================================
            json resJson = json::parse(res->body);
            std::string imageUrl = resJson["data"][0]["url"];

            // ==========================================
            // 3. 提取下载链接并保存图片到本地
            // ==========================================
            std::regex url_regex(R"(^https?://([^/]+)(/.*)$)");
            std::smatch url_match_result;
            if (std::regex_match(imageUrl, url_match_result, url_regex)) {
                std::string host = url_match_result[1];
                std::string path = url_match_result[2];

                httplib::Client dl_cli("https://" + host);
                dl_cli.enable_server_certificate_verification(false);
                dl_cli.set_read_timeout(60, 0);
                
                auto dl_res = dl_cli.Get(path);
                if (dl_res && dl_res->status == 200) {
                    std::string savePath = "saves/portrait_" + name + "_" + std::to_string(std::time(nullptr)) + ".png";
                    std::ofstream file(savePath, std::ios::binary);
                    file.write(dl_res->body.c_str(), dl_res->body.size());
                    file.close();
                    
                    this->portraitPath = savePath;
                    return true; // 大功告成！
                }
            }
        } catch (...) {
            return false;
        }
        return false;
    });
}

nlohmann::json NPC::toJson() const {
    return {
        {"name", name},
        {"basePersona", basePersona},
        {"affection", affection},
        {"portraitPath", portraitPath},
        {"chatHistory", chatHistory} // nlohmann::json 会自动处理 STL 容器嵌套 JSON
    };
}

void NPC::fromJson(const nlohmann::json& j) {
    name = j.value("name", "Unknown NPC");
    basePersona = j.value("basePersona", "");
    affection = j.value("affection", 0);
    portraitPath = j.value("portraitPath", "");
    
    if (j.contains("chatHistory")) {
        chatHistory = j["chatHistory"].get<std::deque<nlohmann::json>>();
    }
}

void NPC::reloadTexture() {
    // 只有当路径不为空时才去加载（避免空NPC或者还没生图的NPC报错）
    if (!portraitPath.empty()) {
        // LoadFromFile 会自动处理 stb_image 的读取和 glGenTextures
        bool success = portraitImage.LoadFromFile(portraitPath);
        if (!success) {
            std::cerr << "存档恢复警告：找不到立绘文件 " << portraitPath << std::endl;
        }
    }
}