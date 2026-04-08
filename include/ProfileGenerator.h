#pragma once
#include "CharacterProfile.h"
#include <future>

class ProfileGenerator {
public:
    // 采用异步方式生成，防止等待 API 返回时卡死整个 GUI 界面
    static std::future<CharacterProfile> generateRandomProfileAsync();
};