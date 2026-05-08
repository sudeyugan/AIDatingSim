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

    chatBgLoader.Free(); 
    currentBgPath = "";
    
    // 5. 重置主角状态 (你可以根据 Player 类的构造函数自行调整)
    mainPlayer = Player("主角"); 
    playerImageLoader.Free();
    
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
            
            if (!currentNPC.is_generated || currentNPC.name == "神秘少女") {
                uiChatHistory.clear();
                uiChatHistory.push_back({"系统", "【警告】量子扰动，角色灵魂解析失败（网络超时或接口格式错误）。请再试一次。"});
                // 失败后直接跳过生图逻辑
            } else {
                std::string complexPersona = "外貌：" + currentNPC.appearance + "。核心性格：" + currentNPC.personality_core + "。隐藏创伤/执念：" + currentNPC.hidden_trauma;
                activeNPC = std::make_unique<NPC>(currentNPC.name, complexPersona, currentNPC.initial_affection);
                
                npcImageLoader.Free(); // 清空上一任 NPC 的立绘
                
                // 只有成功了才去请求昂贵的生图 API
                isGeneratingPortrait = true;
                futurePortrait = activeNPC->generatePortraitAsync();

                hasEncounterStarted = false; 
                uiChatHistory.clear();
                uiChatHistory.push_back({"系统", currentNPC.name + " 的灵魂已在当前世界降临。"});
                uiChatHistory.push_back({"系统", "请在准备好后，点击下方的“开始邂逅”按钮。"});
            }       
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
                currentBgPath = bgPath;
            }
        }
    }

    // 检查玩家“重置人生”是否完成
    if (isGeneratingPlayer && futurePlayerProfile.valid()) {
        if (futurePlayerProfile.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            mainPlayer = futurePlayerProfile.get(); 
            isGeneratingPlayer = false;         
            
            if (mainPlayer.getBackstory().find("记忆解析失败") != std::string::npos) {
                uiChatHistory.clear();
                uiChatHistory.push_back({"系统", "【警告】时空乱流，人生重塑失败（网络或解析错误）。请重新点击重塑人生。"});
                // 失败后直接跳过生图逻辑
            } else {
                // ===========触发主角头像生成 ===========
                playerImageLoader.Free(); // 清空老头像
                
                // 只有成功了才去请求生图 API
                isGeneratingPlayerPortrait = true;
                futurePlayerPortrait = mainPlayer.generatePortraitAsync();
                // ===============================================

                uiChatHistory.clear();
                hasEncounterStarted = false;           
                uiChatHistory.push_back({"系统", "命运的齿轮转动，你以新身份降临。"});
            }
        }
    }

    if (isGeneratingPlayerPortrait && futurePlayerPortrait.valid()) {
        if (futurePlayerPortrait.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            bool success = futurePlayerPortrait.get();
            isGeneratingPlayerPortrait = false;
            
            if (success && !mainPlayer.getPortraitPath().empty()) {
                // 读取硬盘图片，转换成 OpenGL 纹理
                playerImageLoader.LoadFromFile(mainPlayer.getPortraitPath());
            }
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

            isGeneratingBg = true;
            // 使用邂逅的开场场景描述作为生图的 Prompt
            futureBg = std::async(std::launch::async, [encounterScene]() {
                return ProfileGenerator::generateBackgroundAsync(encounterScene);
            });
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
                    return ProfileGenerator::generateBackgroundAsync(transitionScene);
                });
                // ====================================================================
            }
        }
    }
}

