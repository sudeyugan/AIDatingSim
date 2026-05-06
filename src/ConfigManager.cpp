#include "ConfigManager.h"
#include "json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance; // C++11 起保证局部静态变量初始化的线程安全
    return instance;
}

bool ConfigManager::loadConfig(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "[Error] 无法打开配置文件: " << filePath << std::endl;
        return false;
    }

    try {
        json j;
        file >> j; // 将文件流解析为 JSON 对象
        
        if (j.contains("deepseek_api_key")) {
            apiKey = j["deepseek_api_key"];
        } 
        
        if (j.contains("image_api_key") && j["image_api_key"].is_string()) {
            imageApiKey = j["image_api_key"];
        }

        if (j.contains("proxy_host") && j["proxy_host"].is_string()) {
        proxyHost = j["proxy_host"];
        }
        
        if (j.contains("proxy_port") && j["proxy_port"].is_number()) {
            proxyPort = j["proxy_port"];
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Error] 解析 config.json 失败: " << e.what() << std::endl;
        return false;
    }
}

std::string ConfigManager::getApiKey() const {
    return apiKey;
}

std::string ConfigManager::getImageApiKey() const {
    return imageApiKey;
}