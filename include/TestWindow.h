#ifndef TESTWINDOW_H
#define TESTWINDOW_H

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>
#include <QPixmap>
#include <chrono>

namespace DesktopTranslate {

/**
 * @brief 测试窗口 - 显示翻译操作日志和状态
 */
class TestWindow : public QWidget {
    Q_OBJECT

public:
    explicit TestWindow(QWidget* parent = nullptr);
    ~TestWindow() override = default;

    // 添加日志
    void log(const QString& message, const QString& level = "INFO");
    
    // 清空日志
    void clearLog();
    
    // 设置状态
    void setStatus(const QString& status, const QString& color = "blue");
    
    // 显示配置信息
    void showConfig(const QString& configInfo);
    
    // 显示截图预览
    void showScreenshot(const QPixmap& pixmap);
    void showScreenshot(const QImage& image);
    void clearScreenshot();

private:
    void setupUI();

    QLabel* status_label_{nullptr};
    QLabel* config_label_{nullptr};
    QLabel* screenshot_label_{nullptr};
    QTextEdit* log_text_{nullptr};
    QPushButton* clear_button_{nullptr};
    QPushButton* test_api_button_{nullptr};
    
    std::chrono::steady_clock::time_point start_time_;

signals:
    void testApiRequested();
    void translateTextRequested(const QString& text);
};

} // namespace DesktopTranslate

#endif // TESTWINDOW_H
