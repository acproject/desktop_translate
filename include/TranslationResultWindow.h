#ifndef TRANSLATION_RESULT_WINDOW_H
#define TRANSLATION_RESULT_WINDOW_H

#include <QWidget>
#include <QPoint>
#include <QTextBrowser>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <string>

namespace DesktopTranslate {

/**
 * @brief 翻译结果展示窗口
 */
class TranslationResultWindow : public QWidget {
    Q_OBJECT

public:
    explicit TranslationResultWindow(QWidget* parent = nullptr);
    ~TranslationResultWindow() override = default;

    // 设置翻译结果
    void setResult(const std::string& original, const std::string& translated, bool success);
    
    // 显示在指定位置附近
    void showNear(const QPoint& position);

    // 清空内容
    void clear();

signals:
    // 复制到剪贴板信号
    void copyRequested(const QString& text);
    
    // 关闭信号
    void closed();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUI();
    void setupConnections();
    void updateBubbleSize();
    void updatePinState();

    QLabel* original_text_{nullptr};
    QTextBrowser* translated_text_{nullptr};
    QPushButton* pin_button_{nullptr};
    QPushButton* copy_button_{nullptr};
    QPushButton* close_button_{nullptr};
    QLabel* status_label_{nullptr};
    QTimer* auto_close_timer_{nullptr};
    bool pinned_{false};
};

} // namespace DesktopTranslate

#endif // TRANSLATION_RESULT_WINDOW_H
