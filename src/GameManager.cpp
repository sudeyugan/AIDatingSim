#define _CRT_SECURE_NO_WARNINGS
#include "GameManager.h"
#include "NPCFactory.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include "json.hpp"
#include "imgui.h"
#include <algorithm>
#include "imgui_impl_glfw.h"     
#include "imgui_impl_opengl3.h"  
#include <GLFW/glfw3.h>          
#include <filesystem>
#include "ProfileGenerator.h"

namespace fs = std::filesystem;

GameManager::GameManager() : mainPlayer("主角"), isRunning(true) {
    strcpy(worldSettingBuf, "现代日常都市");
}

void GameManager::initGame() {
    std::cout << "[系统] 正在初始化游戏大管家..." << std::endl;

    // 1. 扫描本地存档目录，获取最新存档列表
    scanSaveFiles();

    // 2. 尝试自动加载最近修改的存档 (提升玩家体验)
    if (!availableSaves.empty()) {
        std::string latestSavePath = "";
        auto latestTime = std::filesystem::file_time_type::min();

        // 遍历找到最后修改的那一个文件
        for (const auto& saveName : availableSaves) {
            std::string fullPath = "saves/" + saveName;
            try {
                auto writeTime = std::filesystem::last_write_time(fullPath);
                if (writeTime > latestTime) {
                    latestTime = writeTime;
                    latestSavePath = fullPath;
                }
            } catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "检查存档时间出错: " << e.what() << '\n';
            }
        }

        // 如果找到了最新的存档，尝试加载它
        if (!latestSavePath.empty()) {
            std::cout << "[系统] 发现历史存档，正在自动加载: " << latestSavePath << std::endl;
            bool success = loadGame(latestSavePath);
            
            if (success) {
                std::cout << "[系统] 自动读档成功！欢迎回来。" << std::endl;
                // 增加一条 UI 提示，让玩家知道是读档进来的
                uiChatHistory.push_back({"系统", "（已自动为你加载最近的命运线）"});
                return; // 读档成功后直接结束初始化，不要执行后面的默认代码
            } else {
                std::cerr << "[警告] 自动读档失败，将以全新状态启动。" << std::endl;
            }
        }
    }

    // 3. 兜底逻辑：如果没有存档，或者读档失败，则初始化为“全新游戏”状态
    std::cout << "[系统] 未发现可用存档，以全新状态启动游戏。" << std::endl;
    
    // 确保 UI 聊天记录是干净的，并给出极其清晰的新手引导
    uiChatHistory.clear();
    uiChatHistory.push_back({"系统", "欢迎来到跨次元恋爱模拟系统 v2.0。"});
    uiChatHistory.push_back({"系统", "当前世界一片空白。请在左侧设定世界观，并点击下方的【邂逅新角色】开始你的故事。"});
    uiChatHistory.push_back({"系统", "如果想回到过去的记忆，请点击顶部菜单栏的 System -> Load Game。"});

    // 重置世界观输入框为默认状态
    strcpy(worldSettingBuf, "现代日常都市");
    
    // 确保标志位处于安全状态
    isGeneratingNPC = false;
    isWaitingForReply = false;
    isEventActive = false;
}



