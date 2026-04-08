#include "ProfileGenerator.h"
#include "ConfigManager.h"
#include "httplib.h"
#include "json.hpp"
#include <iostream>

using json = nlohmann::json;

std::future<CharacterProfile> ProfileGenerator::generateRandomProfileAsync() {
    return std::async(std::launch::async, []() -> CharacterProfile {
        CharacterProfile profile;
        
        std::string systemPrompt = 
            "你是一个资深的恋爱冒险游戏(Galgame)剧本家。请你随机创作一个极具魅力的女主角（或男主角）设定。\n"
            "要求：拒绝扁平化的标签，要立体、有反差感，并带有一点心理创伤或执念。\n"
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
            {"temperature", 0.9} // 调高温度，让生成的角色更多样化
        };

        std::string apiKey = ConfigManager::getInstance().getApiKey();
        httplib::Client cli("https://api.deepseek.com");
        httplib::Headers headers = {
            {"Authorization", "Bearer " + apiKey},
            {"Content-Type", "application/json"}
        };

        cli.set_read_timeout(20, 0); 
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
        } else {
            std::cerr << "API 请求失败" << std::endl;
        }

        return profile;
    });
}