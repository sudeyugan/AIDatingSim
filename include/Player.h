#pragma once
#include <string>

class Player {
private:
    std::string name;
    std::string backstory; //玩家的背景故事/角色设定
    int charm;        
    int intelligence; 
    int wealth;       

public:
    Player(std::string pName);
    
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