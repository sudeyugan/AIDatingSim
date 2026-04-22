#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "Player.h"
#include "ConfigManager.h"
#include <httplib.h>
#include <iostream>
#include <fstream>
#include <algorithm>
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

std::string Player::getPortraitPath() const { return portraitPath; }
void Player::setPortraitPath(const std::string& path) { portraitPath = path; }

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
        {"luck", luck},
        {"portraitPath", portraitPath}
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
    portraitPath = j.value("portraitPath", "");
}

std::future<bool> Player::generatePortraitAsync() {
    // 拷贝一份背景故事，防止多线程访问冲突
    std::string story = this->backstory; 
    
    return std::async(std::launch::async, [this, story]() -> bool {
        try {
            std::string apiKey = ConfigManager::getInstance().getImageApiKey();
            apiKey.erase(std::remove(apiKey.begin(), apiKey.end(), '\r'), apiKey.end());
            apiKey.erase(std::remove(apiKey.begin(), apiKey.end(), '\n'), apiKey.end());
            apiKey.erase(apiKey.find_last_not_of(" ") + 1);

            // 构造生图提示词，加入视觉小说风格限制和防和谐安全词
            std::string prompt = "【画面中心仅限单人】高质量视觉小说CG，主角单人半身肖像立绘。二次元精美动漫平涂画风。人物背景设定与气质：" + story + "。如果未提及性别，请画成俊朗的青年。强制要求：写实头身比，面部轮廓清晰，绝对不要拼图或多视图设定集。";

            nlohmann::json requestBody = {
                {"model", "cogview-4-250304"},
                {"prompt", prompt}
            };

            httplib::Client cli("https://open.bigmodel.cn");
            cli.set_read_timeout(60, 0); 
            
            httplib::Headers headers = {
                {"Authorization", "Bearer " + apiKey},
                {"Content-Type", "application/json"}
            };

            auto res = cli.Post("/api/paas/v4/images/generations", headers, requestBody.dump(), "application/json");

            if (res && res->status == 200) {
                nlohmann::json resJson = nlohmann::json::parse(res->body);
                std::string imageUrl = resJson["data"][0]["url"];

                // 解析 URL 并下载图片
                size_t protocolEnd = imageUrl.find("://");
                if (protocolEnd != std::string::npos) {
                    std::string hostPath = imageUrl.substr(protocolEnd + 3);
                    size_t pathStart = hostPath.find('/');
                    std::string host = hostPath.substr(0, pathStart);
                    std::string path = hostPath.substr(pathStart);

                    httplib::Client dlCli("https://" + host);
                    dlCli.set_read_timeout(60, 0);
                    auto imgRes = dlCli.Get(path);

                    if (imgRes && imgRes->status == 200) {
                        std::filesystem::create_directory("saves");
                        

                        // 获取当前时间生成时间戳
                        auto t = std::time(nullptr);
                        struct tm tm_info;
                        localtime_s(&tm_info, &t);
                        char timeBuf[128];
                        std::strftime(timeBuf, sizeof(timeBuf), "%Y%m%d_%H%M%S", &tm_info);
                        
                        // 加上时间戳后缀保留历史头像
                        std::string savePath = "saves/player_avatar_" + std::string(timeBuf) + ".png"; 
                        
                        std::ofstream out(savePath, std::ios::binary);
                        out.write(imgRes->body.c_str(), imgRes->body.size());
                        out.close();
                        
                        // 下载成功，写入类属性
                        this->portraitPath = savePath;
                        return true; 
                    }
                }
            } else {
                std::cerr << "主角生图失败，状态码: " << (res ? std::to_string(res->status) : "超时") << std::endl;
            }
        } catch (...) {
            std::cerr << "主角生图发生异常！" << std::endl;
        }
        return false; 
    });
}