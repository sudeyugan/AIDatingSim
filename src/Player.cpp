#include "Player.h"

// 默认构造函数：所有属性设为 50
Player::Player() : 
    name("未知"), backstory("一个刚刚来到这座城市的普通旅人，似乎忘记了过去的记忆。"),
    physique(50), intellect(50), charm(50), wealth(50), empathy(50), luck(50) {}

// 带名字的构造函数
Player::Player(std::string pName) : 
    name(pName), backstory("一个刚刚来到这座城市的普通旅人，似乎忘记了过去的记忆。"),
    physique(50), intellect(50), charm(50), wealth(50), empathy(50), luck(50) {}

// Getters
std::string Player::getName() const { return name; }
std::string Player::getBackstory() const { return backstory; }
int Player::getPhysique() const { return physique; }
int Player::getIntellect() const { return intellect; } // 修正为 intellect
int Player::getCharm() const { return charm; }
int Player::getWealth() const { return wealth; }
int Player::getEmpathy() const { return empathy; }
int Player::getLuck() const { return luck; }

// Setters
void Player::setBackstory(const std::string& story) { backstory = story; }

void Player::setAttributes(int p, int i, int c, int w, int e, int l) {
    physique = p; intellect = i; charm = c; wealth = w; empathy = e; luck = l;
}

// 建议后续可拓展更新其他属性，这里保留原接口并修正名字
void Player::updateStats(int dCharm, int dInt, int dWealth) {
    charm += dCharm;
    intellect += dInt; // 修正为 intellect
    wealth += dWealth;
}

// JSON 存档：包含所有 6 维属性
nlohmann::json Player::toJson() const {
    return {
        {"name", name},
        {"backstory", backstory},
        {"physique", physique},
        {"intellect", intellect},
        {"charm", charm},
        {"wealth", wealth},
        {"empathy", empathy},
        {"luck", luck}
    };
}

// JSON 读档：读取所有 6 维属性，带默认值防崩溃
void Player::fromJson(const nlohmann::json& j) {
    name = j.value("name", "主角");
    backstory = j.value("backstory", "一个刚刚来到这座城市的普通旅人，似乎忘记了过去的记忆。");
    physique = j.value("physique", 50);
    intellect = j.value("intellect", 50); 
    charm = j.value("charm", 50);
    wealth = j.value("wealth", 50);
    empathy = j.value("empathy", 50);
    luck = j.value("luck", 50);
}