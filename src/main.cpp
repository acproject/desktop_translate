#include "MainWindow.h"
#include "Config.h"
#include "TranslationService.h"
#include <QApplication>
#include <QFile>
#include <QFont>
#include <iostream>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("DesktopTranslate");
    app.setApplicationVersion("1.0.0");
    app.setQuitOnLastWindowClosed(false);  // 关闭窗口不退出应用（托盘应用）
    
    std::cout << "Application created..." << std::endl;
    
    // 设置默认字体
    QFont font = app.font();
    font.setFamily("WenQuanYi Micro Hei");
    app.setFont(font);
    
    try {
        std::cout << "Loading config..." << std::endl;
        // 加载配置
        DesktopTranslate::Config::instance().load();
        
        std::cout << "Creating main window..." << std::endl;
        // 创建主窗口
        DesktopTranslate::MainWindow mainWindow;
        
        std::cout << "Desktop Translate started." << std::endl;
        std::cout << "Press Ctrl+F3 to start selection translation." << std::endl;
        std::cout << "Press Ctrl+F4 to translate from clipboard." << std::endl;
        std::cout << "Click the tray icon to start selection." << std::endl;
        
        std::cout << "Entering event loop..." << std::endl;
        int result = app.exec();
        std::cout << "Event loop exited with code: " << result << std::endl;
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
