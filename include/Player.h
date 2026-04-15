#pragma once
#include <string>
#include "json.hpp"

class Player {
private:
    std::string name;
    std::string backstory; //玩家的背景故事/角色设定

    // TRPG 六维属性 (0 - 100)
    int physique;   // 体格
    int intellect;  // 智识
    int charm;      // 魅力
    int wealth;     // 财力
    int empathy;    // 共情 (影响隐性感知)
    int luck;       // 运气 (影响概率判定)

public:
    Player() : physique(50), intellect(50), charm(50), wealth(50), empathy(50), luck(50) {}
    Player(std::string pName) : name(pName), physique(50), intellect(50), charm(50), wealth(50), empathy(50), luck(50) {}

    // Getter 接口
    int getPhysique() const { return physique; }
    int getIntellect() const { return intellect; }
    int getCharm() const { return charm; }
    int getWealth() const { return wealth; }
    int getEmpathy() const { return empathy; }
    int getLuck() const { return luck; }

    void setAttributes(int p, int i, int c, int w, int e, int l) {
        physique = p; intellect = i; charm = c; wealth = w; empathy = e; luck = l;
    }

    nlohmann::json toJson() const {
        return {
            {"name", name}, {"backstory", backstory},
            {"physique", physique}, {"intellect", intellect}, {"charm", charm},
            {"wealth", wealth}, {"empathy", empathy}, {"luck", luck}
        };
    }
    void fromJson(const nlohmann::json& j);
    
    // Getters
    std::string getName() const;
    std::string getBackstory() const; 

    // Setters / Modifiers
    void setBackstory(const std::string& story); 
    void updateStats(int dCharm, int dInt, int dWealth);
};