void GameManager::checkAsyncTasks() {
    // 检查世界观生成是否完成
    if (isWaitingForReply && futureReply.valid()) {
        if (futureReply.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            NPCResponse response = futureReply.get(); // 【修改】获取结构体
            
            uiChatHistory.push_back({activeNPC->getName(), response.reply}); 
            isWaitingForReply = false;
            
            // ：NPC 觉得是时候推进剧情了！
            if (response.trigger_event && !isEventActive) {
                isGeneratingEvent = true;
                
                std::string recentContext = "";
                int startIdx = std::max(0, (int)uiChatHistory.size() - 6);
                for (int i = startIdx; i < uiChatHistory.size(); ++i) {
                    recentContext += uiChatHistory[i].first + ": " + uiChatHistory[i].second + "\n";
                }
                
                // 让 GM 根据上下文降下事件
                futureEvent = ProfileGenerator::generateRandomEventAsync(std::string(worldSettingBuf), mainPlayer, currentNPC, recentContext);
            }
        }
    }

    // 检查角色生成是否完成
    if (isGeneratingNPC && futureProfile.valid()) {
        if (futureProfile.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            currentNPC = futureProfile.get(); 
            isGeneratingNPC = false;          
            
            std::string complexPersona = "外貌：" + currentNPC.appearance + "。核心性格：" + currentNPC.personality_core + "。隐藏创伤/执念：" + currentNPC.hidden_trauma;
            activeNPC = std::make_unique<NPC>(currentNPC.name, complexPersona);
            
            npcImageLoader.Free(); // 清空上一任 NPC 的立绘
            isGeneratingPortrait = true;
            futurePortrait = activeNPC->generatePortraitAsync();

            hasEncounterStarted = false; 
            uiChatHistory.clear();
            uiChatHistory.push_back({"系统", currentNPC.name + " 的灵魂已在当前世界降临。"});
            uiChatHistory.push_back({"系统", "请在准备好后，点击下方的“开始邂逅”按钮。"});
                    
        }
    }

    // 检查画师（图片下载）是否完工
    if (isGeneratingPortrait && futurePortrait.valid()) {
        if (futurePortrait.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            bool success = futurePortrait.get();
            isGeneratingPortrait = false;
            
            if (success && activeNPC != nullptr) {
                // 读取刚刚下载到硬盘的图片，转换成 OpenGL 纹理
                npcImageLoader.LoadFromFile(activeNPC->getPortraitPath());
            }
        }
    }

    // 检查玩家“重置人生”是否完成
    if (isGeneratingPlayer && futurePlayerProfile.valid()) {
        if (futurePlayerProfile.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto newProfile = futurePlayerProfile.get(); 
            mainPlayer = Player(newProfile.first); 
            mainPlayer.setBackstory(newProfile.second);
            isGeneratingPlayer = false;          
            
            // 保留 NPC！不销毁 activeNPC！
            uiChatHistory.clear();
            hasEncounterStarted = false;           

            uiChatHistory.push_back({"系统", "命运的齿轮转动，你以新身份降临。"});

        }
    }

    // 3. 检查相遇场景是否生成完毕
    if (isGeneratingEncounter && futureEncounter.valid()) {
        if (futureEncounter.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            std::string encounterScene = futureEncounter.get();
            isGeneratingEncounter = false;
            hasEncounterStarted = true;
            // 将绝美的开场白上屏，现在玩家可以开始搭话了！
            uiChatHistory.push_back({"【GM 场景导入】", encounterScene});
        
            if (activeNPC != nullptr) {
                activeNPC->injectSceneMemory(encounterScene);
            }
        }
    }

    // 检查事件是否生成完毕
    if (isGeneratingEvent && futureEvent.valid()) {
        if (futureEvent.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            currentEvent = futureEvent.get();
            isGeneratingEvent = false;
            
            if (currentEvent.is_valid) {
                isEventActive = true; // 进入事件模式
                // 将事件描述上屏
                uiChatHistory.push_back({"【GM 突发事件】", currentEvent.description});
                
                // 滚动条自动到底部（一个小优化，放在后续循环也会生效）
                ImGui::SetScrollHereY(1.0f);
            }
        }
    }
}

