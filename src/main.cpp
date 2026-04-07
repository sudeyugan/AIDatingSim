#include "GameManager.h"
#include "ConfigManager.h"
#include <iostream>
#include <windows.h>

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 1. 启动时优先加载配置
    if (!ConfigManager::getInstance().loadConfig("config.json")) {
        std::cerr << "配置加载失败，请检查工程根目录下是否存在 config.json 并且格式正确。\n";
        return 1; // 异常退出
    }

    // 2. 配置加载成功，进入游戏核心循环
    std::cout << "=== 欢迎来到 AI 驱动恋爱模拟器 ===" << std::endl;
    GameManager game;
    game.initGame();
    game.runLoop();
    
    return 0;
}