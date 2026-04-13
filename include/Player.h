#pragma once
#include <string>
#include "json.hpp"

class Player {
private:
    std::string name;
    std::string backstory; //玩家的背景故事/角色设定
    int charm;        
    int intelligence; 
    int wealth;       

public:
    Player(std::string pName);

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
    
    // Getters
    std::string getName() const;
    std::string getBackstory() const; 
    int getCharm() const;
    int getIntelligence() const;
    int getWealth() const;

    // Setters / Modifiers
    void setBackstory(const std::string& story); 
    void updateStats(int dCharm, int dInt, int dWealth);
};