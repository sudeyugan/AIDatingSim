#include "Player.h"

Player::Player(std::string pName) : name(pName), charm(10), intelligence(10), wealth(10) {}

int Player::getCharm() const { return charm; }
int Player::getIntelligence() const { return intelligence; }
int Player::getWealth() const { return wealth; }

void Player::updateStats(int dCharm, int dInt, int dWealth) {
    charm += dCharm;
    intelligence += dInt;
    wealth += dWealth;
}