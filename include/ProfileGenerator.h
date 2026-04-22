#pragma once
#include "CharacterProfile.h"
#include "GameEvent.h" // 引入事件结构体
#include "Player.h"    // 需要知道玩家是谁
#include <future>
#include <utility>
#include <string>

class ProfileGenerator {
public:

    // 生成过场旁白，返回 <旁白文本, 是否推进了宏观时间>
    static std::future<std::pair<std::string, bool>> generateTransitionSceneAsync(
        const std::string& worldSetting, 
        const std::string& recentContext, 
        const std::string& currentTimeStr
    );

    // 采用异步方式生成，防止等待 API 返回时卡死整个 GUI 界面
    static std::future<CharacterProfile> generateRandomProfileAsync();
    static std::future<std::pair<std::string, std::string>> generatePlayerProfileAsync();

    //接收世界观参数
    static std::future<CharacterProfile> generateRandomProfileAsync(const std::string& worldSetting);
    static std::future<std::pair<std::string, std::string>> generatePlayerProfileAsync(const std::string& worldSetting);
    
    //随机生成一个极具创意的世界观设定
    static std::future<std::string> generateRandomWorldSettingAsync();

    //让 GM 生成突发事件
    static std::future<GameEvent> generateRandomEventAsync(const std::string& worldSetting, const Player& player, const CharacterProfile& npc, const std::string& chatContext);

    // 生成初次相遇的场景
    static std::future<std::string> generateEncounterAsync(const std::string& worldSetting, const Player& player, const CharacterProfile& npc);
    
    // 生成背景图
    static std::future<std::string> generateBackgroundAsync(const std::string& sceneDesc);

};
