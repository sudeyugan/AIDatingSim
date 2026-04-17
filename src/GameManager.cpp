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
    uiChatHistory.push_back({"系统", "请前往【档案】页面设定世界观，并邂逅新角色。"});
    
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
    currentTab = UINavTab::PROFILE;
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

    if (isGeneratingBg && futureBg.valid()) {
        if (futureBg.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            std::string bgPath = futureBg.get();
            isGeneratingBg = false;
            if (!bgPath.empty()) {
                chatBgLoader.LoadFromFile(bgPath);
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
            currentTab = UINavTab::CHAT;
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

                isGeneratingBg = true;
                futureBg = std::async(std::launch::async, [transitionScene]() {
                    // TODO: 在这里调用你的 AI 绘画 API，传入 transitionScene 作为 Prompt
                    // 比如: return ProfileGenerator::generateBackgroundAsync(transitionScene);
                    return std::string(""); 
                });
                // ====================================================================
            }
        }
    }
}

void GameManager::drawChatBubble(const std::string& name, const std::string& text, int type) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float avail_width = ImGui::GetContentRegionAvail().x;
    float wrap_width = avail_width * 0.55f; // 限制气泡最大宽度
    
    ImVec2 text_size = ImGui::CalcTextSize(text.c_str(), NULL, true, wrap_width);
    ImVec2 padding(16, 14); // 稍微增加一点内边距，让文字呼吸感更强
    ImVec2 bubble_size(text_size.x + padding.x * 2, text_size.y + padding.y * 2);
    
    float avatar_size = 44.0f;
    float item_spacing = 12.0f;
    
    // --- 居中系统提示文本优化 ---
    if (type == 0 || type == 3) { 
        ImGui::SetCursorPosX((avail_width - text_size.x) * 0.5f);
        // 给系统文字加一个极其微弱的半透明圆角底色，看起来像微信的系统提示
        ImVec2 sys_pos = ImGui::GetCursorScreenPos();
        draw_list->AddRectFilled(ImVec2(sys_pos.x - 10, sys_pos.y - 4), 
                                 ImVec2(sys_pos.x + text_size.x + 10, sys_pos.y + text_size.y + 4), 
                                 IM_COL32(0, 0, 0, 20), 12.0f);
                                 
        ImGui::PushStyleColor(ImGuiCol_Text, type == 3 ? ImVec4(0.6f, 0.4f, 0.8f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::TextWrapped("%s", text.c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing(); ImGui::Spacing();
        return;
    }

    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
    float line_height = std::max(bubble_size.y, avatar_size) + item_spacing * 2;
    std::string initial = name.length() > 0 ? name.substr(0, std::min<size_t>(3, name.length())) : "?";
    ImVec2 char_size = ImGui::CalcTextSize(initial.c_str());

    // 阴影配置
    ImVec2 shadow_offset(0.0f, 4.0f); // 向下偏移
    ImU32 shadow_color = IM_COL32(0, 0, 0, 25); // 非常淡的黑影

    if (type == 1) { // ========== 玩家（靠右，绿色系） ==========
        float start_x = cursor_pos.x + avail_width - bubble_size.x - avatar_size - item_spacing * 2;
        ImVec2 bubble_pos = ImVec2(start_x, cursor_pos.y);
        
        // 1. 画气泡阴影
        draw_list->AddRectFilled(ImVec2(bubble_pos.x + shadow_offset.x, bubble_pos.y + shadow_offset.y), 
                                 ImVec2(bubble_pos.x + bubble_size.x + shadow_offset.x, bubble_pos.y + bubble_size.y + shadow_offset.y), 
                                 shadow_color, 12.0f);
        // 2. 画气泡本体 (现代化的微渐变感，这里用实色替代)
        ImU32 bg_color = IM_COL32(165, 245, 120, 240); // 稍微亮一点的苹果绿
        draw_list->AddRectFilled(bubble_pos, ImVec2(bubble_pos.x + bubble_size.x, bubble_pos.y + bubble_size.y), bg_color, 12.0f); 
        // 3. 气泡微小描边 (增加精致感)
        draw_list->AddRect(bubble_pos, ImVec2(bubble_pos.x + bubble_size.x, bubble_pos.y + bubble_size.y), IM_COL32(140, 220, 100, 200), 12.0f, 0, 1.0f);

        // 文字
        ImGui::SetCursorScreenPos(ImVec2(bubble_pos.x + padding.x, bubble_pos.y + padding.y));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.15f, 0.25f, 0.15f, 1.0f)); // 深绿色文字比纯黑更护眼
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrap_width);
        ImGui::TextUnformatted(text.c_str());
        ImGui::PopTextWrapPos(); ImGui::PopStyleColor();
        
        // 玩家头像阴影与本体
        ImVec2 avatar_pos = ImVec2(bubble_pos.x + bubble_size.x + item_spacing, cursor_pos.y);
        draw_list->AddRectFilled(ImVec2(avatar_pos.x, avatar_pos.y + 3), ImVec2(avatar_pos.x + avatar_size, avatar_pos.y + avatar_size + 3), shadow_color, 8.0f);
        draw_list->AddRectFilled(avatar_pos, ImVec2(avatar_pos.x + avatar_size, avatar_pos.y + avatar_size), IM_COL32(120, 170, 255, 255), 8.0f);
        draw_list->AddText(ImVec2(avatar_pos.x + (avatar_size - char_size.x)*0.5f, avatar_pos.y + (avatar_size - char_size.y)*0.5f), IM_COL32(255,255,255,255), initial.c_str());
        draw_list->AddRect(avatar_pos, ImVec2(avatar_pos.x + avatar_size, avatar_pos.y + avatar_size), IM_COL32(255,255,255,100), 8.0f, 0, 1.5f); // 头像高光边
        
    } else { // ========== NPC（靠左，白粉系） ==========
        float start_x = cursor_pos.x + item_spacing;
        
        // 头像部分
        ImVec2 avatar_pos = ImVec2(start_x, cursor_pos.y);
        draw_list->AddRectFilled(ImVec2(avatar_pos.x, avatar_pos.y + 3), ImVec2(avatar_pos.x + avatar_size, avatar_pos.y + avatar_size + 3), shadow_color, 8.0f);
        draw_list->AddRectFilled(avatar_pos, ImVec2(avatar_pos.x + avatar_size, avatar_pos.y + avatar_size), IM_COL32(255, 170, 190, 255), 8.0f);
        draw_list->AddText(ImVec2(avatar_pos.x + (avatar_size - char_size.x)*0.5f, avatar_pos.y + (avatar_size - char_size.y)*0.5f), IM_COL32(255,255,255,255), initial.c_str());
        draw_list->AddRect(avatar_pos, ImVec2(avatar_pos.x + avatar_size, avatar_pos.y + avatar_size), IM_COL32(255,255,255,100), 8.0f, 0, 1.5f);

        // 气泡部分
        ImVec2 bubble_pos = ImVec2(avatar_pos.x + avatar_size + item_spacing, cursor_pos.y);
        draw_list->AddRectFilled(ImVec2(bubble_pos.x + shadow_offset.x, bubble_pos.y + shadow_offset.y), 
                                 ImVec2(bubble_pos.x + bubble_size.x + shadow_offset.x, bubble_pos.y + bubble_size.y + shadow_offset.y), 
                                 shadow_color, 12.0f);
        draw_list->AddRectFilled(bubble_pos, ImVec2(bubble_pos.x + bubble_size.x, bubble_pos.y + bubble_size.y), IM_COL32(255, 255, 255, 245), 12.0f); 
        // 给NPC白色气泡加一层非常淡的灰色描边，防止在白背景中隐形
        draw_list->AddRect(bubble_pos, ImVec2(bubble_pos.x + bubble_size.x, bubble_pos.y + bubble_size.y), IM_COL32(230, 230, 235, 255), 12.0f, 0, 1.0f);
        
        ImGui::SetCursorScreenPos(ImVec2(bubble_pos.x + padding.x, bubble_pos.y + padding.y));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.2f, 0.22f, 1.0f));
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrap_width);
        ImGui::TextUnformatted(text.c_str());
        ImGui::PopTextWrapPos(); ImGui::PopStyleColor();
    }
    
    ImGui::SetCursorScreenPos(ImVec2(cursor_pos.x, cursor_pos.y + line_height));
}

