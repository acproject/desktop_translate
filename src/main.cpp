#include "MainWindow.h"
#include "Config.h"
#include "TranslationService.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <iostream>

namespace {

QIcon loadApplicationIcon() {
    const QStringList candidates = {
        QStringLiteral(DESKTOP_TRANSLATE_SOURCE_ICON),
        "/usr/share/pixmaps/desktop-translate.png"
    };

    for (const QString& path : candidates) {
        if (QFile::exists(path)) {
            QIcon icon(path);
            if (!icon.isNull()) {
                return icon;
            }
        }
    }

    return QIcon::fromTheme("desktop-translate");
}

}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("DesktopTranslate");
    app.setApplicationVersion("1.0.2");
    app.setDesktopFileName("desktop-translate");
    app.setQuitOnLastWindowClosed(false);  // 关闭窗口不退出应用（托盘应用）
    const QIcon appIcon = loadApplicationIcon();
    app.setWindowIcon(appIcon);
    
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
        mainWindow.setWindowIcon(appIcon);
        
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
