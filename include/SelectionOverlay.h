#ifndef SELECTION_OVERLAY_H
#define SELECTION_OVERLAY_H

#include <QWidget>
#include <QPoint>
#include <QRect>
#include <QRubberBand>
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

signals:
    // 选区完成信号
    void selectionComplete(const QRect& rect);
    
    // 选区取消信号
    void selectionCancelled();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void setupUI();
    void updateRubberBand();

    bool is_selecting_{false};
    QPoint start_point_;
    QPoint end_point_;
    std::unique_ptr<QRubberBand> rubber_band_;
    QRect selection_rect_;
};

} // namespace DesktopTranslate

#endif // SELECTION_OVERLAY_H
