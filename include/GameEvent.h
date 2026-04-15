#pragma once
#include <string>
#include <vector>

struct EventChoice {
    std::string text;
    std::string stat; 
};

struct GameEvent {
    std::string description;          // 事件的具体描述
    std::vector<EventChoice> choices; // 三个分支选项
    bool is_valid = false;            // 标记事件是否生成成功
};

