#pragma once
#include <string>

class ConfigManager {
private:
    std::string apiKey;
    std::string imageApiKey;

    // 私有化构造函数，防止外部 `new ConfigManager()`
    ConfigManager() = default; 

public:
    // 删除拷贝构造和赋值操作，确保单例的唯一性
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // 获取全局唯一实例的静态方法
    static ConfigManager& getInstance();

    // 加载配置文件
    bool loadConfig(const std::string& filePath);

    // 获取 API Key
    std::string getApiKey() const;
    std::string getImageApiKey() const;
};