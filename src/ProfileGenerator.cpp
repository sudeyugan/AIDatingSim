#include "ProfileGenerator.h"
#include "ConfigManager.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT 
#include "httplib.h"
#include "json.hpp"
#include <iostream>
#include <random>
#include <chrono>

using json = nlohmann::json;

static std::string getEntropySeed() {
    static std::mt19937 gen(std::chrono::system_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<> dis(1, 9999999);
    return std::to_string(dis(gen));
}

std::future<CharacterProfile> ProfileGenerator::generateRandomProfileAsync(const std::string& worldSetting) {
    return std::async(std::launch::async, [worldSetting]() -> CharacterProfile {
        CharacterProfile profile;
        
        std::string systemPrompt = 
            "你是一个资深的恋爱游戏剧本家。\n"
            "【当前世界观设定】：" + worldSetting + "\n" 
            "【系统随机熵种子】：" + getEntropySeed() + "\n"
            "请你在这个世界观的框架下，随机创作一个极具魅力的女主角（或男主角）设定。职业、外貌必须符合该世界观。\n"
            "要求：拒绝扁平化的标签，要立体，可以带有心理创伤或执念。\n"
            "【极其重要】：请根据上面的随机熵种子，确保本次生成的姓名、职业、性格和创伤绝对独特，坚决不使用常见的套路和重名！\n"
            "必须严格输出纯 JSON 格式，格式如下：\n"
            "{\n"
            "  \"name\": \"名字\",\n"
            "  \"appearance\": \"外貌描写（画面感强）\",\n"
            "  \"personality_core\": \"表层与深层性格\",\n"
            "  \"hidden_trauma\": \"不为人知的秘密或创伤\",\n"
            "  \"initial_attitude\": \"对陌生人的初始态度\",\n"
            "  \"initial_affection\": 10\n"
            "}";

        json requestBody = {
            {"model", "deepseek-chat"},
            {"messages", {{{"role", "system"}, {"content", systemPrompt}}}},
            {"response_format", {{"type", "json_object"}}},
            {"temperature", 0.9}, // 调高温度，让生成的角色更多样化
            {"max_tokens", 1500}
        };

        std::string apiKey = ConfigManager::getInstance().getApiKey();
        
        apiKey.erase(std::remove(apiKey.begin(), apiKey.end(), '\n'), apiKey.end());
        apiKey.erase(std::remove(apiKey.begin(), apiKey.end(), '\r'), apiKey.end());
        apiKey.erase(apiKey.find_last_not_of(" ") + 1);
        
        httplib::Client cli("https://api.deepseek.com");
        httplib::Headers headers = {
            {"Authorization", "Bearer " + apiKey}
        };

        cli.set_read_timeout(60, 0);  
        auto res = cli.Post("/chat/completions", headers, requestBody.dump(), "application/json");

        if (res && res->status == 200) {
            try {
                json resJson = json::parse(res->body);
                std::string contentStr = resJson["choices"][0]["message"]["content"];
                
                // 清理可能的 markdown 标记
                size_t start = contentStr.find('{');
                size_t end = contentStr.rfind('}');
                if (start != std::string::npos && end != std::string::npos) {
                    contentStr = contentStr.substr(start, end - start + 1);
                }

                json profileJson = json::parse(contentStr);
                profile.name = profileJson.value("name", "未知");
                profile.appearance = profileJson.value("appearance", "");
                profile.personality_core = profileJson.value("personality_core", "");
                profile.hidden_trauma = profileJson.value("hidden_trauma", "");
                profile.initial_attitude = profileJson.value("initial_attitude", "");
                profile.initial_affection = profileJson.value("initial_affection", 0);
                profile.is_generated = true;

            } catch (const std::exception& e) {
                std::cerr << "解析档案 JSON 失败: " << e.what() << std::endl;
            }
        }else {
            if (res) {
                // 请求发出去了，但服务器返回了错误（如 401 权限错误，400 格式错误等）
                std::cerr << "[Error] API 拒绝了请求。状态码: " << res->status << "\n"
                          << "返回详情: " << res->body << std::endl;
            } else {
                // 请求根本没发出去（例如证书验证失败、没网、或者没加 SSL 宏）
                auto err = res.error();
                std::cerr << "[Error] 网络底层连接失败，错误类型: " << httplib::to_string(err) << std::endl;
            }
        }

        return profile;
    });
}

// 生成玩家档案的实现
std::future<std::pair<std::string, std::string>> ProfileGenerator::generatePlayerProfileAsync(const std::string& worldSetting) {
    return std::async(std::launch::async, [worldSetting]() {
        std::pair<std::string, std::string> result = {"神秘人", "一个失去记忆的旅者。"};
        
        std::string systemPrompt = 
            "你是一个恋爱文字游戏的 Game Master。\n"
            "【当前世界观设定】：" + worldSetting + "\n" 
            "【系统随机熵种子】：" + getEntropySeed() + "\n"
            "请务必在这个世界观的框架下，为玩家随机生成一个极具代入感的男主角身世。\n"
            "要求：身份应该是符合该世界观的特定职业。\n"
            "【极其重要】：请根据上面的随机熵种子，确保本次生成的姓名、职业、性格绝对独特，坚决不使用常见的套路和重名！\n"
            "必须严格输出纯 JSON 格式，格式如下：\n"
            "{\n"
            "  \"name\": \"名字\",\n"
            "  \"backstory\": \"背景设定（要足够详细，包括方方面面，有画面感）\"\n"
            "}";

        json requestBody = {
            {"model", "deepseek-chat"},
            {"messages", {{{"role", "system"}, {"content", systemPrompt}}}},
            {"response_format", {{"type", "json_object"}}},
            {"temperature", 0.9},
            {"max_tokens", 1500}
        };

        std::string apiKey = ConfigManager::getInstance().getApiKey();
        apiKey.erase(std::remove(apiKey.begin(), apiKey.end(), '\n'), apiKey.end());
        apiKey.erase(std::remove(apiKey.begin(), apiKey.end(), '\r'), apiKey.end());
        apiKey.erase(apiKey.find_last_not_of(" ") + 1);

        httplib::Client cli("https://api.deepseek.com");
        httplib::Headers headers = {
            {"Authorization", "Bearer " + apiKey}
        };

        cli.set_read_timeout(60, 0);  
        auto res = cli.Post("/chat/completions", headers, requestBody.dump(), "application/json");

        if (res && res->status == 200) {
            try {
                json resJson = json::parse(res->body);
                std::string contentStr = resJson["choices"][0]["message"]["content"];
                
                size_t start = contentStr.find('{');
                size_t end = contentStr.rfind('}');
                if (start != std::string::npos && end != std::string::npos) {
                    contentStr = contentStr.substr(start, end - start + 1);
                }

                json profileJson = json::parse(contentStr);
                result.first = profileJson.value("name", "神秘人");
                result.second = profileJson.value("backstory", "一个失去记忆的旅者。");

            } catch (const std::exception& e) {
                std::cerr << "解析玩家档案 JSON 失败: " << e.what() << std::endl;
            }
        } else {
            if (res) {
                std::cerr << "[Error] API 拒绝了请求。\n"
                          << "状态码: " << res->status << "\n"
                          << "返回详情: " << res->body << std::endl;
            } else {
                auto err = res.error();
                std::cerr << "[Error] 网络底层连接失败，错误类型: " << httplib::to_string(err) << std::endl;
            }
        }

        return result;
    });
}

std::future<std::string> ProfileGenerator::generateRandomWorldSettingAsync() {
    return std::async(std::launch::async, []() {
        std::string result = "现代日常都市"; // 默认兜底
        
        std::string systemPrompt = 
            "你是一个富有想象力的世界观架构师。\n"
            "请随机生成一个极具创意、甚至有些脑洞大开的游戏世界观背景（例如：蒸汽朋克魔法帝国、被巨型真菌吞噬的末日废土、规则怪谈盛行的现代校园等）。\n"
            "必须严格输出纯 JSON 格式，格式如下：\n"
            "{\n"
            "  \"world_setting\": \"世界观描述（非常详细，包括方方面面）\"\n"
            "}";

        json requestBody = {
            {"model", "deepseek-chat"},
            {"messages", {{{"role", "system"}, {"content", systemPrompt}}}},
            {"response_format", {{"type", "json_object"}}},
            {"temperature", 1.2}, // 极高的温度，激发 AI 的脑洞
            {"max_tokens", 1500}
        };

        std::string apiKey = ConfigManager::getInstance().getApiKey();
        apiKey.erase(std::remove(apiKey.begin(), apiKey.end(), '\n'), apiKey.end());
        apiKey.erase(std::remove(apiKey.begin(), apiKey.end(), '\r'), apiKey.end());
        apiKey.erase(apiKey.find_last_not_of(" ") + 1);

        httplib::Client cli("https://api.deepseek.com");
        httplib::Headers headers = { {"Authorization", "Bearer " + apiKey} };
        cli.set_read_timeout(60, 0);  
        auto res = cli.Post("/chat/completions", headers, requestBody.dump(), "application/json");

        if (res && res->status == 200) {
            try {
                json resJson = json::parse(res->body);
                std::string contentStr = resJson["choices"][0]["message"]["content"];
                size_t start = contentStr.find('{');
                size_t end = contentStr.rfind('}');
                if (start != std::string::npos && end != std::string::npos) {
                    contentStr = contentStr.substr(start, end - start + 1);
                }
                json profileJson = json::parse(contentStr);
                result = profileJson.value("world_setting", "现代日常都市");
            } catch (...) {}
        }
        return result;
    });
}

std::future<GameEvent> ProfileGenerator::generateRandomEventAsync(const std::string& worldSetting, const Player& player, const CharacterProfile& npc, const std::string& chatContext) {
    return std::async(std::launch::async, [worldSetting, player, npc, chatContext]() {
        GameEvent event;
        
        std::string systemPrompt = 
            "你是一个极其优秀的 TRPG Game Master (游戏主持)。\n"
            "【当前世界观】：" + worldSetting + "\n"
            "【玩家设定】：" + player.getName() + "，" + player.getBackstory() + "\n"
            "【当前互动的 NPC】：" + npc.name + "，" + npc.personality_core + "，隐藏执念：" + npc.hidden_trauma + "\n"
            "【最近的对话上下文】\n" + chatContext + "\n"
            "NPC 刚刚发出了推进剧情的信号。请你务必【无缝顺承当前的对话情境】，生成一个推动故事发展的【剧情事件】。\n"
            "注意：它不一定是激烈的突发危险，也可以是：环境的微妙变化（如突然下雨）、一个引人深思的线索出现、NPC 做出某个特殊动作、或者一个新人物介入。\n"
            "提供3个符合当前情境的不同选项（例如：A.主动询问 B.静观其变 C.转移话题）。\n"
            "必须严格输出纯 JSON 格式：\n"
            "{\n"
            "  \"description\": \"事件或场景的生动描写（注重电影般的画面感和文学张力）\",\n"
            "  \"choices\": [\"选项1文字\", \"选项2文字\", \"选项3文字\"]\n"
            "}";

        json requestBody = {
            {"model", "deepseek-chat"},
            {"messages", {{{"role", "system"}, {"content", systemPrompt}}}},
            {"response_format", {{"type", "json_object"}}},
            {"temperature", 0.9},
            {"max_tokens", 1500}
        };

        std::string apiKey = ConfigManager::getInstance().getApiKey();
        apiKey.erase(std::remove(apiKey.begin(), apiKey.end(), '\n'), apiKey.end());
        apiKey.erase(std::remove(apiKey.begin(), apiKey.end(), '\r'), apiKey.end());
        apiKey.erase(apiKey.find_last_not_of(" ") + 1);

        httplib::Client cli("https://api.deepseek.com");
        httplib::Headers headers = { {"Authorization", "Bearer " + apiKey} };
        cli.set_read_timeout(60, 0);  
        auto res = cli.Post("/chat/completions", headers, requestBody.dump(), "application/json");

        if (res && res->status == 200) {
            try {
                json resJson = json::parse(res->body);
                std::string contentStr = resJson["choices"][0]["message"]["content"];
                size_t start = contentStr.find('{');
                size_t end = contentStr.rfind('}');
                if (start != std::string::npos && end != std::string::npos) {
                    contentStr = contentStr.substr(start, end - start + 1);
                }
                json eventJson = json::parse(contentStr);
                event.description = eventJson.value("description", "什么都没有发生。");
                if (eventJson.contains("choices") && eventJson["choices"].is_array()) {
                    for (const auto& choice : eventJson["choices"]) {
                        event.choices.push_back(choice.get<std::string>());
                    }
                }
                event.is_valid = true;
            } catch (const std::exception& e) {
                std::cerr << "解析事件 JSON 失败: " << e.what() << std::endl;
            }
        } else {
            std::cerr << "[Error] 事件生成失败" << std::endl;
        }

        return event;
    });
}

// 场景导入生成逻辑
std::future<std::string> ProfileGenerator::generateEncounterAsync(const std::string& worldSetting, const Player& player, const CharacterProfile& npc) {
    return std::async(std::launch::async, [worldSetting, player, npc]() {
        std::string result = "你们在这个世界中相遇了。";
        
        std::string systemPrompt = 
            "你是一个 TRPG 的 Game Master。\n"
            "【当前世界观】：" + worldSetting + "\n"
            "【玩家设定】：" + player.getName() + "，" + player.getBackstory() + "\n"
            "【NPC设定】：" + npc.name + "，" + npc.personality_core + "\n"
            "玩家和 NPC 刚刚在这个世界里降临。请你描写一段极具画面感的【初次相遇/命运交集】的场景作为游戏的开场白。\n"
            "说明他们是如何碰到一起的，他们现在正在哪里，气氛如何。直接将这段描写输出在 JSON 中。\n"
            "必须严格输出纯 JSON 格式：\n"
            "{\n"
            "  \"scene_description\": \"场景描写\"\n"
            "}";

        json requestBody = {
            {"model", "deepseek-chat"},
            {"messages", json::array({{{"role", "system"}, {"content", systemPrompt}}})},
            {"response_format", {{"type", "json_object"}}},
            {"temperature", 0.9},
            {"max_tokens", 1500}
        };

        std::string apiKey = ConfigManager::getInstance().getApiKey();
        apiKey.erase(std::remove(apiKey.begin(), apiKey.end(), '\n'), apiKey.end());
        apiKey.erase(std::remove(apiKey.begin(), apiKey.end(), '\r'), apiKey.end());
        apiKey.erase(apiKey.find_last_not_of(" ") + 1);

        httplib::Client cli("https://api.deepseek.com");
        cli.enable_server_certificate_verification(false);
        cli.set_read_timeout(120, 0); 
        httplib::Headers headers = { {"Authorization", "Bearer " + apiKey} };
        auto res = cli.Post("/chat/completions", headers, requestBody.dump(), "application/json");

        if (res && res->status == 200) {
            try {
                json resJson = json::parse(res->body);
                std::string contentStr = resJson["choices"][0]["message"]["content"];
                size_t start = contentStr.find('{');
                size_t end = contentStr.rfind('}');
                if (start != std::string::npos && end != std::string::npos) {
                    contentStr = contentStr.substr(start, end - start + 1);
                }
                json sceneJson = json::parse(contentStr);
                result = sceneJson.value("scene_description", "你们在这个世界中相遇了。");
            } catch (...) {}
        }
        return result;
    });
}
