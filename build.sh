#!/bin/bash

# 桌面翻译软件构建脚本

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

BUILD_DIR="${BUILD_DIR:-build}"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
DESKTOP_TRANSLATE_BUILD_LLAMA="${DESKTOP_TRANSLATE_BUILD_LLAMA:-ON}"
DESKTOP_TRANSLATE_ENABLE_LLAMA_CUDA="${DESKTOP_TRANSLATE_ENABLE_LLAMA_CUDA:-ON}"

echo -e "${GREEN}=== 桌面翻译软件构建脚本 ===${NC}"

echo -e "${YELLOW}检查依赖...${NC}"

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

if [ "$DESKTOP_TRANSLATE_ENABLE_LLAMA_CUDA" = "ON" ] && ! command -v nvcc &> /dev/null; then
    echo -e "${RED}错误: 默认启用了 llama.cpp CUDA 构建，但未找到 nvcc${NC}"
    echo "可安装 CUDA Toolkit，或临时执行: DESKTOP_TRANSLATE_ENABLE_LLAMA_CUDA=OFF ./build.sh"
    exit 1
fi

echo -e "${GREEN}依赖检查通过${NC}"

if command -v git &> /dev/null; then
    echo -e "${YELLOW}同步子模块...${NC}"
    git submodule update --init --recursive
fi

if [ -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}清理旧的构建目录...${NC}"
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo -e "${YELLOW}配置项目...${NC}"
cmake_args=(
    ..
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-DDESKTOP_TRANSLATE_BUILD_LLAMA=${DESKTOP_TRANSLATE_BUILD_LLAMA}"
    "-DDESKTOP_TRANSLATE_ENABLE_LLAMA_CUDA=${DESKTOP_TRANSLATE_ENABLE_LLAMA_CUDA}"
    "-DLLAMA_BUILD_COMMON=ON"
    "-DLLAMA_BUILD_TOOLS=ON"
    "-DLLAMA_BUILD_SERVER=ON"
    "-DLLAMA_BUILD_WEBUI=OFF"
    "-DLLAMA_BUILD_TESTS=OFF"
    "-DLLAMA_BUILD_EXAMPLES=OFF"
)

if [ -n "${CMAKE_CUDA_ARCHITECTURES:-}" ]; then
    cmake_args+=("-DCMAKE_CUDA_ARCHITECTURES=${CMAKE_CUDA_ARCHITECTURES}")
fi

cmake "${cmake_args[@]}"

echo -e "${YELLOW}构建项目...${NC}"
cmake --build . -j$(nproc)

echo -e "${GREEN}构建完成！${NC}"
echo ""
echo "可执行文件位置: $BUILD_DIR/desktop_translate"
echo "llama-server: $BUILD_DIR/bin/llama-server"
echo "llama.cpp: $DESKTOP_TRANSLATE_BUILD_LLAMA"
echo "llama.cpp CUDA: $DESKTOP_TRANSLATE_ENABLE_LLAMA_CUDA"
echo ""
echo "运行方式:"
echo "  ./build/desktop_translate"
echo ""
echo "可选依赖（用于框选文本捕获）:"
echo "  sudo apt install xdotool xsel"
