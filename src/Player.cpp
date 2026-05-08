#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "Player.h"
#include "ConfigManager.h"
#include <httplib.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <string>

using json = nlohmann::json;

static std::string base64_decode(const std::string &in) {
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

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
            std::string prompt = 
                "A masterpiece 2D anime character portrait for a Japanese visual novel. "
                "【Art Style】: Authentic Japanese Anime Key Visual, Kyoto Animation style. Strictly use traditional 2D cel-shading (赛璐璐平涂), flat colors, and clear lineart. "
                "【Subject】: A handsome male protagonist (unless specified otherwise in the backstory), young adult anime aesthetic, male focus. "
                "【STRICT EXCLUSIONS】: ABSOLUTELY NO 3D rendering, NO 2.5D, NO semi-realistic, NO Korean webtoon style (韩漫风). NO character sheet (绝对不要角色设定集), NO multiple views (不要多角度拼图). Keep the shading minimal and flat. "
                "请根据以下背景设定与气质，精准刻画人物的神态与穿搭：【" + story + "】。"
                "【极度重要】：请确保图片背景尽量纯净、简约或大面积留白，以便于提取角色作为游戏立绘。";

            json requestBody = {
                {"model", "openai/gpt-5.4-image-2"}, 
                {"messages", json::array({{
                    {"role", "user"},
                    {"content", prompt}
                }})},
                {"modalities", json::array({"image"})} 
            };

            httplib::Client cli("https://openrouter.ai");

            std::string proxyHost = ConfigManager::getInstance().getProxyHost();
            int proxyPort = ConfigManager::getInstance().getProxyPort();
            if (!proxyHost.empty() && proxyPort > 0) {
                cli.set_proxy(proxyHost, proxyPort);
            }
            
            cli.enable_server_certificate_verification(false);
            cli.set_read_timeout(360, 0); // 生图比较慢，给足 120 秒等待

            httplib::Headers headers = { 
                {"Authorization", "Bearer " + apiKey},
                {"Content-Type", "application/json"}
            };
            // openrouter接口路径
            auto res = cli.Post("/api/v1/chat/completions", headers, requestBody.dump(), "application/json");

            if (!res || res->status != 200) {
                if (res) std::cerr << "OpenRouter生图失败: 状态码 " << res->status << " - " << res->body << std::endl;
                else std::cerr << "OpenRouter网络连接失败/超时" << std::endl;
                return false;
            }

            try {
                // ==========================================
                // 3. 解析 OpenRouter 返回的 Base64 图片数据
                // ==========================================
                json resJson = json::parse(res->body);
                
                // 按照文档结构提取 Base64 字符串
                std::string b64dataUrl = resJson["choices"][0]["message"]["images"][0]["image_url"]["url"];

                // Base64 字符串开头通常是 "data:image/png;base64,"，我们需要剥除这部分前缀
                size_t commaPos = b64dataUrl.find(',');
                if (commaPos == std::string::npos) {
                    std::cerr << "未找到 Base64 图片数据流" << std::endl;
                    return false;
                }
                std::string actualBase64 = b64dataUrl.substr(commaPos + 1);

                // ==========================================
                // 4. 解码并直接存入本地硬盘 (不再需要发起第二次下载网络请求)
                // ==========================================
                std::string binaryImageData = base64_decode(actualBase64);

                auto t = std::time(nullptr);
                struct tm tm_info;
    #ifdef _WIN32
                localtime_s(&tm_info, &t); 
    #else
                localtime_r(&t, &tm_info); 
    #endif
                char timeBuf[128];
                std::strftime(timeBuf, sizeof(timeBuf), "%Y%m%d_%H%M%S", &tm_info);
                
                std::string savePath = "saves/player_avatar_" + std::string(timeBuf) + ".png";
                
                std::ofstream file(savePath, std::ios::binary);
                if (file.is_open()) {
                    file.write(binaryImageData.data(), binaryImageData.size());
                    file.close();
                    
                    this->portraitPath = savePath; 
                    std::cout << "[System] 角色形象重构完毕，已保存至: " << savePath << std::endl;
                    return true; 
                }
            } catch (const std::exception& e) {
                std::cerr << "[Error] 解析或保存 OpenRouter 图像异常: " << e.what() << std::endl;
                return false;
            }
            return false;
        } catch (...) { 
            return false;
        }
    });
}