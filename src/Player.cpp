#include "Player.h"

Player::Player(std::string pName) : name(pName), backstory("一个刚刚来到这座城市的普通旅人，似乎忘记了过去的记忆。"), charm(10), intelligence(10), wealth(10) {}

std::string Player::getName() const { return name; }
int Player::getCharm() const { return charm; }
int Player::getIntelligence() const { return intelligence; }
int Player::getWealth() const { return wealth; }
std::string Player::getBackstory() const { return backstory; }

void Player::setBackstory(const std::string& story) { backstory = story; }

void Player::updateStats(int dCharm, int dInt, int dWealth) {
    charm += dCharm;
    intelligence += dInt;
    wealth += dWealth;
}

nlohmann::json Player::toJson() const {
    return {
        {"name", name},
        {"backstory", backstory},
        {"charm", charm},
        {"intelligence", intelligence},
        {"wealth", wealth}
    };
}

void Player::fromJson(const nlohmann::json& j) {
    name = j.value("name", "Unknown");
    backstory = j.value("backstory", "");
    charm = j.value("charm", 0);
    intelligence = j.value("intelligence", 0);
    wealth = j.value("wealth", 0);
}