void GameManager::renderUI() {
    // 设定全屏窗口
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    
    ImGui::Begin("MainGameWindow", nullptr, window_flags);

    // 提前计算底部交互区需要的高度 (比如 3 倍的常规行高)
    float bottom_height = ImGui::GetFrameHeightWithSpacing() * 3.0f;

    // ----------------------------------------------------
    // 区域 A：左侧立绘区 (占据 60% 宽度)
    // ----------------------------------------------------
    float left_width = ImGui::GetContentRegionAvail().x * 0.6f; 
    ImGui::BeginChild("PortraitArea", ImVec2(left_width, ImGui::GetContentRegionAvail().y - bottom_height), true);
    
    if (isGeneratingPortrait) {
        // 画图是很慢的（通常要 5~15 秒），给玩家一个有氛围的提示
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2.0f);
        ImGui::TextDisabled("... AI 画师正在跨越次元描绘 %s 的容貌 ...", currentNPC.name.c_str());
    } 
    else if (npcImageLoader.isLoaded()) {
        // 【图片渲染魔法】！
        // 获取窗口的可用大小
        ImVec2 availSize = ImGui::GetContentRegionAvail();
        
        // 计算等比例缩放，让图片完美适应左侧窗口
        float aspect = (float)npcImageLoader.getWidth() / (float)npcImageLoader.getHeight();
        ImVec2 imageSize;
        if (availSize.x / aspect <= availSize.y) {
            imageSize = ImVec2(availSize.x, availSize.x / aspect);
        } else {
            imageSize = ImVec2(availSize.y * aspect, availSize.y);
        }

        // 居中显示图片
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - imageSize.x) * 0.5f);
        ImGui::SetCursorPosY((ImGui::GetWindowHeight() - imageSize.y) * 0.5f);
        
        // 将 OpenGL 的纹理 ID 喂给 ImGui 画出来
        ImGui::Image((void*)(intptr_t)npcImageLoader.getTextureID(), imageSize);
    } 
    else {
        ImGui::Text("暂无立绘数据");
    }

    ImGui::EndChild();

    ImGui::SameLine(); // 核心：让下一个窗口和上一个窗口并在同一行

    // ----------------------------------------------------
    // 区域 B：右侧信息与对话区 (占据剩余 40% 宽度)
    // ----------------------------------------------------
    ImGui::BeginChild("InfoArea", ImVec2(0, ImGui::GetContentRegionAvail().y - bottom_height), true);
    
    // 世界观设定 UI
    ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.8f, 1.0f), "【当前世界观】");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100);
    ImGui::InputText("##WorldSetting", worldSettingBuf, IM_ARRAYSIZE(worldSettingBuf));
    ImGui::SameLine();
    
    if (isGeneratingWorld) {
        ImGui::BeginDisabled();
        ImGui::Button("构思中...", ImVec2(80, 0));
        ImGui::EndDisabled();
    } else {
        if (ImGui::Button("随机天意", ImVec2(80, 0))) {
            isGeneratingWorld = true;
            futureWorldSetting = ProfileGenerator::generateRandomWorldSettingAsync();
        }
    }
    ImGui::Spacing();
    ImGui::Separator();

    // 玩家与时间状态面板
    ImGui::TextColored(ImVec4(0.3f, 0.7f, 0.9f, 1.0f), "--- 【%s】 的状态 ---", mainPlayer.getName().c_str());
    if (isGeneratingPlayer) {
        ImGui::Text("正在重塑灵魂...");
    } else {
        ImGui::TextWrapped("【身世】: %s", mainPlayer.getBackstory().c_str());
    }
    ImGui::Text("魅力: %d  才智: %d  财富: %d", mainPlayer.getCharm(), mainPlayer.getIntelligence(), mainPlayer.getWealth());
    ImGui::Text("当前时间: 早上");
    ImGui::Separator();
    
    // NPC 状态面板
    ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.6f, 1.0f), "--- 邂逅的角色 ---");
    if (isGeneratingNPC) {
        // 加载中的提示
        ImGui::Text("正在呼唤跨越次元的灵魂，请稍候...");
    } else if (currentNPC.is_generated) {
        // 档案渲染
        ImGui::Text("姓名: %s", currentNPC.name.c_str());
        ImGui::Text("初始好感: %d (%s)", currentNPC.initial_affection, currentNPC.initial_attitude.c_str());
        ImGui::Spacing();
        ImGui::TextWrapped("【外貌】: %s", currentNPC.appearance.c_str());
        ImGui::Spacing();
        ImGui::TextWrapped("【性格】: %s", currentNPC.personality_core.c_str());
        
        // 隐藏的心理创伤（作为剧本钩子，前期可以使用暗色字体提示）
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.5f, 0.5f, 1.0f));
        ImGui::TextWrapped("【未知的执念/秘密】: %s", currentNPC.hidden_trauma.c_str());
        ImGui::PopStyleColor();
    } else {
        ImGui::Text("暂无角色。请点击下方按钮邂逅新的缘分。");
    }
    ImGui::Separator();

    // 聊天记录区 (独立子窗口，支持滚动)
    ImGui::Text("【对话记录】");
    ImGui::BeginChild("ChatHistory", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    //动态渲染真实聊天记录
    for (const auto& chat : uiChatHistory) {
        ImVec4 textColor;
        if (chat.first == "系统" || chat.first == "【GM 场景导入】" || chat.first == "【GM 突发事件】") {
            textColor = ImVec4(0.6f, 0.6f, 0.6f, 1.0f); // 灰色
        } else if (chat.first == mainPlayer.getName()) {
            textColor = ImVec4(0.5f, 0.7f, 0.9f, 1.0f); // 蓝色
        } else {
            textColor = ImVec4(0.9f, 0.5f, 0.6f, 1.0f); // 粉色
        }
        
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::TextWrapped("%s: %s", chat.first.c_str(), chat.second.c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    if (isWaitingForReply) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s 正在思考...", activeNPC->getName().c_str());
    }
    
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::EndChild();

    // ----------------------------------------------------
    // 区域 C：底部交互与输入区
    // ----------------------------------------------------
    ImGui::Separator();
    ImGui::BeginChild("ActionArea", ImVec2(0, 0), false);
    
    // 顶部控制台：生成与重置按钮
    if (isGeneratingNPC) {
        ImGui::BeginDisabled(); ImGui::Button("正在生成...", ImVec2(100, 0)); ImGui::EndDisabled();
    } else {
        if (ImGui::Button("邂逅新角色", ImVec2(100, 0))) {
            isGeneratingNPC = true;
            futureProfile = ProfileGenerator::generateRandomProfileAsync(std::string(worldSettingBuf));
        }
    }
    ImGui::SameLine();
    if (isGeneratingPlayer) {
        ImGui::BeginDisabled(); ImGui::Button("重塑中...", ImVec2(100, 0)); ImGui::EndDisabled();
    } else {
        if (ImGui::Button("重置人生", ImVec2(100, 0))) {
            isGeneratingPlayer = true;
            futurePlayerProfile = ProfileGenerator::generatePlayerProfileAsync(std::string(worldSettingBuf));
        }
    }
    ImGui::SameLine();

    if (activeNPC != nullptr && !hasEncounterStarted) {
        if (isGeneratingEncounter) {
            ImGui::BeginDisabled(); ImGui::Button("正在靠近...", ImVec2(100, 0)); ImGui::EndDisabled();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.96f, 0.54f, 0.60f, 1.0f)); 
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.98f, 0.64f, 0.70f, 1.0f)); 
            
            if (ImGui::Button("开始邂逅", ImVec2(120, 0))) {
                isGeneratingEncounter = true;
                // 呼叫 GM 生成导入幕
                futureEncounter = ProfileGenerator::generateEncounterAsync(std::string(worldSettingBuf), mainPlayer, currentNPC);
            }
            
            ImGui::PopStyleColor(2);
        }
    }
    ImGui::Spacing();

    // 常规聊天模式 VS 事件选择模式
    if (isEventActive) {
        // ==== 事件选择模式 ====
        ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "命运的岔路口（请做出选择）：");
        
        for (size_t i = 0; i < currentEvent.choices.size(); ++i) {
            // 将选项渲染为宽度填满的按钮
            if (ImGui::Button(currentEvent.choices[i].c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                // 当玩家点击选项时：
                std::string chosenAction = currentEvent.choices[i];
                
                // 1. 玩家的话上屏
                uiChatHistory.push_back({mainPlayer.getName(), "【行动】: " + chosenAction});
                
                // 2. 退出事件模式
                isEventActive = false;
                
                // 3. 将玩家的行动发给 NPC 扮演的大脑，让 NPC 对你的选择做出反应！
                isWaitingForReply = true;
                futureReply = std::async(std::launch::async, [this, chosenAction]() {
                    std::string contextualInput = "（采取行动：" + chosenAction + "）";
                    return activeNPC->interact(contextualInput, mainPlayer);
                });
            }
        }
    }else {
        // ==== 常规对话模式 ====
        static char inputBuf[512] = ""; 
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100); 
        bool isEnterPressed = ImGui::InputText("##ChatInput", inputBuf, IM_ARRAYSIZE(inputBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        
        ImGui::SameLine();
        
        //提前锁定本帧的禁用状态
        bool disableInput = (isWaitingForReply || activeNPC == nullptr || isGeneratingEncounter || !hasEncounterStarted);
        if (disableInput) ImGui::BeginDisabled();
        
        if (ImGui::Button("发送", ImVec2(80, 0)) || (isEnterPressed && !disableInput)) {
            if (strlen(inputBuf) > 0) {
                std::string userText = inputBuf;
                uiChatHistory.push_back({mainPlayer.getName(), userText}); 
                inputBuf[0] = '\0'; 
                
                isWaitingForReply = true; // 改变状态，但不会影响下面的 EndDisabled
                futureReply = std::async(std::launch::async, [this, userText]() {
                    // 通过 this 指针隐式访问成员变量
                    return activeNPC->interact(userText, mainPlayer);
                });
                
                ImGui::SetKeyboardFocusHere(-1); 
            }
        }
        
        if (disableInput) ImGui::EndDisabled(); // 使用锁定的状态变量，完美匹配！
        
        // GM 状态提示
        if (isGeneratingEncounter) {
            ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "GM 正在布置相遇的场景...");
        } else if (isGeneratingEvent) {
            ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "GM 正在暗中改变命运的走向...");
        }
    }
    
    ImGui::EndChild();

    ImGui::End();
}

