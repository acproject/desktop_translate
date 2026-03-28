#ifndef CLIPBOARD_MANAGER_H
#define CLIPBOARD_MANAGER_H

#include <QObject>
#include <QString>
#include <memory>

namespace DesktopTranslate {

/**
 * @brief 剪贴板管理器
 */
class ClipboardManager : public QObject {
    Q_OBJECT

public:
    static ClipboardManager& instance();

    // 复制文本到剪贴板
    void copyToClipboard(const QString& text);
    
    // 从剪贴板获取文本
    QString getFromClipboard();
    
    // 从选区获取文本（通过OCR或剪贴板模拟）
    QString getTextFromSelection(int x, int y, int width, int height);

signals:
    void textCopied(const QString& text);

private:
    ClipboardManager();
    ~ClipboardManager() = default;
    ClipboardManager(const ClipboardManager&) = delete;
    ClipboardManager& operator=(const ClipboardManager&) = delete;
};

} // namespace DesktopTranslate

#endif // CLIPBOARD_MANAGER_H