void GameManager::drawChatBubble(const std::string& name, const std::string& text, int type, ImTextureID avatar_tex){
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float avail_width = ImGui::GetContentRegionAvail().x;
    float wrap_width = avail_width * 0.60f; 
    
    ImVec2 padding(20, 16);
    float avatar_radius = 24.0f;
    float avatar_size = avatar_radius * 2;
    float item_spacing = 16.0f;
    float bubble_rounding = 16.0f;
    
    // --- 居中系统提示文本 ---
    if (type == 0 || type == 3) { 
        ImGui::Spacing();
        ImVec2 start_cursor = ImGui::GetCursorScreenPos();
        
        // 分离图层：1为前景(文字)，0为背景(底框)
        draw_list->ChannelsSplit(2);
        draw_list->ChannelsSetCurrent(1); 
        
        ImVec2 calc_size = ImGui::CalcTextSize(text.c_str(), NULL, false, avail_width - 40);
        ImGui::SetCursorPosX((avail_width - calc_size.x) * 0.5f);
        ImVec2 text_pos = ImGui::GetCursorScreenPos();
        
        ImGui::PushStyleColor(ImGuiCol_Text, type == 3 ? ImVec4(0.65f, 0.45f, 0.85f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::PushTextWrapPos(text_pos.x + avail_width - 40);
        ImGui::BeginGroup(); // 打包文字组以获取精确大小
        ImGui::TextUnformatted(text.c_str());
        ImGui::EndGroup();
        ImVec2 real_text_size = ImGui::GetItemRectSize(); // 获取真实渲染后的完美尺寸
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        
        draw_list->ChannelsSetCurrent(0); 
        ImVec2 sys_bg_min = ImVec2(text_pos.x - 16, text_pos.y - 6);
        ImVec2 sys_bg_max = ImVec2(text_pos.x + real_text_size.x + 16, text_pos.y + real_text_size.y + 6);
        draw_list->AddRectFilled(sys_bg_min, sys_bg_max, IM_COL32(0, 0, 0, 15), 20.0f);
        
        draw_list->ChannelsMerge(); // 合并图层
        ImGui::SetCursorScreenPos(ImVec2(start_cursor.x, sys_bg_max.y + 15.0f));
        ImGui::Dummy(ImVec2(0.0f, 0.0f)); 
        return;
    }

    ImVec2 start_cursor = ImGui::GetCursorScreenPos();
    std::string initial = name.length() > 0 ? name.substr(0, std::min<size_t>(3, name.length())) : "?";
    ImVec2 char_size = ImGui::CalcTextSize(initial.c_str());

    // 同样将对话分为文字前景(1)和气泡背景(0)
    draw_list->ChannelsSplit(2);
    draw_list->ChannelsSetCurrent(1); 
    
    ImVec2 text_start_pos;
    float line_height = 0.0f;

    if (type == 1) { // ========== 玩家（靠右） ==========
        // 1. 预估宽度用于右对齐
        ImVec2 calc_size = ImGui::CalcTextSize(text.c_str(), NULL, false, wrap_width);
        float bubble_width = calc_size.x + padding.x * 2;
        
        // 2. 定位头像与气泡起始点
        float avatar_x = start_cursor.x + avail_width - avatar_radius - item_spacing * 2;
        ImVec2 avatar_center = ImVec2(avatar_x, start_cursor.y + avatar_radius);
        ImVec2 bubble_pos = ImVec2(avatar_x - avatar_radius - item_spacing - bubble_width, start_cursor.y);
        text_start_pos = ImVec2(bubble_pos.x + padding.x, bubble_pos.y + padding.y);
        
        // 3. 渲染文字
        ImGui::SetCursorScreenPos(text_start_pos);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.15f, 0.25f, 0.15f, 1.0f)); 
        ImGui::PushTextWrapPos(text_start_pos.x + wrap_width);
        ImGui::BeginGroup();
        ImGui::TextUnformatted(text.c_str());
        ImGui::EndGroup();
        ImVec2 real_text_size = ImGui::GetItemRectSize(); // 拿到真实换行后的高度
        ImGui::PopTextWrapPos(); 
        ImGui::PopStyleColor();
        
        // 4. 计算真实气泡边界
        ImVec2 bubble_size(bubble_width, real_text_size.y + padding.y * 2);
        ImVec2 bubble_max = ImVec2(bubble_pos.x + bubble_size.x, bubble_pos.y + bubble_size.y);
        
        // 5. 渲染头像前景
        ImVec2 avatar_p_min = ImVec2(avatar_center.x - avatar_radius, avatar_center.y - avatar_radius);
        ImVec2 avatar_p_max = ImVec2(avatar_center.x + avatar_radius, avatar_center.y + avatar_radius);
        
        if (avatar_tex) {
            // 如果有图片，直接画成完美的圆形
            draw_list->AddImageRounded((void*)(intptr_t)avatar_tex, avatar_p_min, avatar_p_max, 
                                       ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, avatar_radius);
        } else {
            // 兜底：如果没有图片，画默认文字头像
            draw_list->AddCircleFilled(avatar_center, avatar_radius, IM_COL32(100, 160, 240, 255), 32); 
            draw_list->AddText(ImVec2(avatar_center.x - char_size.x*0.5f, avatar_center.y - char_size.y*0.5f), IM_COL32(255,255,255,255), initial.c_str());
        }

        // 外圈高光描边保留，增加质感
        draw_list->AddCircle(avatar_center, avatar_radius, IM_COL32(255,255,255,150), 32, 2.0f);

        // 6. 切换到底层渲染气泡和阴影
        draw_list->ChannelsSetCurrent(0); 
        for (int i = 0; i < 4; ++i) {
            draw_list->AddRectFilled(
                ImVec2(bubble_pos.x - i, bubble_pos.y - i + 4.0f), 
                ImVec2(bubble_max.x + i, bubble_max.y + i + 4.0f), 
                IM_COL32(0, 0, 0, 8 - i * 2), bubble_rounding + i);
        }
        draw_list->AddRectFilled(bubble_pos, bubble_max, IM_COL32(215, 245, 215, 255), bubble_rounding); 
        draw_list->AddCircleFilled(ImVec2(avatar_center.x, avatar_center.y + 2), avatar_radius, IM_COL32(0,0,0,30), 32); 
        
        line_height = std::max(bubble_size.y, avatar_size) + item_spacing * 1.5f;
        
    } else { // ========== NPC（靠左） ==========
        // 1. 定位头像与气泡起始点
        float avatar_x = start_cursor.x + item_spacing + avatar_radius;
        ImVec2 avatar_center = ImVec2(avatar_x, start_cursor.y + avatar_radius);
        ImVec2 bubble_pos = ImVec2(avatar_x + avatar_radius + item_spacing, start_cursor.y);
        text_start_pos = ImVec2(bubble_pos.x + padding.x, bubble_pos.y + padding.y);
        
        // 2. 渲染文字
        ImGui::SetCursorScreenPos(text_start_pos);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.2f, 0.22f, 1.0f));
        ImGui::PushTextWrapPos(text_start_pos.x + wrap_width);
        ImGui::BeginGroup();
        ImGui::TextUnformatted(text.c_str());
        ImGui::EndGroup();
        ImVec2 real_text_size = ImGui::GetItemRectSize();
        ImGui::PopTextWrapPos(); 
        ImGui::PopStyleColor();
        
        // 3. 动态计算真实边界
        ImVec2 bubble_size(real_text_size.x + padding.x * 2, real_text_size.y + padding.y * 2);
        ImVec2 bubble_max = ImVec2(bubble_pos.x + bubble_size.x, bubble_pos.y + bubble_size.y);
        
        // 4. 渲染头像前景
        ImVec2 avatar_p_min = ImVec2(avatar_center.x - avatar_radius, avatar_center.y - avatar_radius);
        ImVec2 avatar_p_max = ImVec2(avatar_center.x + avatar_radius, avatar_center.y + avatar_radius);

        if (avatar_tex) {
            draw_list->AddImageRounded((void*)(intptr_t)avatar_tex, avatar_p_min, avatar_p_max, 
                                       ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, avatar_radius);
        } else {
            draw_list->AddCircleFilled(avatar_center, avatar_radius, IM_COL32(250, 160, 180, 255), 32);
            draw_list->AddText(ImVec2(avatar_center.x - char_size.x*0.5f, avatar_center.y - char_size.y*0.5f), IM_COL32(255,255,255,255), initial.c_str());
        }
        draw_list->AddCircle(avatar_center, avatar_radius, IM_COL32(255,255,255,150), 32, 2.0f);
        // 5. 切换到底层渲染阴影和背景
        draw_list->ChannelsSetCurrent(0); 
        for (int i = 0; i < 4; ++i) {
            draw_list->AddRectFilled(
                ImVec2(bubble_pos.x - i, bubble_pos.y - i + 4.0f), 
                ImVec2(bubble_max.x + i, bubble_max.y + i + 4.0f), 
                IM_COL32(0, 0, 0, 8 - i * 2), bubble_rounding + i);
        }
        draw_list->AddRectFilled(bubble_pos, bubble_max, IM_COL32(255, 255, 255, 255), bubble_rounding); 
        draw_list->AddRect(bubble_pos, bubble_max, IM_COL32(235, 235, 240, 255), bubble_rounding, 0, 1.0f);
        draw_list->AddCircleFilled(ImVec2(avatar_center.x, avatar_center.y + 2), avatar_radius, IM_COL32(0,0,0,30), 32);
        
        line_height = std::max(bubble_size.y, avatar_size) + item_spacing * 1.5f;
    }
    
    // 合并图层，推进整体界面游标
    draw_list->ChannelsMerge();
    ImGui::SetCursorScreenPos(ImVec2(start_cursor.x, start_cursor.y + line_height));
    ImGui::Dummy(ImVec2(0.0f, 0.0f)); 
}

