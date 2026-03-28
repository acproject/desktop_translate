#include "ClipboardManager.h"
#include <QApplication>
#include <QClipboard>
#include <QProcess>
#include <QScreen>
#include <QPixmap>
#include <QDebug>
#include <QThread>

namespace DesktopTranslate {

ClipboardManager& ClipboardManager::instance() {
    static ClipboardManager manager;
    return manager;
}

ClipboardManager::ClipboardManager() = default;

void ClipboardManager::copyToClipboard(const QString& text) {
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(text);
    emit textCopied(text);
}

QString ClipboardManager::getFromClipboard() {
    QClipboard* clipboard = QApplication::clipboard();
    return clipboard->text();
}

QString ClipboardManager::getTextFromSelection(int x, int y, int width, int height) {
    // 方法1: 尝试使用 xdotool 模拟 Ctrl+C
    // 先保存当前剪贴板内容
    QClipboard* clipboard = QApplication::clipboard();
    QString oldContent = clipboard->text();
    
    // 使用 xdotool 选中文本并复制
    QProcess process;
    
    // 首先移动鼠标到选区中心并点击
    int centerX = x + width / 2;
    int centerY = y + height / 2;
    
    // 双击选中文本
    process.start("xdotool", QStringList() 
        << "mousemove" << QString::number(centerX) << QString::number(centerY)
    );
    process.waitForFinished();
    
    process.start("xdotool", QStringList() 
        << "click" << "--repeat" << "2" << "1"  // 双击左键
    );
    process.waitForFinished();
    
    // 等待一下让选中生效
    QThread::msleep(100);
    
    // 使用 Ctrl+C 复制
    process.start("xdotool", QStringList() 
        << "key" << "ctrl+c"
    );
    process.waitForFinished();
    
    // 等待剪贴板更新
    QThread::msleep(200);
    
    // 获取新内容
    QString newContent = clipboard->text();
    
    // 恢复旧内容（如果新内容为空）
    if (newContent.isEmpty()) {
        // 方法2: 尝试使用 xsel
        process.start("xsel", QStringList() << "-o");
        process.waitForFinished();
        newContent = QString::fromUtf8(process.readAllStandardOutput());
    }
    
    return newContent;
}

} // namespace DesktopTranslate
