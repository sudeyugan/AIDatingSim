#pragma once
#include <string>
#include "json.hpp"
#include <future>

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

    std::string portraitPath;

public:
    Player();
    Player(std::string pName);
    // Getter 接口
    int getPhysique() const;
    int getIntellect() const;
    int getCharm() const;
    int getWealth() const;
    int getEmpathy() const;
    int getLuck() const;

    std::string getName() const;
    std::string getBackstory() const; 
    std::future<bool> generatePortraitAsync();

    std::string getPortraitPath() const;
    void setPortraitPath(const std::string& path);

    // Setter
    void setAttributes(int p, int i, int c, int w, int e, int l);
    void setBackstory(const std::string& story); 
    void updateStats(int dCharm, int dInt, int dWealth);

    // JSON 序列化
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};