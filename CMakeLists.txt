cmake_minimum_required(VERSION 3.20)
project(desktop_translate VERSION 1.0.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6 路径设置（如果需要）
# set(CMAKE_PREFIX_PATH "/path/to/Qt/6.x.x/gcc_64")

# 查找依赖
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Network)
find_package(CURL REQUIRED)
find_package(X11 REQUIRED)

# nlohmann_json - 使用 FetchContent 自动下载
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# 源文件
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/HoverTranslateWindow.cpp
    src/SelectionOverlay.cpp
    src/TranslationService.cpp
    src/TranslationResultWindow.cpp
    src/Config.cpp
    src/ClipboardManager.cpp
    src/TestWindow.cpp
    src/GlobalShortcut.cpp
    src/OCRService.cpp
)

set(HEADERS
    include/MainWindow.h
    include/HoverTranslateWindow.h
    include/SelectionOverlay.h
    include/TranslationService.h
    include/TranslationResultWindow.h
    include/Config.h
    include/ClipboardManager.h
    include/TestWindow.h
    include/GlobalShortcut.h
    include/OCRService.h
)

# 可执行文件
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
)

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    DESKTOP_TRANSLATE_SOURCE_ICON="${CMAKE_CURRENT_SOURCE_DIR}/Icons8/icons8-translate-100.png"
)

# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Network
    CURL::libcurl
    nlohmann_json::nlohmann_json
    ${X11_LIBRARIES}
)

# 编译选项
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
)

# 安装
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Icons8/icons8-translate-100.png
    DESTINATION share/pixmaps
    RENAME desktop-translate.png
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Icons8/icons8-translate-100.png
    DESTINATION share/icons/hicolor/128x128/apps
    RENAME desktop-translate.png
)