void GameManager::runLoop() {
    checkAsyncTasks(); // 1. 先让 AI 大脑处理逻辑并更新状态
    renderUI();        // 2. 根据最新的状态画出 UI
}

void GameManager::startAITask(std::shared_ptr<NPC> npc, std::future<bool> task) {
    if (isAIBusy) return; // 如果已经在忙了，拒绝新任务（防抖）
    
    isAIBusy = true;
    interactingNPC = npc;
    currentAITask = std::move(task); // 接管 future 对象
}

void GameManager::scanSaveFiles() {
    availableSaves.clear();
    const std::string saveDir = "saves"; // 在游戏根目录下的 saves 文件夹

    // 如果文件夹不存在，就自动创建一个
    if (!fs::exists(saveDir)) {
        fs::create_directory(saveDir);
        return;
    }

    // 遍历文件夹，提取所有的 .json 存档文件
    for (const auto& entry : fs::directory_iterator(saveDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            // 保存文件名，例如 "save_01.json"
            availableSaves.push_back(entry.path().filename().string());
        }
    }
}

void GameManager::advanceTime() {
    if (currentTime == TimeOfDay::MORNING) { currentTime = TimeOfDay::NOON; std::cout << "【时间流逝：到了中午】\n"; }
    else if (currentTime == TimeOfDay::NOON) { currentTime = TimeOfDay::NIGHT; std::cout << "【时间流逝：到了晚上】\n"; }
    else { currentTime = TimeOfDay::MORNING; std::cout << "【时间流逝：新的一天开始了】\n"; }
}

