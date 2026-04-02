cmake_minimum_required(VERSION 3.20)

if(WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(_default_vcpkg_toolchain "D:/vcpkg/scripts/buildsystems/vcpkg.cmake")
    if(EXISTS "${_default_vcpkg_toolchain}")
        set(CMAKE_TOOLCHAIN_FILE "${_default_vcpkg_toolchain}" CACHE STRING "")
    endif()
endif()

if(WIN32 AND NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "")
endif()

if(WIN32)
    set(_default_qt6_config "D:/vcpkg/installed/${VCPKG_TARGET_TRIPLET}/share/Qt6")
    if(EXISTS "${_default_qt6_config}")
        list(APPEND CMAKE_PREFIX_PATH "${_default_qt6_config}")
    endif()
endif()

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

if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
endif()

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
    src/ModelServiceManager.cpp
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
    include/ModelServiceManager.h
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
    DESKTOP_TRANSLATE_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Network
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${X11_LIBRARIES})
elseif(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE user32)
endif()

if(WIN32 AND Qt6_DIR)
    get_filename_component(_qt6_triplet_root "${Qt6_DIR}/../.." ABSOLUTE)
    set(_qt_windeployqt "${_qt6_triplet_root}/tools/Qt6/bin/windeployqt.exe")

    if(EXISTS "${_qt_windeployqt}")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND "${_qt_windeployqt}" --no-translations "$<TARGET_FILE:${PROJECT_NAME}>"
            VERBATIM
        )
    endif()
endif()

# 编译选项
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

# 安装
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

if(UNIX AND NOT APPLE)
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
endif()
