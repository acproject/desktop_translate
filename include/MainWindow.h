#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QtGlobal>
#include <memory>

class QTimer;

namespace DesktopTranslate {

class SelectionOverlay;
class TranslationResultWindow;
class TestWindow;
class Config;
struct TranslationResult;

/**
 * @brief 主窗口类 - 系统托盘应用
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

public slots:
    // 开始选区翻译
    void startSelectionTranslation();
    
    // 从剪贴板翻译
    void translateFromClipboard();
    
    // 显示测试窗口
    void showTestWindow();

private slots:
    void onSelectionComplete(const QRect& rect);
    void onSelectionCancelled();
    void onTranslationComplete(const struct TranslationResult& result);
    void onSettingsAction();
    void onAboutAction();
    void onExitAction();
    void onTestApiConnection();
    void onBubbleTranslateRequested(const QString& text, const QPoint& globalPosition);
    void toggleHoverTranslation(bool enabled);
    void onPrimarySelectionChanged();
    void triggerPendingPrimaryTranslation();
    void pollWindowsHoverSelection();

private:
    void setupUI();
    void setupSystemTray();
    void setupShortcuts();
    void setupConnections();
    void performTranslation(const QString& text);
    void updateTestWindowConfig();
    void updateConfigDisplay();

    // 系统托盘
    std::unique_ptr<QSystemTrayIcon> tray_icon_;
    std::unique_ptr<QMenu> tray_menu_;
    
    // 托盘菜单动作
    QAction* action_select_translate_{nullptr};
    QAction* action_clipboard_translate_{nullptr};
    QAction* action_settings_{nullptr};
    QAction* action_about_{nullptr};
    QAction* action_exit_{nullptr};
    QAction* action_test_window_{nullptr};
    QAction* action_hover_window_{nullptr};
    
    // 选区覆盖层
    std::unique_ptr<SelectionOverlay> selection_overlay_;
    
    // 翻译结果窗口
    std::unique_ptr<TranslationResultWindow> result_window_;
    
    // 测试窗口
    std::unique_ptr<TestWindow> test_window_;
    
    // 当前选区位置
    QPoint current_selection_pos_;
    QTimer* primary_selection_timer_{nullptr};
    QString pending_primary_text_;
    QString last_primary_text_;
    bool hover_translation_enabled_{true};
    bool hover_translation_busy_{false};
    QTimer* windows_hover_poll_timer_{nullptr};
    bool left_mouse_button_down_{false};
    bool suppress_hover_clipboard_events_{false};
    qint64 left_mouse_button_pressed_at_ms_{0};
    QPoint left_mouse_button_pressed_pos_;
};

} // namespace DesktopTranslate

#endif // MAINWINDOW_H