bool GameManager::saveGame(const std::string& filename) {
    nlohmann::json saveData;
    
    // 1. 存储游戏元数据
    saveData["currentTime"] = static_cast<int>(currentTime);
    saveData["isRunning"] = isRunning;

    // 2. 存储玩家数据
    if (player) {
        saveData["player"] = player->toJson();
    }

    // 3. 存储所有 NPC 数据
    nlohmann::json npcsJson = nlohmann::json::array();
    for (const auto& npc : targetNPCs) {
        npcsJson.push_back(npc->toJson());
    }
    saveData["npcs"] = npcsJson;

    // 4. 写入文件
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open save file for writing!" << std::endl;
        return false;
    }
    file << saveData.dump(4); // 格式化输出，缩进4个空格，方便你用 VS Code 直接调试查看
    file.close();
    
    return true;
}

bool GameManager::loadGame(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Save file not found!" << std::endl;
        return false;
    }

    nlohmann::json saveData;
    try {
        file >> saveData;
    } catch (nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return false;
    }

    // 1. 恢复元数据
    currentTime = static_cast<TimeOfDay>(saveData.value("currentTime", 0));
    isRunning = saveData.value("isRunning", true);

    // 2. 恢复玩家
    if (saveData.contains("player")) {
        player = std::make_unique<Player>("Temp"); // 先占位
        player->fromJson(saveData["player"]);
    }

    // 3. 恢复 NPC 列表
    targetNPCs.clear();
    if (saveData.contains("npcs")) {
        for (const auto& npcJson : saveData["npcs"]) {
            // 实例化一个临时的 NPC
            auto newNPC = std::make_shared<NPC>("Temp", "Temp");
            
            // 从 JSON 中恢复数据（包含了 name, affection, chatHistory 以及 portraitPath）
            newNPC->fromJson(npcJson);
            
            // 此时 portraitPath 已经从存档中读取出来了
            // 调用 reloadTexture() 重新生成 OpenGL 纹理 ID！
            newNPC->reloadTexture(); 
            
            // 将恢复好的 NPC 塞入游戏管理器的列表中
            targetNPCs.push_back(newNPC);
        }
    }

    return true;
}