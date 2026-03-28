#include "SelectionOverlay.h"
#include <QMouseEvent>
#include <QPainter>
#include <QApplication>
#include <QScreen>
#include <QDebug>
#include <QGuiApplication>

namespace DesktopTranslate {

SelectionOverlay::SelectionOverlay(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void SelectionOverlay::setupUI() {
    // 设置为全屏透明窗口
    setWindowFlags(Qt::FramelessWindowHint | 
                   Qt::WindowStaysOnTopHint | 
                   Qt::Tool);
    
    // 设置透明背景
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    
    // 设置覆盖全屏
    auto screen = QGuiApplication::primaryScreen();
    setGeometry(screen->geometry());
    
    // 设置鼠标追踪
    setMouseTracking(true);
    
    // 创建橡皮筋选区
    rubber_band_ = std::make_unique<QRubberBand>(QRubberBand::Rectangle, this);
    
    qDebug() << "SelectionOverlay setupUI completed, geometry:" << geometry();
}

void SelectionOverlay::startSelection() {
    is_selecting_ = false;
    start_point_ = QPoint();
    end_point_ = QPoint();
    selection_rect_ = QRect();
    
    // 显示覆盖层
    auto screen = QGuiApplication::primaryScreen();
    setGeometry(screen->geometry());
    show();
    activateWindow();
    setFocus();
    
    // 设置光标为十字
    setCursor(Qt::CrossCursor);
    
    qDebug() << "SelectionOverlay started, isVisible:" << isVisible() 
             << ", geometry:" << geometry()
             << ", cursor:" << cursor().shape();
}

void SelectionOverlay::cancelSelection() {
    is_selecting_ = false;
    rubber_band_->hide();
    hide();
    emit selectionCancelled();
}

void SelectionOverlay::mousePressEvent(QMouseEvent* event) {
    qDebug() << "SelectionOverlay mousePressEvent, button:" << event->button() << "pos:" << event->pos();
    
    if (event->button() == Qt::LeftButton) {
        is_selecting_ = true;
        start_point_ = event->pos();
        end_point_ = start_point_;
        rubber_band_->setGeometry(QRect(start_point_, end_point_).normalized());
        rubber_band_->show();
    } else if (event->button() == Qt::RightButton) {
        cancelSelection();
    }
    
    QWidget::mousePressEvent(event);
}

void SelectionOverlay::mouseMoveEvent(QMouseEvent* event) {
    if (is_selecting_) {
        end_point_ = event->pos();
        updateRubberBand();
    }
    
    QWidget::mouseMoveEvent(event);
}

void SelectionOverlay::mouseReleaseEvent(QMouseEvent* event) {
    qDebug() << "SelectionOverlay mouseReleaseEvent, button:" << event->button() << "is_selecting:" << is_selecting_;
    
    if (event->button() == Qt::LeftButton && is_selecting_) {
        is_selecting_ = false;
        end_point_ = event->pos();
        rubber_band_->hide();
        
        selection_rect_ = QRect(start_point_, end_point_).normalized();
        
        qDebug() << "Selection rect:" << selection_rect_ 
                 << "width:" << selection_rect_.width() 
                 << "height:" << selection_rect_.height();
        
        // 确保选区有最小尺寸
        if (selection_rect_.width() > 5 && selection_rect_.height() > 5) {
            qDebug() << "Hiding overlay and emitting selectionComplete signal...";
            hide();
            qDebug() << "Emitting selectionComplete with rect:" << selection_rect_;
            emit selectionComplete(selection_rect_);
            qDebug() << "Signal emitted successfully";
        } else {
            qDebug() << "Selection too small, cancelling...";
            cancelSelection();
        }
    }
    
    QWidget::mouseReleaseEvent(event);
}

void SelectionOverlay::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        cancelSelection();
    }
    
    QWidget::keyPressEvent(event);
}

void SelectionOverlay::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制半透明背景
    QColor overlayColor(0, 0, 0, 30);
    painter.fillRect(rect(), overlayColor);
}

void SelectionOverlay::updateRubberBand() {
    if (rubber_band_) {
        rubber_band_->setGeometry(QRect(start_point_, end_point_).normalized());
    }
}

} // namespace DesktopTranslate
