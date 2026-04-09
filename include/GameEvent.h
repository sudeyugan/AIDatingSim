#pragma once
#include <string>
#include <vector>

struct GameEvent {
    std::string description;          // 事件的具体描述
    std::vector<std::string> choices; // 三个分支选项
    bool is_valid = false;            // 标记事件是否生成成功
};