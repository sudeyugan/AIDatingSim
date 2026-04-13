#pragma once
#include <string>
#include <vector>
#include <memory>
#include <future>
#include <chrono>

#include "Player.h"
#include "NPC.h"
#include "ProfileGenerator.h"
#include "GameEvent.h"
#include "ImageLoader.h"

class GameManager {
private:
    bool isRunning = true; // 控制游戏循环

    // ================= [核心游戏状态] =================
    Player mainPlayer;
    CharacterProfile currentNPC;
    std::unique_ptr<NPC> activeNPC;
    std::vector<std::pair<std::string, std::string>> uiChatHistory;
    char worldSettingBuf[256];

    // ================= [异步状态机 (原 main.cpp 的变量)] =================
    std::future<CharacterProfile> futureProfile;
    bool isGeneratingNPC = false;

    std::future<NPCResponse> futureReply;
    bool isWaitingForReply = false;

    std::future<std::pair<std::string, std::string>> futurePlayerProfile;
    bool isGeneratingPlayer = false;

    std::future<std::string> futureWorldSetting;
    bool isGeneratingWorld = false;

    std::future<GameEvent> futureEvent;
    bool isGeneratingEvent = false;
    GameEvent currentEvent;
    bool isEventActive = false;

    std::future<std::string> futureEncounter;
    bool isGeneratingEncounter = false;
    bool hasEncounterStarted = false;

    ImageLoader npcImageLoader;
    std::future<bool> futurePortrait;
    bool isGeneratingPortrait = false;

    void checkAsyncTasks(); // 专门用于处理所有的 .wait_for(0) 轮询
    void renderUI();        // 专门用于渲染 ImGui 画面

public:
    GameManager();
    void initGame();
    
    // 每帧调用的主逻辑
    void runLoop(); 
    
    // 给 main.cpp 检查是否退出的接口
    bool isGameRunning() const { return isRunning; } 
};