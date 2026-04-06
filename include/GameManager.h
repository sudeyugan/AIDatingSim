#pragma once
#include "Player.h"
#include "NPC.h"
#include <vector>
#include <memory>

enum class TimeOfDay { MORNING, NOON, NIGHT };

class GameManager {
private:
    std::unique_ptr<Player> player;
    std::vector<std::shared_ptr<NPC>> targetNPCs;
    TimeOfDay currentTime;
    bool isRunning;

    void advanceTime();
    void processCommand(const std::string& command);

public:
    GameManager();
    void initGame();
    void runLoop(); // 核心循环
};