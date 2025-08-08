#!/bin/bash
# 用户指定 SDK 路径
SDK_PATH="${1:-/home/lzj/code/openwrt/openwrt-sdk/openwrt-sdk-24.10.0}"
export OPENWRT_SDK="$SDK_PATH"

# 验证 SDK 路径
[ -d "$SDK_PATH/staging_dir" ] || {
    echo "错误：未找到 SDK 目录 $SDK_PATH"
    return 2>/dev/null || exit 1
}

# 查找最新工具链
TOOLCHAIN_DIR=$(find "$SDK_PATH/staging_dir" -maxdepth 1 -type d -name "toolchain-x86_64_gcc-*_musl" | sort -V | tail -n1)
[ -z "$TOOLCHAIN_DIR" ] && {
    echo "错误：未找到工具链目录"
    return 2>/dev/null || exit 1
}

# 设置核心环境
export STAGING_DIR="$SDK_PATH/staging_dir"
export TARGET="x86_64-openwrt-linux-musl"
export PATH="$TOOLCHAIN_DIR/bin:$PATH"
CC="$TOOLCHAIN_DIR/bin/${TARGET}-gcc"

# 显示配置
echo "--------------------------------------------"
echo "SDK 路径: $SDK_PATH"
echo "工具链  : $(basename $TOOLCHAIN_DIR)"
echo "目标平台: $TARGET"
echo "编译器  : $CC"
echo "版本    : $($CC --version | head -n1)"
echo "--------------------------------------------"