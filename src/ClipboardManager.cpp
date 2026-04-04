#include "ClipboardManager.h"
#include <QApplication>
#include <QClipboard>
#include <QEventLoop>
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

QString elideCapturedText(const QString& text) {
    if (text.size() <= 60) {
        return text;
    }
    return text.left(60) + "...";
}

#if defined(Q_OS_WIN)
QString readUnicodeTextFromClipboard() {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        return {};
    }

    HANDLE clipboardData = GetClipboardData(CF_UNICODETEXT);
    if (!clipboardData) {
        return {};
    }

    const auto* text = static_cast<const wchar_t*>(GlobalLock(clipboardData));
    if (!text) {
        return {};
    }

    const QString result = QString::fromWCharArray(text).trimmed();
    GlobalUnlock(clipboardData);
    return result;
}

QString readClipboardTextWithRetry() {
    for (int attempt = 0; attempt < 10; ++attempt) {
        if (OpenClipboard(nullptr)) {
            const QString text = readUnicodeTextFromClipboard();
            CloseClipboard();
            return text;
        }
        Sleep(20);
    }

    qWarning() << "Windows selection capture: OpenClipboard failed" << GetLastError();
    return {};
}

bool sendCopyShortcutToForegroundWindow() {
    HWND foregroundWindow = GetForegroundWindow();
    if (!foregroundWindow) {
        qWarning() << "Windows selection capture: no foreground window available";
        return false;
    }

    DWORD processId = 0;
    const DWORD threadId = GetWindowThreadProcessId(foregroundWindow, &processId);
    if (processId == GetCurrentProcessId()) {
        qInfo() << "Windows selection capture: foreground window belongs to current process, skip Ctrl+C";
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
    const UINT sent = SendInput(4, inputs, sizeof(INPUT));
    if (sent != 4) {
        qWarning() << "Windows selection capture: SendInput failed" << GetLastError();
        return false;
    }

    qInfo() << "Windows selection capture: sent Ctrl+C to target window" << Qt::hex
            << quintptr(targetWindow);
    return true;
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

    const DWORD previousSequence = GetClipboardSequenceNumber();

    DWORD currentSequence = previousSequence;
    QString capturedText;
    bool clipboardChanged = false;

    for (int attempt = 1; attempt <= 2; ++attempt) {
        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);

        QObject::connect(clipboard, &QClipboard::dataChanged, &loop, &QEventLoop::quit);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);

        qInfo() << "Windows selection capture: attempt" << attempt << "sending Ctrl+C";
        emit selectionCaptureLogged(tr("Windows 划词复制：第 %1 次尝试发送 Ctrl+C").arg(attempt), "INFO");
        if (!sendCopyShortcutToForegroundWindow()) {
            emit selectionCaptureLogged(tr("Windows 划词复制：发送 Ctrl+C 失败"), "WARN");
            break;
        }

        timeoutTimer.start(attempt == 1 ? 300 : 450);
        loop.exec();

        currentSequence = GetClipboardSequenceNumber();
        clipboardChanged = currentSequence != previousSequence;
        capturedText = readClipboardTextWithRetry();

        if (clipboardChanged && !capturedText.isEmpty()) {
            qInfo() << "Windows selection capture: clipboard updated after Ctrl+C, length ="
                    << capturedText.size();
            emit selectionCaptureLogged(
                tr("Windows 划词复制：剪贴板已更新，获取到文本 %1")
                    .arg(elideCapturedText(capturedText).toHtmlEscaped()),
                "SUCCESS");
            break;
        }

        qInfo() << "Windows selection capture: attempt" << attempt
                << "did not produce usable clipboard text";
        emit selectionCaptureLogged(
            tr("Windows 划词复制：第 %1 次尝试未获取到有效剪贴板文本").arg(attempt),
            "WARN");
        QThread::msleep(80);
    }

    if (!capturedText.isEmpty() && clipboardChanged) {
        emit selectionCaptureLogged(tr("Windows 划词复制：保留复制后的剪贴板内容"), "INFO");
        return capturedText;
    }

    qInfo() << "Windows selection capture: no text captured from clipboard after Ctrl+C attempts";
    emit selectionCaptureLogged(tr("Windows 划词复制：两次尝试后仍未获取到文本"), "WARN");
    return clipboardChanged ? capturedText : QString();
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
