#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <windows.h>
#include <future>   
#include <chrono>
#include "ProfileGenerator.h" 
#include "Player.h"

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

    // 4. 游戏主循环 (Render Loop)
    while (!glfwWindowShouldClose(window)) {
        // 处理各种输入事件 (鼠标、键盘)
        glfwPollEvents();

        if (isGeneratingNPC && futureProfile.valid()) {
            // wait_for 设置为 0 秒，意味着“只看一眼结果有没有好，没好就立刻继续往下走”
            if (futureProfile.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                currentNPC = futureProfile.get(); // 获取生成的角色档案
                isGeneratingNPC = false;          // 结束生成状态
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
        
        // 玩家与时间状态面板
        ImGui::TextColored(ImVec4(0.3f, 0.7f, 0.9f, 1.0f), "--- 你的状态 ---");
        ImGui::Text("❤️魅力: 10  📖才智: 10  💰财富: 10");
        ImGui::Text("🕒当前时间: 早上");
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
            ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.5f, 1.0f), "【未知的执念/秘密】: %s", currentNPC.hidden_trauma.c_str());
        } else {
            ImGui::Text("暂无角色。请点击下方按钮邂逅新的缘分。");
        }
        ImGui::Separator();
        // 聊天记录区 (独立子窗口，支持滚动)
        ImGui::Text("【对话记录】");
        ImGui::BeginChild("ChatHistory", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        
        // 假数据测试排版
        ImGui::TextWrapped("系统: 游戏初始化成功，你来到了一个陌生的十字路口。");
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "你: 这是一个什么地方？");
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.6f, 1.0f), "NPC: 连这里都不认识，真不知道你是怎么进来的。");
        ImGui::Spacing();
        
        // 自动滚动到最底部的逻辑预留
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
        
        // 功能按钮排布
        // 如果正在生成，按钮变成灰色禁用状态
        if (isGeneratingNPC) {
            ImGui::BeginDisabled();
            ImGui::Button("生成中...", ImVec2(100, 0));
            ImGui::EndDisabled();
        } else {
            if (ImGui::Button("邂逅新角色", ImVec2(100, 0))) {
                isGeneratingNPC = true;
                // 发起异步请求，不会阻塞主线程
                futureProfile = ProfileGenerator::generateRandomProfileAsync();
            }
        }
        ImGui::SameLine();
        ImGui::SameLine();
        if (ImGui::Button("打工 (+财富)", ImVec2(100, 0))) {
            std::cout << "触发：打工" << std::endl;
        }
        ImGui::SameLine();
        if (ImGui::Button("读书 (+才智)", ImVec2(100, 0))) {
            std::cout << "触发：读书" << std::endl;
        }

        ImGui::Spacing();

        // 对话输入框
        static char inputBuf[512] = ""; // 用于存储玩家输入的字符数组
        // 让输入框占据大部分宽度，右边留点位置给发送按钮
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100); 
        
        // ImGuiInputTextFlags_EnterReturnsTrue 让我们按回车键也能发送
        bool isEnterPressed = ImGui::InputText("##ChatInput", inputBuf, IM_ARRAYSIZE(inputBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        
        ImGui::SameLine();
        if (ImGui::Button("发送", ImVec2(80, 0)) || isEnterPressed) {
            if (strlen(inputBuf) > 0) {
                std::cout << "玩家发送: " << inputBuf << std::endl;
                // TODO: 这里将来要调用 NPC 的 interact 逻辑
                
                inputBuf[0] = '\0'; // 发送后清空输入框
                
                // 让输入框重新获得焦点，方便连续打字
                ImGui::SetKeyboardFocusHere(-1); 
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