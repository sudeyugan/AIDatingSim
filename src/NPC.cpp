#define CPPHTTPLIB_OPENSSL_SUPPORT // 开启 HTTPS 支持
#include "ConfigManager.h"
#include "httplib.h"
#include "json.hpp" // 使用 nlohmann/json
#include "NPC.h"
#include <iostream>
#include <sstream>

using json = nlohmann::json;

NPC::NPC(std::string n, std::string persona) : name(n), basePersona(persona), affection(20) {}

std::string NPC::getName() const { return name; }

std::string NPC::generateDynamicSystemPrompt() const {
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

    return promptBuilder.str();
}

std::string NPC::interact(const std::string& playerInput) {
    std::string systemPrompt = generateDynamicSystemPrompt();
    
    // 1. 构造 DeepSeek (兼容 OpenAI) 的请求 JSON 
    json requestBody = {
        {"model", "deepseek-chat"}, // DeepSeek 对话模型
        {"messages", {
            {{"role", "system"}, {"content", systemPrompt}},
            {{"role", "user"}, {"content", playerInput}}
        }},
        {"temperature", 0.7} // 控制回复的随机性和创造性
    };

    std::string bodyStr = requestBody.dump();

    // 2. 发起 HTTPS POST 请求
    // 注意：如果是 Windows 环境报错缺少证书，可能需要配置 ca-bundle
    // 从单例中获取真实的 API Key
    std::string apiKey = ConfigManager::getInstance().getApiKey();
    httplib::Client cli("https://api.deepseek.com");
    httplib::Headers headers = {
        {"Authorization", "Bearer " + apiKey}, // !!! 在这里填入你的真实 API KEY !!!
        {"Content-Type", "application/json"}
    };

    std::cout << "[System] 正在等待 " << name << " 的思考..." << std::endl;
    
    cli.set_read_timeout(15, 0); // 设置超时时间为 15 秒
    auto res = cli.Post("/chat/completions", headers, bodyStr, "application/json");

    // 3. 解析返回结果
    if (res && res->status == 200) {
        try {
            json responseJson = json::parse(res->body);
            std::string reply = responseJson["choices"][0]["message"]["content"];
            return reply;
        } catch (const std::exception& e) {
            return "[Error] API JSON 解析失败: " + std::string(e.what());
        }
    } else {
        std::string errorMsg = res ? std::to_string(res->status) : "网络连接失败/超时";
        if(res && res->status != 200) {
            errorMsg += " - Body: " + res->body; // 打印具体错误信息
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