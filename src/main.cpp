#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#define NOMINMAX 
#include <windows.h>
#include <future>   
#include <chrono>
#include "ProfileGenerator.h" 
#include "Player.h"
#include "ConfigManager.h"
#include "NPC.h"
#include <vector>
#include "GameEvent.h"
#include <algorithm>

void SetupImGuiStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    // 1. 圆角设置 (保持大圆角，增加可爱的果冻感)
    style.WindowRounding    = 12.0f;
    style.ChildRounding     = 8.0f;
    style.FrameRounding     = 6.0f;
    style.PopupRounding     = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding      = 6.0f;

    // 2. 边框与间距 (拉开间距，增加画面的“空气感”)
    style.WindowBorderSize  = 0.0f;
    style.ChildBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.ItemSpacing       = ImVec2(12, 12);
    style.FramePadding      = ImVec2(10, 8);
    style.WindowPadding     = ImVec2(16, 16);

    // 3. 强制使用浅色底色基准
    ImGui::StyleColorsLight();

    // 4. 自定义调色板 (暖白底 + 樱粉色交互 + 咖色文字)
    ImVec4* colors = style.Colors;
    
    // 背景色：极浅的暖白/樱粉底，带有通透感
    colors[ImGuiCol_WindowBg]       = ImVec4(0.98f, 0.96f, 0.96f, 1.00f); 
    colors[ImGuiCol_ChildBg]        = ImVec4(1.00f, 1.00f, 1.00f, 0.85f); // 子区域用纯白，略带透明度
    colors[ImGuiCol_Border]         = ImVec4(0.90f, 0.85f, 0.85f, 0.80f); // 极淡的粉灰边框
    
    // 文本颜色：深灰偏棕，阅读舒适且不突兀
    colors[ImGuiCol_Text]           = ImVec4(0.35f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_TextDisabled]   = ImVec4(0.65f, 0.60f, 0.60f, 1.00f);

    // 输入框背景：轻微的灰粉色
    colors[ImGuiCol_FrameBg]        = ImVec4(0.95f, 0.92f, 0.92f, 1.00f); 
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.90f, 0.85f, 0.85f, 1.00f);
    colors[ImGuiCol_FrameBgActive]  = ImVec4(0.85f, 0.80f, 0.80f, 1.00f);
    
    // 核心交互元素 (按钮、进度条) - 樱花粉色
    colors[ImGuiCol_Button]         = ImVec4(0.96f, 0.64f, 0.69f, 0.80f); // 柔和的樱粉色
    colors[ImGuiCol_ButtonHovered]  = ImVec4(0.96f, 0.64f, 0.69f, 1.00f); // 鼠标悬停
    colors[ImGuiCol_ButtonActive]   = ImVec4(0.90f, 0.55f, 0.60f, 1.00f); // 点击状态
    
    colors[ImGuiCol_CheckMark]      = ImVec4(0.96f, 0.64f, 0.69f, 1.00f);
    colors[ImGuiCol_SliderGrab]     = ImVec4(0.96f, 0.64f, 0.69f, 0.80f);
    colors[ImGuiCol_SliderGrabActive]= ImVec4(0.90f, 0.55f, 0.60f, 1.00f);

    // 滚动条：保持极简和清透
    colors[ImGuiCol_ScrollbarBg]    = ImVec4(0.98f, 0.96f, 0.96f, 0.00f); // 背景完全透明
    colors[ImGuiCol_ScrollbarGrab]  = ImVec4(0.85f, 0.80f, 0.80f, 0.60f); // 淡淡的抓手
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.75f, 0.70f, 0.70f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.65f, 0.60f, 0.60f, 1.00f);
}