void GameManager::renderUI() {
    // 设定全屏无边框主窗口
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    
    ImGui::Begin("MainGameWindow", nullptr, window_flags);

    // ================= 左侧导航栏 =================
    float nav_width = 160.0f;
    ImGui::BeginChild("NavBar", ImVec2(nav_width, 0), true);
    
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.0f);
    ImGui::TextColored(ImVec4(0.96f, 0.54f, 0.60f, 1.0f), "  Dating Sim v2.0");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.1f, 0.5f));
    if (ImGui::Selectable("聊天 (Chat)", currentTab == UINavTab::CHAT, 0, ImVec2(0, 45))) currentTab = UINavTab::CHAT;
    if (ImGui::Selectable("档案 (Profile)", currentTab == UINavTab::PROFILE, 0, ImVec2(0, 45))) currentTab = UINavTab::PROFILE;
    if (ImGui::Selectable("系统 (System)", currentTab == UINavTab::SYSTEM, 0, ImVec2(0, 45))) {
        currentTab = UINavTab::SYSTEM;
        scanSaveFiles(); // 进入系统页时刷新存档
    }
    ImGui::PopStyleVar();

    ImGui::EndChild();
    ImGui::SameLine();

    // ================= 右侧主内容区 =================
    ImGui::BeginChild("MainContent", ImVec2(0, 0), false);

    if (currentTab == UINavTab::CHAT) {
        // --- 聊天 Header ---
        ImGui::BeginChild("ChatHeader", ImVec2(0, 50), true);
        std::string npcNameDisplay = activeNPC ? activeNPC->getName() : "未邂逅";
        std::string timeStr = (currentTime == TimeOfDay::MORNING) ? "清晨" : (currentTime == TimeOfDay::NOON) ? "午后" : "深夜";
        
        ImGui::SetCursorPos(ImVec2(15, 15));
        ImGui::Text("正在与 %s 聊天  |  时间环境: %s", npcNameDisplay.c_str(), timeStr.c_str());
        
        if (isWaitingForReply) { 
            ImGui::SameLine(ImGui::GetWindowWidth() - 160); 
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "对方正在思索回复..."); 
        }
        ImGui::EndChild();

        // --- 聊天主体区 ---
        // 动态计算底部输入区高度 (如果发生事件，需要更多空间放置选项按钮)
        float input_area_height = isEventActive ? ImGui::GetFrameHeightWithSpacing() * 5.0f : 60.0f;
        
        ImGui::BeginChild("ChatHistoryArea", ImVec2(0, -input_area_height), false, ImGuiWindowFlags_NoScrollWithMouse);
        
        // 绘制聊天背景图 (如果有)
        ImVec2 chatMin = ImGui::GetWindowPos();
        ImVec2 chatMax = ImVec2(chatMin.x + ImGui::GetWindowSize().x, chatMin.y + ImGui::GetWindowSize().y);
        if (chatBgLoader.isLoaded()) {
            ImGui::GetWindowDrawList()->AddImage((void*)(intptr_t)chatBgLoader.getTextureID(), chatMin, chatMax, ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255,180));
        } else {
            ImGui::GetWindowDrawList()->AddRectFilled(chatMin, chatMax, IM_COL32(245, 245, 248, 255)); // 浅灰背景兜底
        }

        // 内部嵌套一个可滚动的窗口用于文本排列
        ImGui::BeginChild("ChatScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::Spacing();
        
        for (const auto& chat : uiChatHistory) {
            ImVec4 textColor;
            bool isInsight = false;

            // 保留原有的颜色分配逻辑
            if (chat.first == "[暗中洞察]") {
                textColor = ImVec4(0.8f, 0.6f, 1.0f, 1.0f);
                isInsight = true;
            } else if (chat.first == "系统" || chat.first == "【GM 场景导入】" || chat.first == "【GM 突发事件】" || chat.first == "【GM 场景重构】") {
                textColor = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
            } else if (chat.first == mainPlayer.getName()) {
                textColor = ImVec4(0.3f, 0.5f, 0.8f, 1.0f); 
            } else {
                textColor = ImVec4(0.9f, 0.4f, 0.5f, 1.0f); 
            }
            
            // 渲染文本
            ImGui::PushStyleColor(ImGuiCol_Text, textColor);
            if (isInsight) {
                ImGui::TextWrapped("%s %s", chat.first.c_str(), chat.second.c_str());
            } else {
                ImGui::TextWrapped("%s: %s", chat.first.c_str(), chat.second.c_str());
            }
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
        ImGui::EndChild();

        // --- 聊天底部输入 / 事件检定区 ---
        ImGui::BeginChild("ChatInput", ImVec2(0, 0), true);
        
        if (isEventActive) {
            ImGui::SetCursorPos(ImVec2(10, 10));
            ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "【突发事件！请做出选择】");
            ImGui::Spacing();
            
            // 完全保留原有的 D100 检定核心逻辑
            for (size_t i = 0; i < currentEvent.choices.size(); ++i) {
                std::string choiceText = currentEvent.choices[i].text;
                std::string choiceStat = currentEvent.choices[i].stat;
                std::string buttonLabel = choiceText + " [" + choiceStat + "检定]##" + std::to_string(i);
                
                if (ImGui::Button(buttonLabel.c_str(), ImVec2(-1, 30))) {
                    int playerStatValue = 50; 
                    if (choiceStat == "physique") playerStatValue = mainPlayer.getPhysique();
                    else if (choiceStat == "intellect") playerStatValue = mainPlayer.getIntellect();
                    else if (choiceStat == "charm") playerStatValue = mainPlayer.getCharm();
                    else if (choiceStat == "wealth") playerStatValue = mainPlayer.getWealth();
                    else if (choiceStat == "empathy") playerStatValue = mainPlayer.getEmpathy();
                    else if (choiceStat == "luck") playerStatValue = mainPlayer.getLuck();

                    int roll = rand() % 100 + 1; 
                    std::string rollResult;
                    std::string exactRollDesc = "(掷出: " + std::to_string(roll) + " / 要求: " + std::to_string(playerStatValue) + ")";

                    if (roll <= 5) rollResult = "【大成功 (Critical Success)】";
                    else if (roll > 95) rollResult = "【大失败 (Critical Failure)】";
                    else if (roll <= playerStatValue) rollResult = "【检定成功 (Success)】";
                    else rollResult = "【检定失败 (Failure)】";

                    uiChatHistory.push_back({mainPlayer.getName(), "尝试: " + choiceText + " [" + choiceStat + "检定]"});
                    uiChatHistory.push_back({"系统", "命运判定：" + rollResult + " " + exactRollDesc});
                    
                    isEventActive = false;
                    isWaitingForReply = true;
                    
                    std::string timeContext = (currentTime == TimeOfDay::MORNING) ? "清晨" : (currentTime == TimeOfDay::NOON) ? "午后" : "深夜";
                    std::string contextualInput = 
                        "（系统机制输入，请勿暴露给玩家：当前时间是" + timeContext + "。\n"
                        "玩家采取了行动：【" + choiceText + "】。\n"
                        "底层 TRPG 引擎进行了掷骰判定，结果为：" + rollResult + "（点数 " + std::to_string(roll) + "）。\n"
                        "指令：请作为 GM 和 NPC 的集合体，严格根据这个判定结果推进剧情。\n"
                        "1. 如果是大成功，请描写奇迹反应，NPC 好感度飙升。\n"
                        "2. 如果是大失败，请描写极其倒霉的意外，NPC 感到无语。\n"
                        "3. 请用第一人称（NPC 视角）或第二人称旁白融合的方式作答。）";

                    futureReply = std::async(std::launch::async, [activeNPC = this->activeNPC, playerCopy = this->mainPlayer, contextualInput]() {
                        return activeNPC->interact(contextualInput, playerCopy);
                    });
                }
            }
        } else {
            // 普通对话输入区
            ImGui::SetCursorPos(ImVec2(10, 10));
            static char inputBuf[512] = ""; 
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90);
            
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
            bool pressed = ImGui::InputText("##ChatInput", inputBuf, IM_ARRAYSIZE(inputBuf), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PopStyleVar();
            
            ImGui::SameLine();
            
            bool disableInput = isWaitingForReply || isGeneratingTransition || activeNPC == nullptr || !hasEncounterStarted;
            if (disableInput) ImGui::BeginDisabled();
            
            if (ImGui::Button("发送", ImVec2(75, 40)) || (pressed && !disableInput)) {
                if (strlen(inputBuf) > 0) {
                    std::string userText = inputBuf; 
                    uiChatHistory.push_back({mainPlayer.getName(), userText}); 
                    inputBuf[0] = '\0'; 
                    isWaitingForReply = true;
                    
                    std::string timeCtx = (currentTime == TimeOfDay::MORNING) ? "清晨" : (currentTime == TimeOfDay::NOON) ? "午后" : "深夜";
                    std::string hiddenInput;
                    
                    // 保留疲劳回合判断
                    if (chatTurns >= 5) { 
                        hiddenInput = "（系统提示：时间是" + timeCtx + "。你们在这个场景聊得较久了。请提出转场或告别，并务必将 ready_to_transition 设为 true）\n玩家说：" + userText;
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
        ImGui::EndChild();

    } 
    else if (currentTab == UINavTab::PROFILE) {
        // ================= 档案页面 =================
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.8f, 1.0f), " 【世界与灵魂档案】");
        ImGui::Separator();
        ImGui::Spacing();
        
        // 左半边放立绘
        ImGui::BeginChild("ProfileLeft", ImVec2(ImGui::GetContentRegionAvail().x * 0.35f, 0), true);
        if (isGeneratingPortrait) {
            ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.45f);
            ImGui::TextDisabled("  ... AI 画师描绘中 ...");
        } else if (npcImageLoader.isLoaded()) {
            ImVec2 availSize = ImGui::GetContentRegionAvail();
            float aspect = (float)npcImageLoader.getWidth() / (float)npcImageLoader.getHeight();
            ImVec2 imageSize;
            if (availSize.x / aspect <= availSize.y) imageSize = ImVec2(availSize.x, availSize.x / aspect);
            else imageSize = ImVec2(availSize.y * aspect, availSize.y);
            
            float offsetX = (availSize.x - imageSize.x) * 0.5f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
            ImGui::Image((void*)(intptr_t)npcImageLoader.getTextureID(), imageSize);
        } else {
            ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.45f);
            ImGui::Text("暂无立绘数据");
        }
        ImGui::EndChild();
        
        ImGui::SameLine();
        
        // 右半边放详细情报
        ImGui::BeginChild("ProfileRight", ImVec2(0, 0), false);
        
        ImGui::TextColored(ImVec4(0.3f, 0.7f, 0.9f, 1.0f), "【主角信息】: %s", mainPlayer.getName().c_str());
        ImGui::TextWrapped("【身世】: %s", mainPlayer.getBackstory().c_str());
        ImGui::Spacing();
        if (isGeneratingPlayer) {
            ImGui::BeginDisabled(); ImGui::Button("重塑中...", ImVec2(120, 35)); ImGui::EndDisabled();
        } else {
            if (ImGui::Button("重塑人生", ImVec2(120, 35))) { isGeneratingPlayer = true; futurePlayerProfile = ProfileGenerator::generatePlayerProfileAsync(std::string(worldSettingBuf)); }
        }
        
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.6f, 1.0f), "【邂逅对象】: %s", currentNPC.is_generated ? currentNPC.name.c_str() : "无");
        if (currentNPC.is_generated) {
            ImGui::Text("好感度: %d  |  初始态度: %s", currentNPC.initial_affection, currentNPC.initial_attitude.c_str());
            ImGui::TextWrapped("【外貌】: %s", currentNPC.appearance.c_str());
            ImGui::TextWrapped("【性格】: %s", currentNPC.personality_core.c_str());
            ImGui::TextWrapped("【执念】: %s", currentNPC.hidden_trauma.c_str());
        }
        ImGui::Spacing();
        
        if (isGeneratingNPC) {
            ImGui::BeginDisabled(); ImGui::Button("寻找中...", ImVec2(120, 35)); ImGui::EndDisabled();
        } else {
            if (ImGui::Button("寻找新角色", ImVec2(120, 35))) { isGeneratingNPC = true; futureProfile = ProfileGenerator::generateRandomProfileAsync(std::string(worldSettingBuf)); }
        }
        ImGui::SameLine();
        
        if (activeNPC != nullptr && !hasEncounterStarted) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.96f, 0.54f, 0.60f, 1.0f)); 
            if (isGeneratingEncounter) {
                ImGui::BeginDisabled(); ImGui::Button("靠近中...", ImVec2(120, 35)); ImGui::EndDisabled();
            } else {
                if (ImGui::Button("开始邂逅", ImVec2(120, 35))) {
                    isGeneratingEncounter = true;
                    futureEncounter = ProfileGenerator::generateEncounterAsync(std::string(worldSettingBuf), mainPlayer, currentNPC);
                }
            }
            ImGui::PopStyleColor();
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        
        ImGui::Text("【当前世界观设定】:");
        ImGui::InputTextMultiline("##WorldSetting", worldSettingBuf, IM_ARRAYSIZE(worldSettingBuf), ImVec2(-1, 80));
        ImGui::Spacing();
        if (isGeneratingWorld) {
            ImGui::BeginDisabled(); ImGui::Button("重构中...", ImVec2(120, 35)); ImGui::EndDisabled();
        } else {
            if (ImGui::Button("AI重构世界", ImVec2(120, 35))) { 
                isGeneratingWorld = true; 
                futureWorldSetting = ProfileGenerator::generateRandomWorldSettingAsync(); 
            }
        }
        
        ImGui::EndChild(); // End ProfileRight
        
    } 
    else if (currentTab == UINavTab::SYSTEM) {
        // ================= 系统页面 =================
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.7f, 0.9f, 1.0f), " 【时间与记忆控制中心】");
        ImGui::Separator();
        ImGui::Spacing();
        
        if (ImGui::Button("保存当前命运线 (Save Game)", ImVec2(240, 45))) {
            std::filesystem::create_directory("saves");
            auto t = std::time(nullptr); auto tm = *std::localtime(&t); char tb[32];
            std::strftime(tb, sizeof(tb), "%Y%m%d_%H%M%S", &tm);
            if (saveGame("saves/save_" + std::string(tb) + ".json")) {
                uiChatHistory.push_back({"系统", "（当前命运的轨迹已顺利封存）"});
                scanSaveFiles(); // 立刻刷新
            }
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.4f, 1.0f));
        if (ImGui::Button("开启全新游戏 (New Game)", ImVec2(240, 45))) { 
            startNewGame(); 
            currentTab = UINavTab::PROFILE; // 切回档案让玩家输入
        }
        ImGui::PopStyleColor();
        
        ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.6f, 1.0f), "—— 沉睡在时间缝隙中的记忆碎片 ——");
        ImGui::Spacing();

        if (parsedSaves.empty()) {
            ImGui::TextDisabled("当前世界线一片空白，没有任何命运的记录。");
        } else {
            // 完美保留你原有的精美卡片渲染逻辑
            ImGui::BeginChild("SaveCardsRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            for (size_t i = 0; i < parsedSaves.size(); ++i) {
                const auto& save = parsedSaves[i];
                
                ImGui::PushID(i);
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 0.05f));
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
                ImGui::BeginChild("Card", ImVec2(0, 85), true, ImGuiWindowFlags_NoScrollbar);

                ImGui::SetCursorPos(ImVec2(15, 15));
                ImGui::TextColored(ImVec4(0.85f, 0.45f, 0.55f, 1.0f), "邂逅对象: %s", save.npcName.c_str());
                ImGui::SameLine(220); 
                ImGui::TextDisabled("时标: %s", save.timeStr.c_str());

                ImGui::SetCursorPos(ImVec2(15, 45));
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "世界羁绊:");
                ImGui::SameLine(90);
                
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.96f, 0.64f, 0.69f, 1.0f)); 
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f)); 
                ImGui::ProgressBar(save.affection / 100.0f, ImVec2(200, 16), std::to_string(save.affection).c_str());
                ImGui::PopStyleColor(2);

                ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 180, 25));
                
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.6f, 1.0f));
                if (ImGui::Button("唤醒回忆", ImVec2(80, 35))) { 
                    if (loadGame("saves/" + save.fileName)) {
                        uiChatHistory.push_back({"系统", "（时空震荡，已成功回溯至 " + save.npcName + " 的时间线）"});
                        currentTab = UINavTab::CHAT; // 读档成功自动切回聊天窗
                    }
                }
                ImGui::PopStyleColor();
                ImGui::SameLine();
                
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
                if (ImGui::Button("抹除", ImVec2(60, 35))) {
                    try {
                        std::filesystem::remove("saves/" + save.fileName);
                        scanSaveFiles(); 
                        ImGui::PopStyleColor();
                        ImGui::EndChild();
                        ImGui::PopStyleVar();
                        ImGui::PopStyleColor();
                        ImGui::PopID();
                        break; // 数组变化，打断当帧循环
                    } catch (...) {}
                }
                ImGui::PopStyleColor();

                ImGui::EndChild();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();
                ImGui::PopID();
                ImGui::Spacing();
            }
            ImGui::EndChild();
        }
    }

    ImGui::EndChild(); // End MainContent
    ImGui::End(); // End MainGameWindow
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
