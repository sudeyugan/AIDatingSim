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
    style.WindowRounding    = 0.0f; // 全屏窗口不需要圆角
    style.ChildRounding     = 8.0f;
    style.FrameRounding     = 6.0f;
    style.ItemSpacing       = ImVec2(10, 10);
    style.FramePadding      = ImVec2(12, 8);
    style.WindowPadding     = ImVec2(12, 12);
    style.WindowBorderSize  = 0.0f;

    ImGui::StyleColorsLight();
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]       = ImVec4(0.98f, 0.98f, 0.99f, 1.00f); 
    colors[ImGuiCol_ChildBg]        = ImVec4(1.00f, 1.00f, 1.00f, 1.00f); 
    colors[ImGuiCol_Border]         = ImVec4(0.90f, 0.90f, 0.92f, 1.00f); 
    colors[ImGuiCol_Text]           = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_Button]         = ImVec4(0.90f, 0.90f, 0.95f, 1.00f); 
    colors[ImGuiCol_ButtonHovered]  = ImVec4(0.85f, 0.85f, 0.90f, 1.00f); 
    colors[ImGuiCol_ButtonActive]   = ImVec4(0.80f, 0.80f, 0.85f, 1.00f); 
    
    // 侧边栏选中项颜色 (樱粉色)
    colors[ImGuiCol_Header]         = ImVec4(0.96f, 0.64f, 0.69f, 0.50f);
    colors[ImGuiCol_HeaderHovered]  = ImVec4(0.96f, 0.64f, 0.69f, 0.80f);
    colors[ImGuiCol_HeaderActive]   = ImVec4(0.96f, 0.64f, 0.69f, 1.00f);
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