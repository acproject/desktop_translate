# Desktop Translate

#### [English](README.md) |  [中文](README_ZH.md)

## 介绍

一个简单的桌面翻译工具，支持框选翻译、剪贴板翻译和悬浮气泡翻译。使用本地大模型 API（OpenAI 兼容）进行翻译。

## 功能特性

- **框选翻译**：框选屏幕任意区域的文本，自动 OCR 识别并翻译
- **剪贴板翻译**：自动翻译剪贴板中的文本
- **悬浮气泡翻译**：悬浮窗口，实时翻译 PRIMARY 选区/剪贴板文本
- **本地模型**：基于 llama.cpp，支持 OpenAI 兼容 API，数据不出本地
- **快捷键**：全局快捷键触发翻译，无需切换窗口
- **翻译结果气泡**：优雅的浮动气泡展示翻译结果，支持钉住和复制

## 截图

### 系统托盘菜单

![托盘菜单](assets/1.png)

### 翻译结果气泡

![翻译气泡](assets/2.png)

### 设置面板

![设置](assets/3.png)

### 关于

![关于](assets/4.png)

## 依赖

### Linux (Ubuntu/Debian)

```bash
sudo apt install cmake qt6-base-dev libcurl4-openssl-dev libx11-dev pkg-config
# 可选：用于框选文本捕获
sudo apt install xdotool xsel
```

### Windows

- Visual Studio 2022 (MSVC)
- CMake 3.20+
- Qt6 (通过 vcpkg 安装)
- libcurl (通过 vcpkg 安装)

## 构建

### Linux

```bash
# 默认构建（启用 CUDA）
./build.sh

# 禁用 CUDA 构建
DESKTOP_TRANSLATE_ENABLE_LLAMA_CUDA=OFF ./build.sh

# Debug 构建
./build_debug.sh
```

构建产物：
- `build/desktop_translate` — 主程序
- `build/bin/llama-server` — llama.cpp 推理服务

### Windows

```powershell
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### 打包 deb (Linux)

```bash
./build_deb.sh
# 或指定版本号
./build_deb.sh 1.0.7
```

生成的 deb 包位于 `dist/` 目录。

## 使用

### 启动

```bash
./build/desktop_translate
```

启动后程序运行在系统托盘中。

### 快捷键

| 快捷键 | 功能 |
|--------|------|
| `F7` | 框选翻译 |
| `F9` | 剪贴板翻译 |
| `Ctrl+F8` | 悬浮气泡翻译开关 |

快捷键可在设置面板中自定义。

### 托盘菜单

- **框选翻译 (S)**：进入框选模式，用鼠标框选需要翻译的文本区域
- **剪贴板翻译 (C)**：翻译当前剪贴板中的文本
- **显示测试窗口 (T)**：打开调试测试窗口
- **悬浮气泡翻译 (H)**：开启/关闭悬浮翻译气泡
- **设置 (O)**：打开设置面板
- **关于 (A)**：显示版本信息
- **退出 (X)**：退出程序

### 翻译结果气泡

翻译完成后，结果以气泡形式显示在选区附近：
- **钉住**：固定气泡，防止自动关闭
- **复制**：将译文复制到剪贴板
- **拖拽**：可拖动气泡到任意位置
- 未钉住的气泡会在 12 秒后自动关闭

## 配置

### 翻译 API 设置

| 配置项 | 说明 | 示例 |
|--------|------|------|
| API 地址 | 本地模型服务地址 | `http://127.0.0.1` |
| 端口 | 服务端口 | `8117` |
| 模型 | 模型名称 | `HY-MT1.5-1.8B-GGUF` |
| API Key | 认证密钥（可选） | |
| 超时时间 | 请求超时（秒） | `300` |

### OCR 服务设置

| 配置项 | 说明 | 示例 |
|--------|------|------|
| OCR 地址 | OCR 服务地址 | `http://127.0.0.1` |
| 端口 | OCR 服务端口 | `8111` |
| 模型 | OCR 模型名称 | `PaddleOCR-VL-1.5-GGUF` |
| API Key | 认证密钥（可选） | |

### 翻译设置

| 配置项 | 说明 | 可选值 |
|--------|------|--------|
| 源语言 | 原文语言 | `auto`（自动检测）, `en`, `zh`, `ja` 等 |
| 目标语言 | 译文语言 | `zh`, `en`, `ja`, `ko` 等 |

## 本地模型部署

本项目使用 llama.cpp 作为推理后端，支持 GGUF 格式的模型。

### 翻译模型

推荐使用支持 OpenAI 兼容 API 的翻译模型，例如：

```bash
# 使用 llama-server 启动翻译模型服务
./build/bin/llama-server \
  --model models/HY-MT1.5-1.8B-GGUF \
  --port 8117 \
  --host 127.0.0.1 \
  --ctx-size 4096
```

### OCR 模型

框选翻译功能需要 OCR 服务支持：

```bash
# 使用 llama-server 启动 OCR 模型服务
./build/bin/llama-server \
  --model models/PaddleOCR-VL-1.5-GGUF \
  --port 8111 \
  --host 127.0.0.1 \
  --ctx-size 4096
```

## 项目结构

```
desktop_translate/
── src/                    # 源代码
│   ├── main.cpp            # 入口
│   ├── MainWindow.cpp      # 主窗口/托盘
│   ├── HoverTranslateWindow.cpp  # 悬浮翻译窗口
│   ├── SelectionOverlay.cpp      # 框选覆盖层
│   ├── TranslationService.cpp    # 翻译服务
│   ├── TranslationResultWindow.cpp # 翻译结果气泡
│   ├── OCRService.cpp      # OCR 服务
│   ├── Config.cpp          # 配置管理
│   ├── ClipboardManager.cpp # 剪贴板管理
│   ├── GlobalShortcut.cpp  # 全局快捷键
│   ├── TestWindow.cpp      # 测试窗口
│   └── ModelServiceManager.cpp # 模型服务管理
├── include/                # 头文件
├── third_party/            # 第三方库
│   ├── json/               # nlohmann/json
│   └── llama.cpp/          # llama.cpp
├── Icons8/                 # 图标资源
├── assets/                 # 截图资源
├── CMakeLists.txt          # CMake 配置
├── build.sh                # Linux 构建脚本
├── build_debug.sh          # Debug 构建脚本
└── build_deb.sh            # deb 打包脚本
```

## 许可证

MIT
