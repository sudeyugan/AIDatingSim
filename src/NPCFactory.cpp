#include "NPCFactory.h"
#include <vector>
#include <random>

std::shared_ptr<NPC> NPCFactory::createManualNPC(const std::string& name, const std::string& trait, const std::string& occupation) {
    std::string persona = trait + "的" + occupation;
    return std::make_shared<NPC>(name, persona);
}

std::shared_ptr<NPC> NPCFactory::createRandomNPC() {
    std::vector<std::string> names = {"Alice", "Bob", "Charlie"};
    std::vector<std::string> traits = {"傲娇", "温柔", "腹黑"};
    std::vector<std::string> occupations = {"学霸", "前台", "刺客"};

    // 简单的随机数生成
    std::random_device rd;
    std::mt19937 gen(rd());
    
    int nIdx = std::uniform_int_distribution<>(0, static_cast<int>(names.size()) - 1)(gen);
    int tIdx = std::uniform_int_distribution<>(0, static_cast<int>(traits.size()) - 1)(gen);
    int oIdx = std::uniform_int_distribution<>(0, static_cast<int>(occupations.size()) - 1)(gen);
    return createManualNPC(names[nIdx], traits[tIdx], occupations[oIdx]); 
}