#include "GameManager.h"
#include "NPCFactory.h"
#include <iostream>
#include <sstream>

GameManager::GameManager() : currentTime(TimeOfDay::MORNING), isRunning(true) {}

void GameManager::initGame() {
    player = std::make_unique<Player>("Protagonist");
    targetNPCs.push_back(NPCFactory::createRandomNPC());
    std::cout << "游戏初始化完成。输入 'help' 查看指令。\n";
}

void GameManager::runLoop() {
    std::string input;
    while (isRunning) {
        std::cout << "\n> ";
        std::getline(std::cin, input);
        processCommand(input);
    }
}

void GameManager::processCommand(const std::string& command) {
    std::istringstream iss(command);
    std::string action;
    iss >> action;

    if (action == "talk") {
        std::string npcIndexStr;
        iss >> npcIndexStr;
        // 简化处理：默认和第一个 NPC 聊天
        if (!targetNPCs.empty()) {
            std::cout << "你说: ";
            std::string dialogue;
            std::getline(std::cin, dialogue);
            
            // 获取 AI 回复
            std::string reply = targetNPCs[0]->interact(dialogue);
            std::cout << targetNPCs[0]->getName() << " 回复: " << reply << std::endl;
            
            // 模拟好感度变化与时间流逝
            targetNPCs[0]->changeAffection(5); 
            advanceTime();
        }
    } else if (action == "look") {
        if (!targetNPCs.empty()) {
            targetNPCs[0]->generatePortraitAPI();
        }
    } else if (action == "quit") {
        isRunning = false;
        std::cout << "退出游戏。\n";
    } else {
        std::cout << "未知指令。尝试: talk, look, quit\n";
    }
}

void GameManager::advanceTime() {
    if (currentTime == TimeOfDay::MORNING) { currentTime = TimeOfDay::NOON; std::cout << "【时间流逝：到了中午】\n"; }
    else if (currentTime == TimeOfDay::NOON) { currentTime = TimeOfDay::NIGHT; std::cout << "【时间流逝：到了晚上】\n"; }
    else { currentTime = TimeOfDay::MORNING; std::cout << "【时间流逝：新的一天开始了】\n"; }
}