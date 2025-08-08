# =============== 项目及C语言标准配置 ===============
# 1. 指定 CMake 的最低版本要求
cmake_minimum_required(VERSION 3.10)
# 2. 设置工具链路径
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../toolchain.cmake")
# 3. 明确指定为 C 项目
project(lab2 C)
# 4. 明确指定源文件
add_executable(lab2 src/main2.c)
# 5. C 语言标准配置
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# =============== 跨平台关键配置 ===============
# 1. 添加头文件搜索路径，动态获取 sysroot（避免硬编码路径）
target_include_directories(lab2 SYSTEM PRIVATE
    ${CMAKE_FIND_ROOT_PATH}/usr/include
)

# 2. 设置 OpenWRT 环境变量（兼容 OpenWRT 构建系统）
set(ENV{STAGING_DIR} ${STAGING_DIR})  # 使用 toolchain.cmake 中的变量

# 3. 智能链接库配置
find_library(PCAP_LIB pcap HINTS ${CMAKE_FIND_ROOT_PATH}/usr/lib)
target_link_libraries(lab2 PRIVATE 
    ${PCAP_LIB} 
    pthread
)