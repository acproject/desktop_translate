#!/bin/bash

# 桌面翻译软件构建脚本

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== 桌面翻译软件构建脚本 ===${NC}"

# 检查依赖
echo -e "${YELLOW}检查依赖...${NC}"

# 检查 CMake
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}错误: 未找到 cmake，请先安装 cmake${NC}"
    echo "Ubuntu/Debian: sudo apt install cmake"
    exit 1
fi

# 检查 Qt6
if ! pkg-config --exists Qt6Widgets 2>/dev/null; then
    echo -e "${RED}错误: 未找到 Qt6，请先安装 Qt6${NC}"
    echo "Ubuntu/Debian: sudo apt install qt6-base-dev"
    exit 1
fi

# 检查 libcurl
if ! pkg-config --exists libcurl 2>/dev/null; then
    echo -e "${RED}错误: 未找到 libcurl，请先安装 libcurl${NC}"
    echo "Ubuntu/Debian: sudo apt install libcurl4-openssl-dev"
    exit 1
fi

echo -e "${GREEN}依赖检查通过${NC}"

# 创建构建目录
BUILD_DIR="build"
if [ -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}清理旧的构建目录...${NC}"
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 配置项目
echo -e "${YELLOW}配置项目...${NC}"
cmake .. -DCMAKE_BUILD_TYPE=Release

# 构建
echo -e "${YELLOW}构建项目...${NC}"
cmake --build . -j$(nproc)

echo -e "${GREEN}构建完成！${NC}"
echo ""
echo "可执行文件位置: $BUILD_DIR/desktop_translate"
echo ""
echo "运行方式:"
echo "  ./build/desktop_translate"
echo ""
echo "可选依赖（用于框选文本捕获）:"
echo "  sudo apt install xdotool xsel"
