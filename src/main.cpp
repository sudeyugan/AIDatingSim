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
    
    // 1. 全局圆角与边距设计 (现代化大圆角)
    style.WindowRounding    = 0.0f;  // 主窗口无圆角
    style.ChildRounding     = 12.0f; // 子窗口大圆角 (卡片感)
    style.FrameRounding     = 8.0f;  // 按钮和输入框圆角
    style.PopupRounding     = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding      = 8.0f;

    style.ItemSpacing       = ImVec2(12, 12); // 增加呼吸感
    style.FramePadding      = ImVec2(16, 10); // 更宽敞的按钮和输入框
    style.WindowPadding     = ImVec2(20, 20);
    style.WindowBorderSize  = 0.0f;  // 移除生硬的边框
    style.ChildBorderSize   = 1.0f;  // 保留极其轻微的卡片边框

    // 2. 现代高级浅色调色板 (樱粉+莫兰迪灰)
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]       = ImVec4(0.96f, 0.96f, 0.97f, 1.00f); // 高级灰白背景
    colors[ImGuiCol_ChildBg]        = ImVec4(1.00f, 1.00f, 1.00f, 0.85f); // 半透明纯白卡片
    colors[ImGuiCol_Border]         = ImVec4(0.88f, 0.88f, 0.90f, 0.60f); // 极淡的边框
    colors[ImGuiCol_BorderShadow]   = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    
    // 文字颜色 (主次分明)
    colors[ImGuiCol_Text]           = ImVec4(0.15f, 0.15f, 0.18f, 1.00f); // 深灰代替纯黑，更护眼
    colors[ImGuiCol_TextDisabled]   = ImVec4(0.55f, 0.55f, 0.60f, 1.00f);
    
    // 基础控件颜色 (输入框等)
    colors[ImGuiCol_FrameBg]        = ImVec4(0.94f, 0.94f, 0.96f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.92f, 0.92f, 0.95f, 1.00f);
    colors[ImGuiCol_FrameBgActive]  = ImVec4(0.90f, 0.90f, 0.93f, 1.00f);

    // 强调色 (樱粉系) - 用于按钮和选中状态
    ImVec4 accent         = ImVec4(0.96f, 0.54f, 0.64f, 1.00f);
    ImVec4 accentHovered  = ImVec4(0.98f, 0.60f, 0.70f, 1.00f);
    ImVec4 accentActive   = ImVec4(0.92f, 0.48f, 0.58f, 1.00f);

    colors[ImGuiCol_Button]         = accent; 
    colors[ImGuiCol_ButtonHovered]  = accentHovered; 
    colors[ImGuiCol_ButtonActive]   = accentActive; 
    
    // 侧边栏及高亮
    colors[ImGuiCol_Header]         = ImVec4(0.96f, 0.54f, 0.64f, 0.15f);
    colors[ImGuiCol_HeaderHovered]  = ImVec4(0.96f, 0.54f, 0.64f, 0.25f);
    colors[ImGuiCol_HeaderActive]   = ImVec4(0.96f, 0.54f, 0.64f, 0.40f);

    // 滚动条极其克制
    colors[ImGuiCol_ScrollbarBg]    = ImVec4(0.98f, 0.98f, 0.98f, 0.50f);
    colors[ImGuiCol_ScrollbarGrab]  = ImVec4(0.85f, 0.85f, 0.88f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.75f, 0.75f, 0.78f, 1.00f);
}

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    ConfigManager::getInstance().loadConfig("../config.json");
    
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // 修改为类似手机/平板的默认竖屏比例，当然全屏也能自适应
    GLFWwindow* window = glfwCreateWindow(1024, 768, "AI Dating Sim v2.0", nullptr, nullptr);
    if (window == nullptr) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; 
    SetupImGuiStyle();
    
    const ImWchar* glyphRanges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 20.0f, NULL, glyphRanges);
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    GameManager gameManager;
    gameManager.initGame();

    while (!glfwWindowShouldClose(window) && gameManager.isGameRunning()) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        gameManager.runLoop();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}