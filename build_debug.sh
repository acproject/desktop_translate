#!/bin/bash

# 快速开发构建脚本（Debug模式）

set -e

BUILD_DIR="build-debug"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(nproc)

echo ""
echo "Debug build completed: $BUILD_DIR/desktop_translate"
