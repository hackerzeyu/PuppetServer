cmake_minimum_required(VERSION 3.16)
project(WebServer)

set(CMAKE_CXX_STANDARD 17)

include_directories(
        ${PROJECT_SOURCE_DIR}/Conf
        ${PROJECT_SOURCE_DIR}/Log
        ${PROJECT_SOURCE_DIR}/Logic
        ${PROJECT_SOURCE_DIR}/Tcp
        ${PROJECT_SOURCE_DIR}/ThreadPool
)

# 添加调试选项
set(CMAKE_BUILD_TYPE Debug)  # 设置构建类型为 Debug
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")  # 添加调试标志和关闭优化

file(GLOB_RECURSE SRC "Conf/*.cpp" "Log/*.cpp"  "Tcp/*.cpp"  "ThreadPool/*.cpp" "Logic/*.cpp")

add_executable(server main.cpp ${SRC})
target_link_libraries(server pthread jsoncpp)