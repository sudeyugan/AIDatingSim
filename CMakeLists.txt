cmake_minimum_required(VERSION 3.14)
project(AIDatingSim VERSION 2.0) # 升级到 2.0 啦！

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 1. 自动下载并配置 GLFW 库
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8 # 稳定版本
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# 2. 收集 ImGui 的源文件
file(GLOB IMGUI_SOURCES 
    "vendor/imgui/*.cpp"
)

# 3. 收集我们自己游戏的源文件
file(GLOB SOURCES "src/*.cpp")

# 4. 指定头文件搜索路径
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/vendor/imgui # 让项目能找到 imgui.h
    ${CMAKE_SOURCE_DIR}/vendor/stb_image
)

if(MSVC)
    add_compile_options("/utf-8")
endif()

# 5. 生成可执行文件 (包含游戏代码 + ImGui代码)
add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES})

# 6. 链接依赖库
# 链接 GLFW 和 OpenGL
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
elseif(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework OpenGL")
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE GL)
endif()

# 链接 OpenSSL (你之前保留的)
find_package(OpenSSL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)