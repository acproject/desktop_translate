#ifndef TRANSLATION_RESULT_WINDOW_H
#define TRANSLATION_RESULT_WINDOW_H

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <memory>
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

    QLabel* original_label_{nullptr};
    QTextEdit* original_text_{nullptr};
    QLabel* translated_label_{nullptr};
    QTextEdit* translated_text_{nullptr};
    QPushButton* copy_button_{nullptr};
    QPushButton* close_button_{nullptr};
    QLabel* status_label_{nullptr};
};

} // namespace DesktopTranslate

#endif // TRANSLATION_RESULT_WINDOW_H
