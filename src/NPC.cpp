#define CPPHTTPLIB_OPENSSL_SUPPORT // 开启 HTTPS 支持
#include "ConfigManager.h"
#include "httplib.h"
#include "json.hpp" 
#include "Player.h"
#include "NPC.h"
#include <iostream>
#include <sstream>

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
                  << "你必须严格以 JSON 格式输出，不要包含任何其他说明文字或 Markdown 标记。格式如下：\n"
                  << "{\n"
                  << "  \"reply\": \"你的对话回复（扮演角色）\",\n"
                  << "  \"affection_change\": 你对玩家这句话的好感度变化值（-5到+5之间的整数，根据玩家发言的得体程度决定）\n"
                  << "}";

    promptBuilder << "\n【玩家当前面板信息】\n"
                  << "玩家名字：" << player.getName() << "\n"
                  << "魅力值：" << player.getCharm() << "（影响你对ta外貌的初始判断）\n"
                  << "才智值：" << player.getIntelligence() << "（影响你们学术/深度交流的顺畅度）\n"
                  << "财富值：" << player.getWealth() << "（影响ta展现出的财力）\n"
                  << "*注意：请根据你的人设性格，适当在字里行间对玩家的数值高低做出潜在的反应（例如势利眼会看重财富，慕强会看重才智）。但不要像机器人一样直接报数字。\n";

    return promptBuilder.str();
}

std::string NPC::interact(const std::string& playerInput, const Player& player) {
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

            return reply;

        } catch (const std::exception& e) {
            return "[Error] API JSON 解析失败或不符合预期格式: " + std::string(e.what()) + "\n原始返回: " + res->body;
        }
    } else {
        std::string errorMsg = res ? std::to_string(res->status) : "网络连接失败/超时";
        if(res && res->status != 200) {
            errorMsg += " - Body: " + res->body; 
        }
        return "[Error] AI 通信失败: " + errorMsg;
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