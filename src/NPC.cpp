#include "NPC.h"
#include <iostream>
#include <sstream>

NPC::NPC(std::string n, std::string persona) : name(n), basePersona(persona), affection(20) {}

std::string NPC::generateDynamicSystemPrompt() const {
    std::ostringstream promptBuilder;
    
    // 1. 注入基础人设
    promptBuilder << "你是 " << name << "。" << basePersona << "\n";
    promptBuilder << "你在一个文字恋爱模拟游戏中。请用符合你当前对玩家好感度的语气进行简短回复（不超过50字）。\n";

    // 2. 动态情感分支注入
    promptBuilder << "【当前好感度状态】：";
    if (affection < 30) {
        promptBuilder << "冷淡。你对玩家不太信任，语气客气甚至疏远。只回答必要的问题。";
    } else if (affection < 70) {
        promptBuilder << "熟络。你把玩家当成不错的朋友，愿意分享一些日常，语气轻松自然。";
    } else {
        promptBuilder << "暗恋。你对玩家充满好感，语气中带有暗示、关心，甚至会主动找话题。";
    }

    return promptBuilder.str();
}

std::string NPC::interact(const std::string& playerInput) {
    std::string systemPrompt = generateDynamicSystemPrompt();
    
    // TODO: 在这里通过 HTTP 客户端 (如 libcurl) 调用 LLM API
    // 伪代码: response = LLMClient::post(systemPrompt, playerInput);
    
    std::string mockResponse = "[AI Reply based on: " + systemPrompt.substr(0, 30) + "...]";
    return mockResponse;
}

void NPC::generatePortraitAPI() const {
    std::string imagePrompt = "1girl, " + basePersona + ", highly detailed, anime style, visual novel sprite";
    // TODO: 调用生图 API (如调用当前模型的 image_generation 接口或外部 API)
    std::cout << "[System] 正在根据特征 [" << imagePrompt << "] 生成角色立绘..." << std::endl;
}

void NPC::changeAffection(int amount) {
    affection += amount;
    if(affection > 100) affection = 100;
    if(affection < 0) affection = 0;
}