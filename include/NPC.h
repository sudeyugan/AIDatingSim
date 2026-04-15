#pragma once
#include <string>
#include <memory>
#include <deque>
#include "json.hpp"
#include "Player.h"
#include <future>
#include "ImageLoader.h"

class Player;

struct NPCResponse {
    std::string reply;
    bool trigger_event; // 是否呼叫 GM 触发事件
    bool ready_to_transition;// NPC认为当前场景是否可以结束
};

class NPC {
private:
    std::string name;
    std::string basePersona; // 核心人设 (例如：傲娇学霸、温柔前台)
    int affection; // 好感度 (0-100)
    std::deque<nlohmann::json> chatHistory; 
    const size_t MAX_HISTORY = 40;
    std::string portraitPath;        
    ImageLoader portraitImage;

public:
    NPC(std::string n, std::string persona);

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

    // 核心逻辑：根据当前好感度动态生成 System Prompt
    std::string generateDynamicSystemPrompt(const Player& player) const;

    // 与玩家交互 (接收玩家输入，返回 NPC 文本回复)
    NPCResponse interact(const std::string& playerInput, const Player& player);

    void injectSceneMemory(const std::string& sceneDescription);

    // 调用生图 API 生成角色立绘
    void generatePortraitAPI() const;

    // 修改好感度
    void changeAffection(int amount);
    std::string getName() const;

    std::future<bool> generatePortraitAsync();

    ImageLoader& getPortraitImage() { return portraitImage; }
    std::string getPortraitPath() const { return portraitPath; }

    void reloadTexture();
};