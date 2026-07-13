@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

REM ==========================================================
REM  Windows 字典功能构建脚本
REM  - 假定 vcpkg 已安装在 D:\vcpkg
REM  - 假定 Qt6 已通过 vcpkg 安装
REM  - 自动跳过 CUDA，仅构建 CPU 版 llama.cpp
REM ==========================================================

set "PROJECT_ROOT=%~dp0"
set "BUILD_DIR=%PROJECT_ROOT%build"
set "VCPKG_ROOT=D:\vcpkg"
set "CMAKE_TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"

echo.
echo === Windows 字典功能构建脚本 ===
echo.

REM 依赖检查
where cmake >nul 2>nul
if errorlevel 1 (
    echo [错误] 未找到 cmake，请先安装 CMake 并加入 PATH
    pause
    exit /b 1
)

if not exist "%CMAKE_TOOLCHAIN%" (
    echo [错误] 未找到 vcpkg toolchain: %CMAKE_TOOLCHAIN%
    echo        请确认 vcpkg 已安装，或修改本脚本顶部的 VCPKG_ROOT
    pause
    exit /b 1
)

REM 同步子模块
where git >nul 2>nul
if not errorlevel 1 (
    echo [信息] 同步子模块...
    git submodule update --init --recursive
)

REM 清理并重新配置
if exist "%BUILD_DIR%" (
    echo [信息] 清理旧构建目录: %BUILD_DIR%
    rmdir /s /q "%BUILD_DIR%"
)
mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

echo.
echo [信息] 配置 CMake (CPU only)...
cmake "%PROJECT_ROOT%" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_TOOLCHAIN_FILE="%CMAKE_TOOLCHAIN%" ^
    -DVCPKG_TARGET_TRIPLET=x64-windows ^
    -DDESKTOP_TRANSLATE_BUILD_LLAMA=ON ^
    -DDESKTOP_TRANSLATE_ENABLE_LLAMA_CUDA=OFF ^
    -DLLAMA_BUILD_COMMON=ON ^
    -DLLAMA_BUILD_TOOLS=ON ^
    -DLLAMA_BUILD_SERVER=ON ^
    -DLLAMA_BUILD_WEBUI=OFF ^
    -DLLAMA_BUILD_TESTS=OFF ^
    -DLLAMA_BUILD_EXAMPLES=OFF
if errorlevel 1 (
    echo [错误] CMake 配置失败
    pause
    exit /b 1
)

echo.
echo [信息] 编译 (Release)...
cmake --build . --config Release -j
if errorlevel 1 (
    echo [错误] 编译失败
    pause
    exit /b 1
)

echo.
echo === 构建成功 ===
echo.
echo 可执行文件: %BUILD_DIR%\Release\desktop_translate.exe
echo llama-server: %BUILD_DIR%\bin\Release\llama-server.exe
echo.
echo 启动方式 (强制 CPU 推理):
echo   "%PROJECT_ROOT%翻译_CPU.cmd"
echo.
echo 启动后请:
echo   1. 在设置面板中确认翻译服务地址/端口/模型
echo   2. 启动本地翻译模型服务 (llama-server.exe)
echo   3. 点击托盘菜单 "字典查询 (D)" 或按 Ctrl+F5 测试
echo.
pause
endlocal
