#pragma once
#include <string>

// 深度角色档案结构体
struct CharacterProfile {
    std::string name;                 // 姓名
    std::string appearance;           // 外貌特征（方便后续接入生图）
    std::string personality_core;     // 核心性格
    std::string hidden_trauma;        // 隐藏的心理创伤或执念（剧本钩子）
    std::string initial_attitude;     // 对玩家的初始态度
    int initial_affection = 0;        // 初始好感度 (通常是 0-20)
    bool is_generated = false;        // 是否已经生成完毕
};