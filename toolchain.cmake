# 从环境变量获取路径
if(NOT DEFINED ENV{OPENWRT_SDK})
    message(FATAL_ERROR "请先运行 env.sh 设置环境变量")
endif()

#目标平台设置
set(CMAKE_SYSTEM_NAME Linux)       # 目标系统为 Linux
set(CMAKE_SYSTEM_PROCESSOR x86_64) # 目标 CPU 架构为 x86_64

#OpenWrt SDK 路径设置
set(OPENWRT_SDK $ENV{OPENWRT_SDK})
set(STAGING_DIR "${OPENWRT_SDK}/staging_dir")

# 设置交叉编译器 
set(CMAKE_C_COMPILER ${STAGING_DIR}/toolchain-x86_64_gcc-13.3.0_musl/bin/x86_64-openwrt-linux-gcc) 
set(CMAKE_CXX_COMPILER ${STAGING_DIR}/toolchain-x86_64_gcc-13.3.0_musl/bin/x86_64-openwrt-linux-g++)

# 设置sysroot 
set(CMAKE_FIND_ROOT_PATH ${STAGING_DIR}/target-x86_64_musl)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER) 
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY) 
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)