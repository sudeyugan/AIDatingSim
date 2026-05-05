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
#include <vector>
#include <string>

using json = nlohmann::json;

static std::string base64_decode(const std::string &in) {
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

NPC::NPC(std::string n, std::string persona) : name(n), basePersona(persona), affection(20) {}

std::string NPC::getName() const { return name; }

std::string NPC::generateDynamicSystemPrompt(const Player& player) const {
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
        "2. 如果你的设定是高中生，请像一个真实的、有血有肉的女高中生一样说话。绝对不要像客服一样刻板，可以使用年轻人的语气词（如：哎、呢、嘛、哦、笨蛋）。\n"
        "3. 玩家的回复可能会采取具体的“【行动】”。当玩家采取行动时，你需要根据你的性格做出真实的反应。\n"
        "4. 每次回复请控制在 200 字以内，保持对话的来回节奏。\n\n"
        "【被动感知检定 (Passive Check)】\n"
        "当前玩家属性：共情=" + std::to_string(player.getEmpathy()) + "，智识=" + std::to_string(player.getIntellect()) + "。\n"
        "如果玩家的某项属性 >= 70，他就能察觉到普通人注意不到的隐藏细节。\n"
        "请务必判断当前情境，如果触发了感知，请将这些【隐藏信息】（如你极力掩饰的微表情、场景中暗藏的线索）放在 'passive_insights' 数组中返回；如果没有触发，请返回空数组。\n\n"
        "【好感度动态判定】\n"
        "你需要根据玩家的发言和行动，判断是否对你的好感度产生了影响：\n"
        "- 如果玩家的话语让你开心、心动、觉得有趣或觉得被尊重，好感度增加 (+1 到 +5)\n"
        "- 如果玩家的话语让你反感、觉得无聊、冒犯或不适，好感度降低 (-1 到 -5)\n"
        "- 如果只是平淡的日常对话，则为 0\n\n"
        "【输出格式要求】\n"
        "你必须且只能返回合法的纯 JSON 格式，绝不能包含任何 Markdown 标记或额外注释！格式如下：\n"
        "{\n"
        "  \"reply\": \"你说出的话，或者附带的动作描写（用括号括起来）\",\n"
        "  \"trigger_event\": false,\n"
        "  \"ready_to_transition\": false,\n"
        "  \"affection_change\": 0,\n"
        "  \"passive_insights\": [\"(共情检定成功) 你敏锐地察觉到她握紧了衣角，似乎在紧张。\"] \n"
        "}\n"
        "【字段说明】\n"
        "- trigger_event: 默认为 false。如果你觉得气氛到位，需要GM介入推进关系或发生突发事件，请设为 true。"
        "- ready_to_transition: 默认为 false。如果你觉得当前话题已经自然结束、或者到了该道别、换个地方的时候，请设为 true。";
        "- affection_change: 好感度变化值，严格限制在 -5 到 +5 之间。";
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

    // 1.1 压入 System Prompt (系统提示词通常不为空)
    messages.push_back({{"role", "system"}, {"content", systemPrompt}});

    // 1.2 压入历史记忆 (Context)
    for (const auto& msg : chatHistory) {
        // 【修改点】：增加非空校验
        // 只有当消息内容不为空时，才加入发送列表
        if (msg.contains("content") && !msg["content"].get<std::string>().empty()) {
            messages.push_back(msg);
        }
    }

    // 1.3 压入玩家本次输入
    messages.push_back({{"role", "user"}, {"content", playerInput}});
    
    json requestBody = {
        {"model", "deepseek-v4-pro"},
        {"messages", messages},
        {"temperature", 0.85},
        {"response_format", {{"type", "json_object"}}}
    };

    std::string bodyStr = requestBody.dump();

    // 发起 HTTP 请求...
    std::string apiKey = ConfigManager::getInstance().getApiKey();
    httplib::Client cli("https://api.deepseek.com");
    httplib::Headers headers = {
        {"Authorization", "Bearer " + apiKey},
        {"Content-Type", "application/json"}
    };

    int max_retries = 3;
    for (int attempt = 0; attempt < max_retries; ++attempt) {
        // 如果是重试，动态追加一条严厉的系统提示，防止模型连续输出同样的空白
        json currentMessages = messages; 
        if (attempt > 0) {
            std::cout << "[System] 重新尝试唤醒 " << name << " 的思绪... (第 " << attempt + 1 << " 次尝试)" << std::endl;
            currentMessages.push_back({{"role", "system"}, {"content", "错误：你刚才输出了空内容或非法格式。请务必输出一段包含 'reply' 字段的有效 JSON 文本，不要只有空格！"}});
        } else {
            std::cout << "[System] 正在等待 " << name << " 的思考..." << std::endl;
        }

        json requestBody = {
            {"model", "deepseek-v4-pro"},
            {"messages", messages},
            {"temperature", 0.85},
            {"response_format", {{"type", "json_object"}}},
        };

        std::string bodyStr = requestBody.dump();

        cli.set_read_timeout(30, 0); 
        auto res = cli.Post("/v1/chat/completions", headers, bodyStr, "application/json");
        // 3. 解析返回结果
        if (res && res->status == 200) {
            std::string aiContentStr = ""; 
            try {
                json responseJson = json::parse(res->body);
                aiContentStr = responseJson["choices"][0]["message"]["content"];
                
                // 如果全为空格，立刻抛出异常触发重试
                if (aiContentStr.find_first_not_of(" \t\n\r") == std::string::npos) {
                    throw std::runtime_error("AI 返回为空");
                }

                // 清理多余文本提取 JSON
                size_t jsonStart = aiContentStr.find('{');
                size_t jsonEnd = aiContentStr.rfind('}');
                if (jsonStart != std::string::npos && jsonEnd != std::string::npos && jsonEnd >= jsonStart) {
                    aiContentStr = aiContentStr.substr(jsonStart, jsonEnd - jsonStart + 1);
                } else {
                    throw std::runtime_error("未找到JSON括号截断");
                }

                // 解析 JSON
                json aiResult = json::parse(aiContentStr);
                std::string reply = aiResult.value("reply", "……（沉默）");
                
                if (reply.empty()) {
                    return {"[系统提示] 对方陷入了长久的沉默，请试着换个话题。", false, false, 0, {}};
                }

                int affectionChange = aiResult.value("affection_change", 0);
                bool triggerEvent = aiResult.value("trigger_event", false);
                bool readyToTransition = aiResult.value("ready_to_transition", false); 

                std::vector<std::string> insights;
                if (aiResult.contains("passive_insights") && aiResult["passive_insights"].is_array()) {
                    for (const auto& insight : aiResult["passive_insights"]) {
                        insights.push_back(insight.get<std::string>());
                    }
                }

                // 只有成功解析，才能将本次对话存入历史记录
                chatHistory.push_back({{"role", "user"}, {"content", playerInput}});

                json aiMemoryJson = {
                    {"reply", reply},
                    {"trigger_event", triggerEvent},
                    {"ready_to_transition", readyToTransition},
                    {"affection_change", affectionChange}
                };
                chatHistory.push_back({{"role", "assistant"}, {"content", aiMemoryJson.dump()}}); 

                while (chatHistory.size() > MAX_HISTORY) {
                    chatHistory.pop_front();
                }

                // 修改实际的好感度并打印日志
                if (affectionChange != 0) {
                    changeAffection(affectionChange);
                }

                // 将解析出来的所有变量打包并返回
                return {reply, triggerEvent, readyToTransition, affectionChange, insights};

            } catch (const std::exception& e) {
                std::cerr << "[Debug] 第 " << attempt + 1 << " 次解析失败: " << e.what() << std::endl;
                if (attempt == max_retries - 1) {
                    return {"[Error] 角色思绪有些混乱(多次重试均失败) 请再试一次", false, false, 0, {}};
                }
            }
        } else {
            std::cerr << "[Debug] 第 " << attempt + 1 << " 次请求网络失败。" << std::endl;
            if (attempt == max_retries - 1) {
                std::string errorMsg = res ? std::to_string(res->status) : "网络连接失败/超时";
                if(res && res->status != 200) errorMsg += " - Body: " + res->body; 
                return {"[Error] AI 通信超时: " + errorMsg, false, false, 0, {}};
            }
        }
    } 
    return {"[Error] 未知错误", false, false, 0, {}};
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
        // 1. 请求生图 API 
        // ==========================================
        std::string apiKey = ConfigManager::getInstance().getImageApiKey();
        
        apiKey.erase(std::remove(apiKey.begin(), apiKey.end(), '\n'), apiKey.end());
        apiKey.erase(std::remove(apiKey.begin(), apiKey.end(), '\r'), apiKey.end());
        apiKey.erase(apiKey.find_last_not_of(" ") + 1);

        // 构造生图提示词
        std::string prompt = 
            "顶级画师创作的日系视觉小说（Galgame）角色半身立绘，Masterpiece。现代高预算动画的宣传图风格（Anime Key Visual），融合了细腻的平涂技法与电影级的丁达尔光效。人物面部刻画唯美自然，充满青年向漫画的成熟感与空气感（绝对避免夸张的幼态大眼和廉价感）。"
            "请严格根据以下外貌设定进行精细描绘：【" + appearanceDesc + "】。"
            "【极度重要】：请确保图片背景尽量纯净、简约或大面积留白，以便于提取角色作为游戏立绘使用。";

        json requestBody = {
            {"model", "openai/gpt-5.4-image-2"}, 
            {"messages", json::array({{
                {"role", "user"},
                {"content", prompt}
            }})},
            {"modalities", json::array({"image"})} 
        };

        httplib::Client cli("https://openrouter.ai");
        cli.enable_server_certificate_verification(false);
        cli.set_read_timeout(240, 0); 

        httplib::Headers headers = { 
            {"Authorization", "Bearer " + apiKey},
            {"Content-Type", "application/json"}
        };
        // openrouter接口路径
        auto res = cli.Post("/api/v1/chat/completions", headers, requestBody.dump(), "application/json");

        if (!res || res->status != 200) {
            if (res) std::cerr << "OpenRouter生图失败: 状态码 " << res->status << " - " << res->body << std::endl;
            else std::cerr << "OpenRouter网络连接失败/超时" << std::endl;
        }

        try {
                json resJson = json::parse(res->body);
            std::string b64dataUrl = resJson["choices"][0]["message"]["images"][0]["image_url"]["url"];

            size_t commaPos = b64dataUrl.find(',');
            if (commaPos == std::string::npos) return false;
            std::string actualBase64 = b64dataUrl.substr(commaPos + 1);

            std::string binaryImageData = base64_decode(actualBase64);

            auto t = std::time(nullptr);
            struct tm tm_info;
#ifdef _WIN32
            localtime_s(&tm_info, &t);
#else
            localtime_r(&t, &tm_info);
#endif
            char timeBuf[128];
            std::strftime(timeBuf, sizeof(timeBuf), "%Y%m%d_%H%M%S", &tm_info);
            
            std::string savePath = "saves/npc_avatar_" + std::string(timeBuf) + ".png";
            std::ofstream file(savePath, std::ios::binary);
            if (file.is_open()) {
                file.write(binaryImageData.data(), binaryImageData.size());
                file.close();
                
                this->portraitPath = savePath;
                return true;
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