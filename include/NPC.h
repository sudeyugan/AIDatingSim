#pragma once
#include <string>
#include <memory>

class NPC {
private:
    std::string name;
    std::string basePersona; // 核心人设 (例如：傲娇学霸、温柔前台)
    int affection;           // 好感度 (0-100)

public:
    NPC(std::string n, std::string persona);

    // 核心逻辑：根据当前好感度动态生成 System Prompt
    std::string generateDynamicSystemPrompt() const;

    // 与玩家交互 (接收玩家输入，返回 NPC 文本回复)
    std::string interact(const std::string& playerInput);

    // 调用生图 API 生成角色立绘
    void generatePortraitAPI() const;

    // 修改好感度
    void changeAffection(int amount);
    std::string getName() const;
};