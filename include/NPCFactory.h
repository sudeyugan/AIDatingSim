#pragma once
#include "NPC.h"
#include <memory>

class NPCFactory {
public:
    // 手动指定参数生成
    static std::shared_ptr<NPC> createManualNPC(const std::string& name, const std::string& trait, const std::string& occupation);
    
    // 随机标签抽取生成
    static std::shared_ptr<NPC> createRandomNPC();
};