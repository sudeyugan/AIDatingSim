#pragma once
#include <string>

class Player {
private:
    std::string name;
    int charm;        // 魅力
    int intelligence; // 才智
    int wealth;       // 财富

public:
    Player(std::string pName);
    
    // Getters
    std::string getName() const;
    int getCharm() const;
    int getIntelligence() const;
    int getWealth() const;

    // Setters / Modifiers
    void updateStats(int dCharm, int dInt, int dWealth);
};