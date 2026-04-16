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

void GameManager::advanceTime() {
    // 根据当前时间推进到下一个时间段，并向 UI 聊天记录中插入系统旁白
    if (currentTime == TimeOfDay::MORNING) { 
        currentTime = TimeOfDay::NOON; 
    }
    else if (currentTime == TimeOfDay::NOON) { 
        currentTime = TimeOfDay::NIGHT; 
    }
    else { 
        currentTime = TimeOfDay::MORNING; 
    }
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

void GameManager::startNewGame() {
    std::cout << "[系统] 正在清理世界线，准备开启全新游戏..." << std::endl;

    // 1. 清空聊天记录，写入初始新手引导
    uiChatHistory.clear();
    uiChatHistory.push_back({"系统", "欢迎来到跨次元恋爱模拟系统 v2.0。"});
    uiChatHistory.push_back({"系统", "当前世界一片空白。请在左侧设定世界观，并点击下方的【邂逅新角色】开始你的故事。"});
    
    // 2. 恢复默认世界观
    strcpy(worldSettingBuf, "现代日常都市");
    
    // 3. 重置时间与回合
    currentTime = TimeOfDay::MORNING;
    chatTurns = 0;
    
    // 4. 清除 NPC 与立绘
    activeNPC = nullptr;
    currentNPC = CharacterProfile(); 
    npcImageLoader.Free(); // 清理显卡中的图片
    
    // 5. 重置主角状态 (你可以根据 Player 类的构造函数自行调整)
    mainPlayer = Player("主角"); 
    
    // 6. 重置所有异步状态机标志位
    hasEncounterStarted = false;
    isGeneratingNPC = false;
    isWaitingForReply = false;
    isEventActive = false;
}

void GameManager::checkAsyncTasks() {
    // 检查世界观生成是否完成
    if (isGeneratingWorld && futureWorldSetting.valid()) {
        if (futureWorldSetting.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            std::string newWorld = futureWorldSetting.get();
            isGeneratingWorld = false;
            
            // 将新生成的世界观安全地拷贝到 UI 输入框的缓存中
            strncpy(worldSettingBuf, newWorld.c_str(), sizeof(worldSettingBuf) - 1);
            worldSettingBuf[sizeof(worldSettingBuf) - 1] = '\0';
            
            uiChatHistory.push_back({"系统", "（世界线已重构：" + newWorld + "）"});
            ImGui::SetScrollHereY(1.0f);
        }
    }

    if (isWaitingForReply && futureReply.valid()) {
        if (futureReply.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            NPCResponse response = futureReply.get(); 
            
            // 1. NPC 的话上屏
            uiChatHistory.push_back({activeNPC->getName(), response.reply}); 
            for (const auto& insight : response.passive_insights) {
                uiChatHistory.push_back({"[暗中洞察]", insight}); 
            }
            
            isWaitingForReply = false;
            
            chatTurns++; // 增加回合数
            // 1. 【优先】判定是否触发命运事件（高潮打断）
            if (response.trigger_event && !isEventActive) {
                isGeneratingEvent = true;
                std::string recentContext = "";
                int startIdx = std::max(0, (int)uiChatHistory.size() - 6);
                for (int i = startIdx; i < uiChatHistory.size(); ++i) {
                    recentContext += uiChatHistory[i].first + ": " + uiChatHistory[i].second + "\n";
                }
                futureEvent = ProfileGenerator::generateRandomEventAsync(std::string(worldSettingBuf), mainPlayer, currentNPC, recentContext);
            }
            // 2. 【其次】如果没有任何突发事件，再去平淡地判定是否该转场/回家了
            else if (response.ready_to_transition && !isGeneratingTransition) {
                isGeneratingTransition = true;
                
                // 提取最近 4 句话给 GM 作为判断依据
                std::string recentContext = "";
                int startIdx = std::max(0, (int)uiChatHistory.size() - 4);
                for (int i = startIdx; i < uiChatHistory.size(); ++i) {
                    recentContext += uiChatHistory[i].first + ": " + uiChatHistory[i].second + "\n";
                }
                
                std::string timeCtx = (currentTime == TimeOfDay::MORNING) ? "清晨" : 
                                      (currentTime == TimeOfDay::NOON) ? "午后" : "深夜";
                                          
                uiChatHistory.push_back({"系统", "（镜头流转，场景重构中...）"});
                futureTransition = ProfileGenerator::generateTransitionSceneAsync(std::string(worldSettingBuf), recentContext, timeCtx);
                ImGui::SetScrollHereY(1.0f);
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

    // 检查时间/场景过渡是否生成完毕
    if (isGeneratingTransition && futureTransition.valid()) {
        if (futureTransition.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto transitionResult = futureTransition.get();
            std::string transitionScene = transitionResult.first;
            bool isTimeAdvanced = transitionResult.second;
            
            isGeneratingTransition = false;
            
            // 只有当大模型认定“时间到了下一个阶段”时，才调用原有的宏观推进逻辑
            if (isTimeAdvanced) {
                advanceTime(); // 这里面会处理 currentTime 的流逝逻辑
            }
            
            // 重置聊天回合疲劳值
            chatTurns = 0; 
            
            // 绝美过场旁白上屏
            uiChatHistory.push_back({"【GM 场景重构】", transitionScene});
            ImGui::SetScrollHereY(1.0f);
            
            // 把新场景直接塞进 NPC 的上下文中，让她知道已经换图了
            if (activeNPC != nullptr) {
                activeNPC->injectSceneMemory(transitionScene);
                
                // 转场完毕后，立刻进入等待 NPC 回复的状态，不给玩家冷场的机会
                isWaitingForReply = true; 
                
                std::string timeCtx = (currentTime == TimeOfDay::MORNING) ? "清晨" : 
                                      (currentTime == TimeOfDay::NOON) ? "午后" : "深夜";
                
                // 构造一条专用的隐藏指令，逼迫大模型根据新场景主动开口
                std::string hiddenInput = "（系统提示：场景与时间刚刚发生了推移，当前时间是" + timeCtx + "。请根据最新的场景旁白，自然且主动地向玩家开口，抛出一个新话题或对环境做出感叹，打破沉默。）";
                
                // 开启异步任务，让 NPC 思考新场景的第一句话
                futureReply = std::async(std::launch::async, [activeNPC = this->activeNPC, playerCopy = this->mainPlayer, hiddenInput]() {
                    return activeNPC->interact(hiddenInput, playerCopy);
                });
                // ====================================================================
            }
        }
    }
}

void GameManager::renderUI() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("系统 (System)")) {
            if (ImGui::MenuItem("开启全新命运 (New Game)")) {
                startNewGame();
            }
            if (ImGui::MenuItem("读取命运线 (Load Game)")) {
                showLoadMenu = true;
                scanSaveFiles(); // 每次打开时刷新本地存档列表
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // 2. 现代化的独立记忆面板
    if (showLoadMenu) {
        // 让窗口更大一点，比例更协调
        ImGui::SetNextWindowSize(ImVec2(500, 480), ImGuiCond_FirstUseEver);
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f - 250, 
                                       viewport->WorkPos.y + viewport->WorkSize.y * 0.5f - 240), 
                                ImGuiCond_FirstUseEver);

        if (ImGui::Begin("读取命运线 (Load Game)", &showLoadMenu, ImGuiWindowFlags_NoCollapse)) {
            ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.6f, 1.0f), "—— 沉睡在时间缝隙中的记忆碎片 ——");
            ImGui::Separator();
            ImGui::Spacing();

            if (parsedSaves.empty()) {
                ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2.0f);
                ImGui::TextDisabled("当前世界线一片空白，没有任何命运的记录。");
            } else {
                // 开启一个支持滑动的内部区域
                ImGui::BeginChild("SaveCardsRegion", ImVec2(0, 0), false, ImGuiWindowFlags_None);
                
                for (int i = 0; i < parsedSaves.size(); ++i) {
                    const auto& save = parsedSaves[i];
                    
                    ImGui::PushID(i); // 极其重要，防止按钮冲突
                    
                    // 【卡片底板样式】：纯白底色 + 大圆角边框
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 0.9f));
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
                    ImGui::BeginChild("Card", ImVec2(0, 85), true, ImGuiWindowFlags_NoScrollbar);

                    // ============ 卡片内部排版 ============
                    // 第一行：角色名与时间
                    ImGui::SetCursorPos(ImVec2(15, 15));
                    ImGui::TextColored(ImVec4(0.85f, 0.45f, 0.55f, 1.0f), "邂逅对象: %s", save.npcName.c_str());
                    ImGui::SameLine(220); // 固定列宽，保证对齐
                    ImGui::TextDisabled("时间: %s", save.timeStr.c_str());

                    // 第二行：羁绊进度条
                    ImGui::SetCursorPos(ImVec2(15, 45));
                    ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "羁绊值:");
                    ImGui::SameLine(75);
                    
                    // 画一个粉红色的进度条
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.96f, 0.64f, 0.69f, 1.0f)); 
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.9f, 0.9f, 0.9f, 1.0f)); // 进度条底色
                    float progress = save.affection / 100.0f; // 转换为 0.0 ~ 1.0 的比例
                    ImGui::ProgressBar(progress, ImVec2(160, 16), std::to_string(save.affection).c_str());
                    ImGui::PopStyleColor(2);

                    // 右侧绝对定位：唤醒、删除按钮
                    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 170, 25)); // 往左调一点以便放两个按钮
                    
                    // 【唤醒记忆 按钮】
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.85f, 0.9f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                    if (ImGui::Button("唤醒记忆", ImVec2(80, 35))) {
                        if (loadGame("saves/" + save.fileName)) {
                            uiChatHistory.push_back({"系统", "（时空震荡，已成功回溯至 " + save.npcName + " 的时间线）"});
                            showLoadMenu = false; 
                        }
                    }
                    ImGui::PopStyleColor(2);

                    ImGui::SameLine();

                    // 【抹除（删除） 按钮】
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.4f, 0.4f, 0.8f)); // 偏红的危险色
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    if (ImGui::Button("抹除", ImVec2(60, 35))) {
                        try {
                            std::filesystem::remove("saves/" + save.fileName); // 删除物理文件
                            scanSaveFiles(); // 立刻刷新后端的 vector
                            
                            // 【极为重要】：因为 scanSaveFiles() 改变了 parsedSaves 的大小，
                            // 必须立刻 Pop 掉所有样式并跳出当前这一帧的 for 循环，否则程序会崩溃！
                            ImGui::PopStyleColor(3);
                            ImGui::EndChild();
                            ImGui::PopStyleVar();
                            ImGui::PopStyleColor();
                            ImGui::PopID();
                            break; 
                        } catch (const std::exception& e) {
                            std::cerr << "删除存档失败: " << e.what() << '\n';
                        }
                    }
                    ImGui::PopStyleColor(3);
                    // ======================================

                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();
                    ImGui::PopID();
                    ImGui::Spacing(); // 卡片之间的垂直间距
                }
                ImGui::EndChild();
            }
        }
        ImGui::End();
    }
    // 设定全屏窗口
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    
    ImGui::Begin("MainGameWindow", nullptr, window_flags);

    // 动态计算上下比例：上半部(立绘+状态)占 40%，下半部(聊天+交互)占 60%
    float total_avail_y = ImGui::GetContentRegionAvail().y;
    float top_height = total_avail_y * 0.4f;

    // ====================================================
    // 区域 A & B：上半部分 (左侧立绘 35% + 右侧情报 65%)
    // ====================================================
    ImGui::BeginChild("TopArea", ImVec2(0, top_height), false);

    float left_width = ImGui::GetContentRegionAvail().x * 0.35f; 
    ImGui::BeginChild("PortraitArea", ImVec2(left_width, 0), true);

    if (isGeneratingPortrait) {
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2.0f);
        ImGui::TextDisabled("... AI 画师正在描绘 %s ...", currentNPC.name.c_str());
    } 
    else if (npcImageLoader.isLoaded()) {
        ImVec2 availSize = ImGui::GetContentRegionAvail();
        float aspect = (float)npcImageLoader.getWidth() / (float)npcImageLoader.getHeight();
        ImVec2 imageSize;
        if (availSize.x / aspect <= availSize.y) {
            imageSize = ImVec2(availSize.x, availSize.x / aspect);
        } else {
            imageSize = ImVec2(availSize.y * aspect, availSize.y);
        }
        float offsetX = (availSize.x - imageSize.x) * 0.5f;
        float offsetY = (availSize.y - imageSize.y) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);
        ImGui::Image((void*)(intptr_t)npcImageLoader.getTextureID(), imageSize);
    } 
    else {
        ImGui::Text("暂无立绘数据");
    }
    ImGui::EndChild(); // 结束立绘区

    ImGui::SameLine(); 

    // 右侧情报区占据剩余宽度
    ImGui::BeginChild("InfoArea", ImVec2(0, 0), true);

    std::string timeStr = (currentTime == TimeOfDay::MORNING) ? "清晨" : 
                      (currentTime == TimeOfDay::NOON) ? "午后" : "深夜";

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "【当前阶段】: %s", timeStr.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(距离下个时段: %d 回合)", 5 - chatTurns);
    ImGui::Separator();
    
    ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.8f, 1.0f), "【当前世界观】");
    ImGui::InputTextMultiline("##WorldSetting", worldSettingBuf, IM_ARRAYSIZE(worldSettingBuf), 
    ImVec2(ImGui::GetContentRegionAvail().x - 100, ImGui::GetTextLineHeight() * 3)); // 稍微改矮一点
    ImGui::SameLine();
    
    if (isGeneratingWorld) {
        ImGui::BeginDisabled(); ImGui::Button("构思中...", ImVec2(80, 0)); ImGui::EndDisabled();
    } else {
        if (ImGui::Button("随机天意", ImVec2(80, 0))) {
            isGeneratingWorld = true;
            futureWorldSetting = ProfileGenerator::generateRandomWorldSettingAsync();
        }
    }
    ImGui::Separator();

    ImGui::TextColored(ImVec4(0.3f, 0.7f, 0.9f, 1.0f), "--- 【%s】 的状态 ---", mainPlayer.getName().c_str());
    if (isGeneratingPlayer) {
        ImGui::Text("正在重塑灵魂...");
    } else {
        ImGui::TextWrapped("【身世】: %s", mainPlayer.getBackstory().c_str());
    }
    ImGui::Separator();
    
    ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.6f, 1.0f), "--- 邂逅的角色 ---");
    if (isGeneratingNPC) {
        ImGui::Text("正在呼唤跨越次元的灵魂，请稍候...");
    } else if (currentNPC.is_generated) {
        ImGui::Text("姓名: %s  |  初始好感: %d (%s)", currentNPC.name.c_str(), currentNPC.initial_affection, currentNPC.initial_attitude.c_str());
        ImGui::TextWrapped("【外貌】: %s", currentNPC.appearance.c_str());
        ImGui::TextWrapped("【性格】: %s", currentNPC.personality_core.c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.5f, 0.5f, 1.0f));
        ImGui::TextWrapped("【执念/秘密】: %s", currentNPC.hidden_trauma.c_str());
        ImGui::PopStyleColor();
    } else {
        ImGui::Text("暂无角色。请点击下方按钮邂逅新的缘分。");
    }
    ImGui::EndChild(); // 结束情报区

    ImGui::EndChild(); // 结束上半部分 TopArea

    ImGui::Spacing();
    ImGui::Separator();

    // ====================================================
    // 区域 C & D：下半部分 (全宽对话记录 + 底部按钮交互)
    // ====================================================
    
    // 提前计算底部交互区需要的高度
    float bottom_action_height = ImGui::GetFrameHeightWithSpacing() * 3.0f;
    float chat_history_height = ImGui::GetContentRegionAvail().y - bottom_action_height - 10.0f;

    ImGui::BeginChild("ChatHistory", ImVec2(0, -90.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
    
    for (const auto& chat : uiChatHistory) {
        ImVec4 textColor;
        bool isInsight = false; // 用于标记是否是洞察信息

        // 1. 分配颜色
        if (chat.first == "[暗中洞察]") {
            textColor = ImVec4(0.8f, 0.6f, 1.0f, 1.0f); // 神秘的紫色/亮紫色
            isInsight = true;
        } else if (chat.first == "系统" || chat.first == "【GM 场景导入】" || chat.first == "【GM 突发事件】") {
            textColor = ImVec4(0.6f, 0.6f, 0.6f, 1.0f); // 旁白灰色
        } else if (chat.first == mainPlayer.getName()) {
            textColor = ImVec4(0.5f, 0.7f, 0.9f, 1.0f); // 玩家蓝色
        } else {
            textColor = ImVec4(0.9f, 0.5f, 0.6f, 1.0f); // 女主粉色
        }
        
        // 2. 渲染文本
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        
        if (isInsight) {
            // 如果是洞察信息，把冒号去掉，改成空格，比如：[暗中洞察] (共情检定成功) 她手心出汗了...
            ImGui::TextWrapped("%s %s", chat.first.c_str(), chat.second.c_str());
        } else {
            // 普通对话
            ImGui::TextWrapped("%s: %s", chat.first.c_str(), chat.second.c_str());
        }
        
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    if (isWaitingForReply) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s 正在思考...", activeNPC->getName().c_str());
    }
    
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild(); // 结束聊天记录区

    // 底部动作交互区
    ImGui::BeginChild("ActionArea", ImVec2(0, 0), false);
    
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

    if (ImGui::Button("保存命运线", ImVec2(100, 0))) {
        std::filesystem::create_directory("saves");
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        char timeStrBuf[32];
        std::strftime(timeStrBuf, sizeof(timeStrBuf), "%Y%m%d_%H%M%S", &tm);
        std::string savePath = "saves/save_" + std::string(timeStrBuf) + ".json";
        
        if (saveGame(savePath)) {
            uiChatHistory.push_back({"系统", "（当前命运的轨迹已保存）"});
            scanSaveFiles(); 
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
                futureEncounter = ProfileGenerator::generateEncounterAsync(std::string(worldSettingBuf), mainPlayer, currentNPC);
            }
            ImGui::PopStyleColor(2);
        }
    }
    ImGui::Spacing();

    // 常规聊天模式 VS 事件选择模式
    if (isEventActive) {
        for (size_t i = 0; i < currentEvent.choices.size(); ++i) {
            std::string choiceText = currentEvent.choices[i].text;
            std::string choiceStat = currentEvent.choices[i].stat;
            std::string displayStat = choiceStat;    

            std::string buttonLabel = choiceText + " [" + displayStat + "检定]##" + std::to_string(i);
            
            if (ImGui::Button(buttonLabel.c_str())) {
                
                int playerStatValue = 50; 
                if (choiceStat == "physique") playerStatValue = mainPlayer.getPhysique();
                else if (choiceStat == "intellect") playerStatValue = mainPlayer.getIntellect();
                else if (choiceStat == "charm") playerStatValue = mainPlayer.getCharm();
                else if (choiceStat == "wealth") playerStatValue = mainPlayer.getWealth();
                else if (choiceStat == "empathy") playerStatValue = mainPlayer.getEmpathy();
                else if (choiceStat == "luck") playerStatValue = mainPlayer.getLuck();

                // 【终极重构】：D100 命运掷骰规则
                int roll = rand() % 100 + 1; // 1 - 100
                std::string rollResult;
                std::string exactRollDesc = "(掷出: " + std::to_string(roll) + " / 要求: " + std::to_string(playerStatValue) + ")";

                // 判断逻辑：加入大成功与大失败
                if (roll <= 5) {
                    rollResult = "【大成功 (Critical Success)】";
                } else if (roll > 95) {
                    rollResult = "【大失败 (Critical Failure)】";
                } else if (roll <= playerStatValue) {
                    rollResult = "【检定成功 (Success)】";
                } else {
                    rollResult = "【检定失败 (Failure)】";
                }

                // UI 上屏：让玩家感受到掷骰子的刺激感
                uiChatHistory.push_back({mainPlayer.getName(), "尝试: " + choiceText + " [" + displayStat + "检定]"});
                uiChatHistory.push_back({"系统", "命运判定：" + rollResult + " " + exactRollDesc});
                
                isEventActive = false;
                isWaitingForReply = true;
                
                // 【终极重构】：给大模型极其详尽的战报上下文！
                std::string timeContext = (currentTime == TimeOfDay::MORNING) ? "清晨" : 
                                          (currentTime == TimeOfDay::NOON) ? "午后" : "深夜";
                
                futureReply = std::async(std::launch::async, [this, choiceText, rollResult, roll, timeContext]() {
                    std::string contextualInput = 
                        "（系统机制输入，请勿暴露给玩家：当前时间是" + timeContext + "。\n"
                        "玩家采取了行动：【" + choiceText + "】。\n"
                        "底层 TRPG 引擎进行了掷骰判定，结果为：" + rollResult + "（点数 " + std::to_string(roll) + "）。\n"
                        "指令：请作为 GM 和 NPC 的集合体，严格根据这个判定结果推进剧情。\n"
                        "1. 如果是大成功，请描写玩家的行动引发了极其帅气、完美的奇迹反应，NPC 好感度飙升。\n"
                        "2. 如果是大失败，请描写极其社死、倒霉、甚至搞笑的意外，NPC 会感到震惊或无语。\n"
                        "3. 请用第一人称（NPC 视角）或第二人称旁白融合的方式作答。）";
                        
                    return activeNPC->interact(contextualInput, mainPlayer);
                });
            }
        }
    } else {
        static char inputBuf[512] = ""; 
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80);
        bool pressed = ImGui::InputText("##ChatInput", inputBuf, IM_ARRAYSIZE(inputBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        
        bool disableInput = isWaitingForReply || isGeneratingTransition || activeNPC == nullptr || !hasEncounterStarted;
        if (disableInput) ImGui::BeginDisabled();
        
        if (ImGui::Button("发送") || (pressed && !disableInput)) {
            if (strlen(inputBuf) > 0) {
                std::string userText = inputBuf; 
                uiChatHistory.push_back({mainPlayer.getName(), userText}); 
                inputBuf[0] = '\0'; 
                isWaitingForReply = true;
                
                std::string timeCtx = (currentTime == TimeOfDay::MORNING) ? "清晨" : 
                                      (currentTime == TimeOfDay::NOON) ? "午后" : "深夜";
                std::string hiddenInput = "（系统提示：当前游戏时间是" + timeCtx + "）" + userText;
                
                if (chatTurns >= 5) { 
                    hiddenInput = "（系统提示：你们在这个场景聊得有一段时间了。请寻找合适的借口自然地结束当前话题。你可以选择提议去附近的另一个地方继续约会，或者直接提出告别结束今天的见面，并务必将 ready_to_transition 设为 true）\n玩家说：" + userText;
                } else {
                    hiddenInput = "（系统提示：当前游戏时间是" + timeCtx + "）\n玩家说：" + userText;
                }

                futureReply = std::async(std::launch::async, [activeNPC = this->activeNPC, playerCopy = this->mainPlayer, hiddenInput]() {
                    return activeNPC->interact(hiddenInput, playerCopy);
                });
            }
        }
        if (disableInput) ImGui::EndDisabled();
    }
    
    ImGui::EndChild(); // 结束动作交互区

    ImGui::End(); // MainGameWindow 结束
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
    parsedSaves.clear();
    const std::string saveDir = "saves"; // 在游戏根目录下的 saves 文件夹

    // 如果文件夹不存在，就自动创建一个
    if (!fs::exists(saveDir)) {
        fs::create_directory(saveDir);
        return;
    }

    // 遍历文件夹，提取所有的 .json 存档文件
    for (const auto& entry : std::filesystem::directory_iterator(saveDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            std::string fileName = entry.path().filename().string();
            availableSaves.push_back(fileName);

            // 创建一张默认的记忆卡片
            SaveFileInfo info;
            info.fileName = fileName;
            info.npcName = "独处";
            info.timeStr = "未知";
            info.affection = 0; 

            // 打开文件提取灵魂数据
            std::ifstream file(entry.path());
            if (file.is_open()) {
                try {
                    nlohmann::json j;
                    file >> j;
                    
                    // 1. 提取名字和时间
                    info.npcName = j.value("profile_name", "神秘人");
                    int timeVal = j.value("currentTime", 0);
                    info.timeStr = (timeVal == 0) ? "清晨" : (timeVal == 1) ? "午后" : "深夜";

                    // 2. 深入 activeNPC 内部提取好感度
                    if (j.contains("activeNPC") && j["activeNPC"].is_object()) {
                        info.affection = j["activeNPC"].value("affection", 0);
                    }
                } catch (...) {
                    info.npcName = "数据碎片(已损坏)";
                }
                file.close();
            }
            // 将精美的卡片塞入列表
            parsedSaves.push_back(info);
        }
    }
}

bool GameManager::saveGame(const std::string& filename) {
    nlohmann::json saveData;

    // 1. 存储全局系统状态（包括最新的时间和回合数）
    saveData["currentTime"] = static_cast<int>(currentTime);
    saveData["chatTurns"] = chatTurns;
    saveData["worldSetting"] = std::string(worldSettingBuf);

    // 2. 存储玩家数据 (现在的变量叫 mainPlayer，且是对象不是指针)
    saveData["player"] = mainPlayer.toJson();

    // 3. 存储当前互动的 NPC 数据 (现在的变量叫 activeNPC)
    if (activeNPC != nullptr) {
        saveData["activeNPC"] = activeNPC->toJson();
        
        // 顺便存一下当前女生的基础档案 (currentNPC 结构体)
        saveData["profile_name"] = currentNPC.name;
        saveData["profile_appearance"] = currentNPC.appearance;
        saveData["profile_personality"] = currentNPC.personality_core;
        saveData["profile_trauma"] = currentNPC.hidden_trauma;
        saveData["profile_is_generated"] = currentNPC.is_generated;
    }

    // 4. 存储 UI 界面上显示的聊天记录（保证读档后画面一模一样）
    nlohmann::json chatArray = nlohmann::json::array();
    for (const auto& chat : uiChatHistory) {
        chatArray.push_back({
            {"speaker", chat.first},
            {"content", chat.second}
        });
    }
    saveData["uiChatHistory"] = chatArray;

    // 5. 写入本地 JSON 文件
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open save file for writing!" << std::endl;
        return false;
    }
    file << saveData.dump(4); // 格式化输出，方便查错
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

    // 1. 恢复系统时间与回合状态
    currentTime = static_cast<TimeOfDay>(saveData.value("currentTime", 0));
    chatTurns = saveData.value("chatTurns", 0);
    
    // 恢复世界观输入框
    std::string wSetting = saveData.value("worldSetting", "现代日常都市");
    strncpy(worldSettingBuf, wSetting.c_str(), sizeof(worldSettingBuf) - 1);
    worldSettingBuf[sizeof(worldSettingBuf) - 1] = '\0';

    // 2. 恢复玩家本体
    if (saveData.contains("player")) {
        mainPlayer.fromJson(saveData["player"]);
    }

    // 3. 恢复当前的 NPC 和跨次元视觉影像
    if (saveData.contains("activeNPC")) {
        // 先恢复她的基础档案设定
        currentNPC.name = saveData.value("profile_name", "");
        currentNPC.appearance = saveData.value("profile_appearance", "");
        currentNPC.personality_core = saveData.value("profile_personality", "");
        currentNPC.hidden_trauma = saveData.value("profile_trauma", "");
        currentNPC.is_generated = saveData.value("profile_is_generated", false);

        // 重新实例化大脑并灌入记忆
        activeNPC = std::make_shared<NPC>("Temp", "Temp");
        activeNPC->fromJson(saveData["activeNPC"]);

        // 【图形学魔法】：根据 JSON 里的路径，重新把立绘塞回 OpenGL 显卡！
        std::string portraitPath = activeNPC->getPortraitPath();
        if (!portraitPath.empty()) {
            npcImageLoader.LoadFromFile(portraitPath);
        }
        hasEncounterStarted = true; // 既然能存下来，那肯定已经相遇了
    }

    // 4. 恢复右侧 UI 的聊天气泡记录
    uiChatHistory.clear();
    if (saveData.contains("uiChatHistory")) {
        for (const auto& item : saveData["uiChatHistory"]) {
            uiChatHistory.push_back({item["speaker"], item["content"]});
        }
    }

    return true;
}
