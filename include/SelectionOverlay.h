#ifndef SELECTION_OVERLAY_H
#define SELECTION_OVERLAY_H

#include <QWidget>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QRubberBand>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <memory>

namespace DesktopTranslate {

class MainWindow;

/**
 * @brief 选区覆盖层 - 用于捕获鼠标框选区域
 */
class SelectionOverlay : public QWidget {
    Q_OBJECT

public:
    explicit SelectionOverlay(QWidget* parent = nullptr);
    ~SelectionOverlay() override = default;

    // 开始选区模式
    void startSelection();
    
    // 取消选区
    void cancelSelection();
    
    // 确认选区
    void confirmSelection();

signals:
    // 选区完成信号
    void selectionComplete(const QRect& rect);
    
    // 选区取消信号
    void selectionCancelled();
    
    // 选区确认信号（包含最终选区）
    void selectionConfirmed(const QRect& rect);
    
    // 文本确认信号（用户确认要翻译的文本）
    void textConfirmed(const QString& text, const QRect& rect);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void setupUI();
    void updateRubberBand();
    void updateButtonsPosition();
    void showSelectionInfo();
    void showTextInputDialog();

    bool is_selecting_{false};
    bool has_selection_{false};
    QPoint start_point_;
    QPoint end_point_;
    std::unique_ptr<QRubberBand> rubber_band_;
    QRect selection_rect_;
    
    // 确认和取消按钮
    QPushButton* confirm_button_{nullptr};
    QPushButton* cancel_button_{nullptr};
    QLabel* info_label_{nullptr};
    
    // 文本输入框
    QTextEdit* text_input_{nullptr};
    QLabel* text_input_label_{nullptr};
    QPushButton* text_confirm_button_{nullptr};
    QPushButton* text_cancel_button_{nullptr};
};

} // namespace DesktopTranslate

#endif // SELECTION_OVERLAY_H
