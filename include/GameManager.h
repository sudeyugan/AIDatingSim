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

enum class TimeOfDay { MORNING, NOON, NIGHT };

class GameManager {
private:
    bool isRunning = true; 

    TimeOfDay currentTime = TimeOfDay::MORNING;

    void advanceTime();

    int chatTurns = 0; // 聊天回合计数器，用于控制时间流逝

    // ================= [核心游戏状态] =================
    Player mainPlayer;
    CharacterProfile currentNPC;
    std::shared_ptr<NPC> activeNPC; // 注意：改用 shared_ptr 方便后续存档和传参
    std::vector<std::pair<std::string, std::string>> uiChatHistory;
    char worldSettingBuf[256];

    // ================= [存档系统变量] =================
    std::vector<std::string> availableSaves;

    // ================= [异步状态机变量] =================
    bool isAIBusy = false;
    std::future<bool> currentAITask;            
    std::shared_ptr<NPC> interactingNPC = nullptr; 

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

    // ================= [内部私有方法] =================
    void checkAsyncTasks(); 
    void renderUI();        

public:
    GameManager();
    void initGame();
    void runLoop(); 
    bool isGameRunning() const { return isRunning; } 

    // 存档与防抖接口
    void scanSaveFiles();
    bool saveGame(const std::string& filename);
    bool loadGame(const std::string& filename);
    void startAITask(std::shared_ptr<NPC> npc, std::future<bool> task);
};