// GLFW 错误回调函数
static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);

    if (!ConfigManager::getInstance().loadConfig("../config.json")) {
        std::cerr << "[警告] 配置加载失败，请检查工程目录下是否存在 config.json" << std::endl;
    }

    // 1. 初始化 GLFW
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    // 设定 OpenGL 版本 (3.0+)
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // 2. 创建窗口
    GLFWwindow* window = glfwCreateWindow(1280, 720, "AI Dating Sim v2.0", nullptr, nullptr);
    if (window == nullptr) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // 开启垂直同步 (VSync)

    // 3. 初始化 Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // 开启键盘控制
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; 

    SetupImGuiStyle();

    // 获取支持简体中文的 Unicode 字符范围
    const ImWchar* glyphRanges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    
    // 这里为了让你能最快跑通，直接调用 Windows 自带的“微软雅黑”字体 (假设你是 Windows 系统)
    // 18.0f 是字体大小，可以自己调
    ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 18.0f, NULL, glyphRanges);
    
    if (font == nullptr) {
        std::cerr << "警告：中文字体加载失败！ImGui 将降级使用默认英文字体。" << std::endl;
    }

    // 初始化后端
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // ==========================================
    Player mainPlayer("主角");
    CharacterProfile currentNPC;                  // 当前的目标角色
    std::future<CharacterProfile> futureProfile;  // 用于接收异步生成的结果
    bool isGeneratingNPC = false;                 // 标记是否正在生成中

    std::unique_ptr<NPC> activeNPC = nullptr;       // 真正负责对话的 NPC 实例
    std::vector<std::pair<std::string, std::string>> uiChatHistory; // UI 显示用的聊天记录：<发言者, 内容>
    std::future<NPCResponse> futureReply;       // 异步等待 AI 的回复
    bool isWaitingForReply = false;                 // 是否正在等待 AI 回复

    // 重置人生的状态变量
    std::future<std::pair<std::string, std::string>> futurePlayerProfile;
    bool isGeneratingPlayer = false;

    // 世界观相关变量
    static char worldSettingBuf[256] = "现代日常都市"; // 支持玩家手动输入
    std::future<std::string> futureWorldSetting;
    bool isGeneratingWorld = false;

    //  事件系统状态
    std::future<GameEvent> futureEvent;
    bool isGeneratingEvent = false;
    GameEvent currentEvent;
    bool isEventActive = false;
    std::future<std::string> futureEncounter;
    bool isGeneratingEncounter = false;
    bool hasEncounterStarted = false;

    // 4. 游戏主循环 (Render Loop)
    while (!glfwWindowShouldClose(window)) {
        // 处理各种输入事件 (鼠标、键盘)
        glfwPollEvents();

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
                
                uiChatHistory.clear();
                hasEncounterStarted = false;
                
                // 不直接开聊，而是呼叫 GM 创作相遇场景！
                isGeneratingEncounter = true;
                futureEncounter = ProfileGenerator::generateEncounterAsync(std::string(worldSettingBuf), mainPlayer, currentNPC);
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
                
                if (activeNPC != nullptr) {
                    // 既然主角变了，世界线变动，触发全新的相遇场景！
                    isGeneratingEncounter = true;
                    futureEncounter = ProfileGenerator::generateEncounterAsync(std::string(worldSettingBuf), mainPlayer, currentNPC);
                } else {
                    uiChatHistory.push_back({"系统", "命运的齿轮转动，你以新身份降临。请点击下方邂逅新角色。"});
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

        // 启动 ImGui 的新一帧
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 1. 让主窗口填满整个 GLFW 窗口，且去除系统的标题栏和边框
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
        
        ImGui::Begin("MainGameWindow", nullptr, window_flags);

        // 提前计算底部交互区需要的高度 (比如 3 倍的常规行高)
        float bottom_height = ImGui::GetFrameHeightWithSpacing() * 3.0f;

        // ----------------------------------------------------
        // 区域 A：左侧视觉区 (占据 60% 宽度，高度减去底部交互区)
        // ----------------------------------------------------
        ImGui::BeginChild("SceneArea", ImVec2(ImGui::GetContentRegionAvail().x * 0.6f, ImGui::GetContentRegionAvail().y - bottom_height), true);
        
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "【场景可视化区域】");
        ImGui::Text("这里未来将渲染 OpenGL 纹理（生图 API 返回的图片）。");
        ImGui::Text("目前角色：正在等待加载...");
        
        // 留白占位
        ImGui::Dummy(ImVec2(0.0f, 100.0f)); 
        ImGui::TextDisabled("(立绘占位符)");

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
                    futureReply = std::async(std::launch::async, [&activeNPC, chosenAction, &mainPlayer]() {
                        // 我们在用户文本前加一个前缀，让 NPC 意识到这是一个行动而不是说话
                        std::string contextualInput = "（我采取了行动：" + chosenAction + "，请根据你的设定做出反应）";
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
                    futureReply = std::async(std::launch::async, [&activeNPC, userText, &mainPlayer]() {
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
        // ==============================

        // 渲染
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        
        // 设置背景颜色 (深灰色)
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // 交换缓冲区
        glfwSwapBuffers(window);
    }

    // 5. 退出清理
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}