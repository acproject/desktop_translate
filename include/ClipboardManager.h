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

    void copyToClipboard(const QString& text);
    QString getFromClipboard();
    QString getTextFromSelection(int x, int y, int width, int height);
    QString captureSelectedTextFromActiveWindow();

signals:
    void textCopied(const QString& text);
    void selectionCaptureLogged(const QString& message, const QString& level);

private:
    ClipboardManager();
    ~ClipboardManager() = default;
    ClipboardManager(const ClipboardManager&) = delete;
    ClipboardManager& operator=(const ClipboardManager&) = delete;
};

} // namespace DesktopTranslate

#endif // CLIPBOARD_MANAGER_H
