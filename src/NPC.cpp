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
    std::ostringstream promptBuilder;
    
    promptBuilder << "你是 " << name << "。" << basePersona << "。\n";
    promptBuilder << "我们在一个文字恋爱模拟游戏中。请用符合你当前对玩家好感度的语气进行沉浸式回复，字数控制在50字以内，不要超出角色设定。\n";

    promptBuilder << "【当前好感度状态】：";
    if (affection < 30) {
        promptBuilder << "冷淡。你对玩家不太信任，语气客气甚至疏远，可能有点不耐烦。";
    } else if (affection < 70) {
        promptBuilder << "熟络。你把玩家当成不错的朋友，愿意分享日常，语气轻松自然。";
    } else {
        promptBuilder << "暗恋。你对玩家充满好感，语气中带有暗示、关心，甚至会主动找话题。";
    }

    promptBuilder << "\n【重要指令】\n"
                  << "你必须严格以 JSON 格式输出，不要包含任何其他说明文字。格式如下：\n"
                  << "{\n"
                  << "  \"reply\": \"你的对话回复（扮演角色）\",\n"
                  << "  \"affection_change\": 好感度变化值（-5到+5的整数）,\n"
                  << "  \"trigger_event\": true/false (核心逻辑：当且仅当话题结束陷入僵局、情绪到达高潮、或者你认为当前场景需要外部力量推进剧情时，设为 true，呼叫系统介入；正常聊天时必须为 false)\n"
                  << "}";

    // 【将玩家背景注入给 AI
    promptBuilder << "\n【玩家当前面板信息】\n"
                  << "玩家名字：" << player.getName() << "\n"
                  << "玩家背景设定：" << player.getBackstory() << "（请结合此背景对玩家产生特定的主观印象）\n" 
                  << "魅力值：" << player.getCharm() << "（影响你对ta外貌的初始判断）\n"
                  << "才智值：" << player.getIntelligence() << "（影响你们学术/深度交流的顺畅度）\n"
                  << "财富值：" << player.getWealth() << "（影响ta展现出的财力）\n"
                  << "*注意：请根据你的人设性格，结合玩家的背景和属性，适当在字里行间对玩家做出潜在的反应。但不要像机器人一样直接报数字。\n";

    return promptBuilder.str();
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
                    std::string savePath = "current_npc_portrait.png";
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