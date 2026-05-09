#pragma once
#include <string>
#include <vector>
#include <memory>
#include <future>
#include <chrono>
#include <algorithm>

#include "Player.h"
#include "NPC.h"
#include "ProfileGenerator.h"
#include "GameEvent.h"
#include "ImageLoader.h"
#include "imgui.h"

enum class TimeOfDay { MORNING, NOON, NIGHT };

enum class UINavTab { CHAT, PROFILE, SYSTEM };

class GameManager {
private:
    bool isRunning = true; 

    UINavTab currentTab = UINavTab::CHAT;

    TimeOfDay currentTime = TimeOfDay::MORNING;

    void advanceTime();

    // 用于存放后台异步任务的容器
    std::vector<std::future<void>> backgroundTasks;

    // 清理已完成任务的私有方法
    void cleanFinishedTasks();

    int chatTurns = 0; // 聊天回合计数器，用于控制时间流逝

    bool isGeneratingTransition = false;
    std::future<std::pair<std::string, bool>> futureTransition;

    // ================= [核心游戏状态] =================
    Player mainPlayer;
    CharacterProfile currentNPC;
    std::shared_ptr<NPC> activeNPC; // 注意：改用 shared_ptr 方便后续存档和传参
    std::vector<std::pair<std::string, std::string>> uiChatHistory;
    char worldSettingBuf[2048];

    // ================= [存档系统变量] =================
    std::vector<std::string> availableSaves;
    struct SaveFileInfo {
    std::string fileName;  // 内部真实文件名（如 save_123.json，隐藏不显示）
    std::string npcName;   // 邂逅对象的名字
    std::string timeStr;   // 游戏内时间阶段
    int affection;         // 核心：好感度/羁绊值
    };
    std::vector<SaveFileInfo> parsedSaves;

    std::string currentBgPath = "";

    // ================= [异步状态机变量] =================
    bool isAIBusy = false;
    std::future<bool> currentAITask;            
    std::shared_ptr<NPC> interactingNPC = nullptr; 

    std::future<CharacterProfile> futureProfile;
    bool isGeneratingNPC = false;

    std::future<NPCResponse> futureReply;
    bool isWaitingForReply = false;

    std::future<Player> futurePlayerProfile;
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

    ImageLoader chatBgLoader;
    std::future<std::string> futureBg;
    bool isGeneratingBg = false;

    ImageLoader playerImageLoader;
    std::future<bool> futurePlayerPortrait;
    bool isGeneratingPlayerPortrait = false;

    // ================= [内部私有方法] =================
    void checkAsyncTasks(); 
    void renderUI(); 
    void drawChatBubble(const std::string& name, const std::string& text, int type, ImTextureID avatar_tex = 0);

public:
    GameManager();
    void initGame();
    void runLoop(); 
    bool isGameRunning() const { return isRunning; } 
    void startNewGame();

    // 存档与防抖接口
    void scanSaveFiles();
    bool saveGame(const std::string& filename);
    bool loadGame(const std::string& filename);
    void startAITask(std::shared_ptr<NPC> npc, std::future<bool> task);

    // 提供一个接口供其他组件（如 NPC）添加异步任务
    void addBackgroundTask(std::future<void>&& task);

};