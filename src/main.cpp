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
#include "GameManager.h" 
#include "Player.h"
#include "ConfigManager.h"
#include "NPC.h"
#include <vector>
#include "GameEvent.h"
#include <algorithm>
#include "ImageLoader.h"

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
    ConfigManager::getInstance().loadConfig("../config.json");
    
    // 1. GLFW 与 OpenGL 初始化
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "AI Dating Sim v2.0", nullptr, nullptr);
    if (window == nullptr) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // 2. ImGui 初始化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; 
    SetupImGuiStyle();
    const ImWchar* glyphRanges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 18.0f, NULL, glyphRanges);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    GameManager gameManager;
    gameManager.initGame();

    // 4. 游戏主循环 (将 gameManager.isGameRunning() 作为退出条件)
    while (!glfwWindowShouldClose(window) && gameManager.isGameRunning()) {
        glfwPollEvents();

        // 启动新的一帧
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        gameManager.runLoop();

        // 渲染与交换缓冲区
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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