#ifndef GLOBALSHORTCUT_H
#define GLOBALSHORTCUT_H

#include <QObject>
#include <QString>
#include <memory>

namespace DesktopTranslate {

class WindowsHotkeyEventFilter;

/**
 * @brief 全局快捷键类 - 使用 X11 实现真正的全局快捷键
 */
class GlobalShortcut : public QObject {
    Q_OBJECT

public:
    static GlobalShortcut& instance();

    // 注册快捷键
    bool registerShortcut(const QString& shortcut, const QString& id);
    
    // 注销快捷键
    void unregisterShortcut(const QString& id);
    
    // 注销所有快捷键
    void unregisterAll();

signals:
    void shortcutActivated(const QString& id);

private:
    friend class WindowsHotkeyEventFilter;
    GlobalShortcut();
    ~GlobalShortcut();
    void activateShortcutByNativeId(int native_id);
    GlobalShortcut(const GlobalShortcut&) = delete;
    GlobalShortcut& operator=(const GlobalShortcut&) = delete;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace DesktopTranslate

#endif // GLOBALSHORTCUT_H