void GameManager::renderUI() {
    // 设定全屏无边框主窗口
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.92f, 0.92f, 0.94f, 1.0f));
    ImGui::Begin("MainGameWindow", nullptr, window_flags);
    ImGui::PopStyleColor();

    // ================= 现代化侧边栏 =================
    float nav_width = 180.0f;
    ImGui::BeginChild("NavBar", ImVec2(nav_width, 0), true, ImGuiWindowFlags_NoScrollbar);
    
    ImGui::Spacing(); ImGui::Spacing();
    // 渐变感/设计感的 LOGO
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // 如果你有大号字体可以在这里切换
    ImGui::TextColored(ImVec4(0.96f, 0.54f, 0.64f, 1.0f), "  AI DATING SIM");
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "  VER 2.0");
    ImGui::PopFont();
    
    ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

    // 辅助函数：绘制带左侧指示条的高级侧边栏按钮
    auto DrawNavButton = [](const char* label, bool selected) -> bool {
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.2f, 0.5f));
        ImVec2 pos = ImGui::GetCursorScreenPos();
        bool clicked = ImGui::Selectable(label, selected, 0, ImVec2(0, 50));
        if (selected) {
            // 在选中项左侧画一条粉色强调线
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(pos.x, pos.y + 10), ImVec2(pos.x + 4, pos.y + 40), 
                IM_COL32(245, 138, 163, 255), 2.0f);
        }
        ImGui::PopStyleVar();
        return clicked;
    };

    if (DrawNavButton("聊天 (Chat)", currentTab == UINavTab::CHAT)) currentTab = UINavTab::CHAT;
    if (DrawNavButton("档案 (Profile)", currentTab == UINavTab::PROFILE)) currentTab = UINavTab::PROFILE;
    if (DrawNavButton("系统 (System)", currentTab == UINavTab::SYSTEM)) {
        currentTab = UINavTab::SYSTEM;
        scanSaveFiles();
    }

    ImGui::EndChild();
    ImGui::SameLine();

    // ================= 右侧主内容区 =================
    ImGui::BeginChild("MainContent", ImVec2(0, 0), false);

    if (currentTab == UINavTab::CHAT) {
        // --- 聊天 Header (玻璃拟物风) ---
        ImGui::BeginChild("ChatHeader", ImVec2(0, 60), true);
        std::string npcNameDisplay = activeNPC ? activeNPC->getName() : "未邂逅";
        std::string timeStr = (currentTime == TimeOfDay::MORNING) ? "清晨" : (currentTime == TimeOfDay::NOON) ? "午后" : "深夜";
        
        ImGui::SetCursorPos(ImVec2(20, 20));
        ImGui::TextDisabled("与");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f, 0.2f, 0.2f, 1.0f), "%s", npcNameDisplay.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("交谈中  ·  %s", timeStr.c_str());
        
        if (isWaitingForReply) { 
            ImGui::SameLine(ImGui::GetWindowWidth() - 180); 
            ImGui::TextColored(ImVec4(0.96f, 0.54f, 0.64f, 1.0f), "对方正在输入中..."); 
        }
        ImGui::EndChild();

        // --- 聊天主体区 ---
        // 动态计算底部输入区高度 (如果发生事件，需要更多空间放置选项按钮)
        float input_area_height = isEventActive ? 180.0f : 80.0f; // 动态输入框高度
        
        ImGui::BeginChild("ChatHistoryArea", ImVec2(0, -input_area_height), false, ImGuiWindowFlags_NoScrollWithMouse);
        
        // 绘制聊天背景图 (如果有)
        ImVec2 chatMin = ImGui::GetWindowPos();
        ImVec2 chatMax = ImVec2(chatMin.x + ImGui::GetWindowSize().x, chatMin.y + ImGui::GetWindowSize().y);
        if (chatBgLoader.isLoaded()) {
            ImGui::GetWindowDrawList()->AddImage((void*)(intptr_t)chatBgLoader.getTextureID(), chatMin, chatMax, ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255,220));
        } else {
            ImGui::GetWindowDrawList()->AddRectFilled(chatMin, chatMax, IM_COL32(245, 245, 248, 255)); // 浅灰背景兜底
        }

        // 内部嵌套一个可滚动的窗口用于文本排列
        ImGui::BeginChild("ChatScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::Spacing();
        
        for (const auto& chat : uiChatHistory) {
            int type = 0;
            ImTextureID tex = 0;

            if (chat.first == mainPlayer.getName()) {
                type = 1; // 玩家
                if (playerImageLoader.isLoaded()) {
                    tex = (ImTextureID)(intptr_t)playerImageLoader.getTextureID();
                }
            } else if (chat.first == "[暗中洞察]") {
                type = 3; // 洞察
            } else if (chat.first != "系统" && chat.first.find("GM") == std::string::npos) {
                type = 2; // NPC
                if (npcImageLoader.isLoaded()) {
                    tex = (ImTextureID)(intptr_t)npcImageLoader.getTextureID();
                }
            }
            
            drawChatBubble(chat.first, chat.second, type, tex);
        }

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
        ImGui::EndChild();

        // --- 聊天底部输入 / 事件检定区 ---
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f); // 输入区无圆角贴底
        ImGui::BeginChild("ChatInput", ImVec2(0, 0), true);
        ImGui::PopStyleVar();

        if (isEventActive) {
            ImGui::SetCursorPos(ImVec2(15, 15));
            ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "⚠️ 命运抉择时刻");
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
        } else if (activeNPC != nullptr && !hasEncounterStarted) {
            // =================未邂逅时的引导按钮 =================
            ImGui::Spacing(); ImGui::Spacing();
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImGui::SetCursorPosX((avail.x - 240) * 0.5f); // 居中计算
            
            if (isGeneratingEncounter) {
                ImGui::BeginDisabled(); 
                ImGui::Button("命运正在交汇...", ImVec2(240, 42)); 
                ImGui::EndDisabled();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.96f, 0.54f, 0.64f, 1.0f)); // 使用更粉嫩的高亮色
                if (ImGui::Button("开启我们的邂逅", ImVec2(240, 42))) {
                    isGeneratingEncounter = true;
                    futureEncounter = ProfileGenerator::generateEncounterAsync(std::string(worldSettingBuf), mainPlayer, currentNPC);
                }
                ImGui::PopStyleColor();
            } 
        } else {
            // 普通对话输入区
            ImGui::SetCursorPos(ImVec2(15, 15));
            static char inputBuf[512] = ""; 
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 140);
            
            // 放大输入框内边距
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16, 12));
            bool pressed = ImGui::InputTextWithHint("##ChatInput", "输入你想说的话...", inputBuf, IM_ARRAYSIZE(inputBuf), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PopStyleVar();
            
            ImGui::SameLine(0, 15);
            
            bool disableInput = isWaitingForReply || isGeneratingTransition || activeNPC == nullptr || !hasEncounterStarted;
            if (disableInput) ImGui::BeginDisabled();
            
            // 现代化大按钮
            if (ImGui::Button("发送 (Enter)", ImVec2(120, 42)) || (pressed && !disableInput)) {
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
        // ================= 档案页面现代化 (纵向卡片布局) =================
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::Indent(10.0f);
        ImGui::TextDisabled("DATABASE");
        ImGui::TextUnformatted("世界与灵魂档案");
        ImGui::Unindent(10.0f);
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        
        ImGui::BeginChild("ProfileContent", ImVec2(0, 0), false);

        float availWidth = ImGui::GetContentRegionAvail().x;
        float rightColWrapWidth = availWidth - 220.0f;

        // ---------------------------------------------------------
        // 1. 玩家档案卡片 (上方)
        // ---------------------------------------------------------
        float pLeftHeight = 200.0f; // 默认最小高度
        float pRenderWidth = 180.0f;
        if (playerImageLoader.isLoaded()) {
            float aspect = (float)playerImageLoader.getWidth() / (float)playerImageLoader.getHeight();
            pLeftHeight = pRenderWidth / aspect; // 动态计算等比立绘高度
        }
        
        // 动态预判右侧文字高度
        std::string pBackstory = "背景故事: " + mainPlayer.getBackstory();
        float pTextHeight = ImGui::CalcTextSize(pBackstory.c_str(), NULL, false, rightColWrapWidth).y;
        float pRightHeight = 220.0f + pTextHeight; // 基础元素高度(标题+按钮+行距) + 动态文字高度
        
        // 卡片高度取两边最高的那个，再额外加 40px 的上下内边距
        float playerCardHeight = std::max(pLeftHeight, pRightHeight) + 40.0f; 

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.94f, 0.96f, 1.0f, 0.4f)); 
        // 添加 ImGuiWindowFlags_NoScrollbar 彻底干掉丑陋的内部滚动条
        ImGui::BeginChild("PlayerProfileCard", ImVec2(0, playerCardHeight), true, ImGuiWindowFlags_NoScrollbar);
        
        ImGui::Columns(2, "PlayerSplit", false);
        ImGui::SetColumnWidth(0, 200.0f); // 固定左侧宽度

        // --- 玩家立绘 (左侧) ---
        if (isGeneratingPlayerPortrait) {
            ImGui::TextDisabled("\n\n\n  容貌重构中...");
        } else if (playerImageLoader.isLoaded()) {
            float offsetY = (playerCardHeight - pLeftHeight) * 0.5f - 10.0f;
            if (offsetY > 0) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);
            ImGui::Image((void*)(intptr_t)playerImageLoader.getTextureID(), ImVec2(pRenderWidth, pLeftHeight));
        } else {
            ImGui::TextDisabled("\n\n\n  暂无主角立绘");
        }

        ImGui::NextColumn();

        // --- 玩家资料 (右侧) ---
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 15.0f);
        ImGui::TextColored(ImVec4(0.4f, 0.6f, 0.9f, 1.0f), "■ 主角情报 (Player)");
        ImGui::Spacing();
        ImGui::Text("代号: %s", mainPlayer.getName().c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + rightColWrapWidth);
        ImGui::TextWrapped("背景故事: %s", mainPlayer.getBackstory().c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // ================== 现代表格排版 RPG 属性面板 ==================
        ImGui::TextColored(ImVec4(0.4f, 0.6f, 0.9f, 1.0f), "■ 基础能力 (Attributes)");
        ImGui::Spacing();
        
        // 辅助函数：根据数值自动计算字母评级
        auto getRank = [](int val) -> const char* {
            if (val >= 90) return "S";
            if (val >= 75) return "A";
            if (val >= 60) return "B";
            if (val >= 40) return "C";
            return "D";
        };

        // 使用现代的 Table API 代替 Columns，保证完美的两列对齐
        if (ImGui::BeginTable("PlayerStatsTable", 2, ImGuiTableFlags_None)) {
            
            // 核心渲染器
            auto drawSleekStat = [&](const char* label, int value, ImVec4 color) {
                ImGui::TextDisabled("%s", label);
                ImGui::SameLine(65);
                ImGui::TextColored(color, "%d", value);
                ImGui::SameLine(100);
                ImGui::TextColored(ImVec4(color.x, color.y, color.z, 0.7f), "[%s]", getRank(value));

                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                ImVec2 p = ImGui::GetCursorScreenPos();
                
                // 动态获取当前单元格的可用宽度
                float bar_width = ImGui::GetContentRegionAvail().x * 0.85f; 
                float bar_height = 6.0f; 
                float rounding = bar_height * 0.5f; 

                // 画底槽
                draw_list->AddRectFilled(p, ImVec2(p.x + bar_width, p.y + bar_height), IM_COL32(180, 180, 180, 40), rounding);

                // 画进度
                float fill_width = bar_width * (std::max(0, std::min(value, 100)) / 100.0f);
                ImU32 fill_col = ImGui::ColorConvertFloat4ToU32(color);
                draw_list->AddRectFilled(p, ImVec2(p.x + fill_width, p.y + bar_height), fill_col, rounding);

                // 画高光点
                if (fill_width > 0) {
                    draw_list->AddCircleFilled(ImVec2(p.x + fill_width, p.y + bar_height / 2.0f), 3.5f, IM_COL32(255, 255, 255, 220));
                }

                ImGui::Dummy(ImVec2(bar_width, bar_height + 12.0f)); 
            };

            // 第一行：体格 (左) | 智识 (右)
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            drawSleekStat("体格", mainPlayer.getPhysique(), ImVec4(0.95f, 0.45f, 0.45f, 1.0f)); 
            ImGui::TableSetColumnIndex(1);
            drawSleekStat("智识", mainPlayer.getIntellect(),ImVec4(0.45f, 0.65f, 0.95f, 1.0f));

            // 第二行：魅力 (左) | 财力 (右)
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            drawSleekStat("魅力", mainPlayer.getCharm(),    ImVec4(0.95f, 0.55f, 0.75f, 1.0f));    
            ImGui::TableSetColumnIndex(1);
            drawSleekStat("财力", mainPlayer.getWealth(),   ImVec4(0.95f, 0.75f, 0.25f, 1.0f));   

            // 第三行：共情 (左) | 运气 (右)
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            drawSleekStat("共情", mainPlayer.getEmpathy(),  ImVec4(0.35f, 0.85f, 0.65f, 1.0f));  
            ImGui::TableSetColumnIndex(1);
            drawSleekStat("运气", mainPlayer.getLuck(),     ImVec4(0.75f, 0.45f, 0.95f, 1.0f));     
            
            ImGui::EndTable();
        }
        ImGui::Spacing();
        
        if (isGeneratingPlayer) {
            ImGui::BeginDisabled(); ImGui::Button("正在重构人生...", ImVec2(140, 32)); ImGui::EndDisabled();
        } else {
            if (ImGui::Button("重塑人生", ImVec2(120, 32))) { 
                isGeneratingPlayer = true; 
                futurePlayerProfile = ProfileGenerator::generatePlayerProfileAsync(std::string(worldSettingBuf)); 
            }
        }
        ImGui::Columns(1);
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        // ---------------------------------------------------------
        // 2. NPC 档案卡片 (下方)
        // ---------------------------------------------------------
        float nLeftHeight = 200.0f;
        float nRenderWidth = 180.0f;
        if (npcImageLoader.isLoaded()) {
            float aspect = (float)npcImageLoader.getWidth() / (float)npcImageLoader.getHeight();
            nLeftHeight = nRenderWidth / aspect;
        }

        float nRightHeight = 100.0f; 
        std::string nCore = "核心性格: " + currentNPC.personality_core;
        std::string nTrauma = "隐藏执念: " + currentNPC.hidden_trauma;
        
        if (currentNPC.is_generated) {
            nRightHeight += 50.0f; // 进度条预留高度
            // 精确计算 AI 生成的变长文本所需的高度
            nRightHeight += ImGui::CalcTextSize(nCore.c_str(), NULL, false, rightColWrapWidth).y;
            nRightHeight += ImGui::CalcTextSize(nTrauma.c_str(), NULL, false, rightColWrapWidth).y;
        }
        
        float npcCardHeight = std::max(nLeftHeight, nRightHeight) + 50.0f;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 0.94f, 0.96f, 0.4f)); 
        ImGui::BeginChild("NPCProfileCard", ImVec2(0, npcCardHeight), true, ImGuiWindowFlags_NoScrollbar);
        
        ImGui::Columns(2, "NPCSplit", false);
        ImGui::SetColumnWidth(0, 200.0f);

        // --- NPC 立绘 (左侧) ---
        if (isGeneratingPortrait) {
            ImGui::TextDisabled("\n\n\n  画师描绘中...");
        } else if (npcImageLoader.isLoaded()) {
            float offsetY = (npcCardHeight - nLeftHeight) * 0.5f - 10.0f;
            if (offsetY > 0) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);
            ImGui::Image((void*)(intptr_t)npcImageLoader.getTextureID(), ImVec2(nRenderWidth, nLeftHeight));
        } else {
            ImGui::TextDisabled("\n\n\n  暂无角色立绘");
        }

        ImGui::NextColumn();

        // --- NPC 资料 (右侧) ---
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 15.0f);
        ImGui::TextColored(ImVec4(0.96f, 0.54f, 0.64f, 1.0f), "■ 邂逅对象 (NPC)");
        ImGui::Spacing();
        ImGui::Text("姓名: %s", currentNPC.is_generated ? currentNPC.name.c_str() : "未知");
        
        if (currentNPC.is_generated) {
            ImGui::Text("好感度:"); ImGui::SameLine();
            ImGui::ProgressBar(currentNPC.initial_affection / 100.0f, ImVec2(150, 18));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + rightColWrapWidth);
            ImGui::TextWrapped("核心性格: %s", currentNPC.personality_core.c_str());
            ImGui::TextWrapped("隐藏执念: %s", currentNPC.hidden_trauma.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
        }
        
        ImGui::Spacing();
        if (isGeneratingNPC) {
            ImGui::BeginDisabled(); ImGui::Button("正在寻找...", ImVec2(120, 32)); ImGui::EndDisabled();
        } else {
            if (ImGui::Button("邂逅新角色", ImVec2(120, 32))) { 
                isGeneratingNPC = true; 
                futureProfile = ProfileGenerator::generateRandomProfileAsync(std::string(worldSettingBuf), mainPlayer); 
            }
        }
        
        ImGui::Columns(1);
        ImGui::EndChild();
        ImGui::PopStyleColor();

        // ---------------------------------------------------------
        // 3. 世界观设置 (底部)
        // ---------------------------------------------------------
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "■ 世界线构造 (World Settings)");
        ImGui::InputTextMultiline("##WorldSetting", worldSettingBuf, IM_ARRAYSIZE(worldSettingBuf), ImVec2(-1, 80));
        if (isGeneratingWorld) {
            ImGui::BeginDisabled(); ImGui::Button("世界重构中...", ImVec2(140, 32)); ImGui::EndDisabled();
        } else {
            if (ImGui::Button("重构世界", ImVec2(140, 32))) { 
                isGeneratingWorld = true; 
                futureWorldSetting = ProfileGenerator::generateRandomWorldSettingAsync(); 
            }
        } 

        ImGui::EndChild();
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
    saveData["hasEncounterStarted"] = hasEncounterStarted;
    saveData["currentBgPath"] = currentBgPath;

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

        if (!mainPlayer.getPortraitPath().empty()) {
            playerImageLoader.LoadFromFile(mainPlayer.getPortraitPath());
        }
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
        activeNPC = std::make_shared<NPC>("Temp", "Temp", 20);
        activeNPC->fromJson(saveData["activeNPC"]);

        // 【图形学魔法】：根据 JSON 里的路径，重新把立绘塞回 OpenGL 显卡！
        std::string portraitPath = activeNPC->getPortraitPath();
        if (!portraitPath.empty()) {
            npcImageLoader.LoadFromFile(portraitPath);
        }
        hasEncounterStarted = saveData.value("hasEncounterStarted", true);
    }

    // 4. 恢复右侧 UI 的聊天气泡记录
    uiChatHistory.clear();
    if (saveData.contains("uiChatHistory")) {
        for (const auto& item : saveData["uiChatHistory"]) {
            uiChatHistory.push_back({item["speaker"], item["content"]});
        }
    }

    currentBgPath = saveData.value("currentBgPath", "");
    if (!currentBgPath.empty()) {
        chatBgLoader.LoadFromFile(currentBgPath);
    } else {
        chatBgLoader.Free(); // 如果旧存档没有背景，就清空画面
    }

    return true;
}
