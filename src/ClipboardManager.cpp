#include "ClipboardManager.h"
#include <QApplication>
#include <QClipboard>
#include <QEventLoop>
#include <QMimeData>
#include <QProcess>
#include <QScreen>
#include <QPixmap>
#include <QDebug>
#include <QTimer>
#include <QThread>

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace DesktopTranslate {

namespace {

std::unique_ptr<QMimeData> cloneMimeData(const QMimeData* source) {
    auto mimeData = std::make_unique<QMimeData>();
    if (!source) {
        return mimeData;
    }

    for (const QString& format : source->formats()) {
        mimeData->setData(format, source->data(format));
    }

    return mimeData;
}

#if defined(Q_OS_WIN)
bool sendCopyShortcutToForegroundWindow() {
    HWND foregroundWindow = GetForegroundWindow();
    if (!foregroundWindow) {
        return false;
    }

    DWORD processId = 0;
    const DWORD threadId = GetWindowThreadProcessId(foregroundWindow, &processId);
    if (processId == GetCurrentProcessId()) {
        return false;
    }

    GUITHREADINFO guiThreadInfo{};
    guiThreadInfo.cbSize = sizeof(guiThreadInfo);
    HWND targetWindow = foregroundWindow;
    if (threadId != 0 && GetGUIThreadInfo(threadId, &guiThreadInfo) && guiThreadInfo.hwndFocus) {
        targetWindow = guiThreadInfo.hwndFocus;
    }

    INPUT inputs[4]{};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'C';
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'C';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(4, inputs, sizeof(INPUT)) == 4;
}
#endif

}

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

QString ClipboardManager::captureSelectedTextFromActiveWindow() {
#if defined(Q_OS_WIN)
    QClipboard* clipboard = QApplication::clipboard();
    if (!clipboard) {
        return {};
    }

    const QString previousText = clipboard->text(QClipboard::Clipboard);
    auto previousMimeData = cloneMimeData(clipboard->mimeData(QClipboard::Clipboard));

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    QObject::connect(clipboard, &QClipboard::dataChanged, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);

    if (!sendCopyShortcutToForegroundWindow()) {
        return {};
    }

    timeoutTimer.start(250);
    loop.exec();

    const QString capturedText = clipboard->text(QClipboard::Clipboard).trimmed();

    if (previousMimeData && !previousMimeData->formats().isEmpty()) {
        clipboard->setMimeData(previousMimeData.release(), QClipboard::Clipboard);
    } else if (previousText.isEmpty()) {
        clipboard->clear(QClipboard::Clipboard);
    } else {
        clipboard->setText(previousText, QClipboard::Clipboard);
    }

    return capturedText;
#else
    return {};
#endif
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
