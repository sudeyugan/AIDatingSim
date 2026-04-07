cmake_minimum_required(VERSION 3.10)
project(AIDatingSim VERSION 1.0)

# 设置 C++ 标准为 C++17 或以上
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 指定头文件搜索路径
include_directories(${CMAKE_SOURCE_DIR}/include)

if(MSVC)
    add_compile_options("/utf-8")
endif()

# 收集源文件
file(GLOB SOURCES "src/*.cpp")

# 生成可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

find_package(OpenSSL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
# 如果后续引入第三方库 (例如 curl 用于 API 请求)，取消下方注释
# find_package(CURL REQUIRED)
# target_link_libraries(${PROJECT_NAME} CURL::libcurl)