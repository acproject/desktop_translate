#include "SelectionOverlay.h"
#include "ClipboardManager.h"
#include <QMouseEvent>
#include <QPainter>
#include <QApplication>
#include <QScreen>
#include <QDebug>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QKeyEvent>

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
    
    // 创建确认按钮
    confirm_button_ = new QPushButton(tr("确认选区"), this);
    confirm_button_->setStyleSheet(
        "QPushButton { background-color: #28a745; color: white; border: none; "
        "border-radius: 4px; padding: 8px 16px; font-weight: bold; }"
        "QPushButton:hover { background-color: #218838; }"
    );
    confirm_button_->hide();
    
    // 创建取消按钮
    cancel_button_ = new QPushButton(tr("取消"), this);
    cancel_button_->setStyleSheet(
        "QPushButton { background-color: #dc3545; color: white; border: none; "
        "border-radius: 4px; padding: 8px 16px; font-weight: bold; }"
        "QPushButton:hover { background-color: #c82333; }"
    );
    cancel_button_->hide();
    
    // 创建信息标签
    info_label_ = new QLabel(this);
    info_label_->setStyleSheet(
        "QLabel { background-color: rgba(0, 0, 0, 180); color: white; "
        "border-radius: 4px; padding: 8px 12px; font-weight: bold; }"
    );
    info_label_->hide();
    
    // 连接信号
    connect(confirm_button_, &QPushButton::clicked, this, &SelectionOverlay::confirmSelection);
    connect(cancel_button_, &QPushButton::clicked, this, &SelectionOverlay::cancelSelection);
    
    qDebug() << "SelectionOverlay setupUI completed, geometry:" << geometry();
}

void SelectionOverlay::startSelection() {
    is_selecting_ = false;
    has_selection_ = false;
    start_point_ = QPoint();
    end_point_ = QPoint();
    selection_rect_ = QRect();
    
    // 隐藏按钮
    confirm_button_->hide();
    cancel_button_->hide();
    info_label_->hide();
    rubber_band_->hide();
    
    // 显示覆盖层
    auto screen = QGuiApplication::primaryScreen();
    setGeometry(screen->geometry());
    show();
    activateWindow();
    setFocus();
    
    // 设置光标为十字
    setCursor(Qt::CrossCursor);
    
    qDebug() << "SelectionOverlay started, isVisible:" << isVisible() 
             << ", geometry:" << geometry();
}

void SelectionOverlay::cancelSelection() {
    is_selecting_ = false;
    has_selection_ = false;
    rubber_band_->hide();
    confirm_button_->hide();
    cancel_button_->hide();
    info_label_->hide();
    hide();
    emit selectionCancelled();
}

void SelectionOverlay::confirmSelection() {
    if (!has_selection_ || selection_rect_.isEmpty()) {
        cancelSelection();
        return;
    }
    
    qDebug() << "Confirming selection:" << selection_rect_;
    
    hide();
    emit selectionComplete(selection_rect_);
}

void SelectionOverlay::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // 如果点击在按钮上，不处理
        if (confirm_button_->isVisible() && confirm_button_->geometry().contains(event->pos())) {
            QWidget::mousePressEvent(event);
            return;
        }
        if (cancel_button_->isVisible() && cancel_button_->geometry().contains(event->pos())) {
            QWidget::mousePressEvent(event);
            return;
        }
        
        // 开始新的选区 - 使用全局坐标
        is_selecting_ = true;
        has_selection_ = false;
        start_point_ = event->globalPos();
        end_point_ = start_point_;
        rubber_band_->setGeometry(QRect(mapFromGlobal(start_point_), mapFromGlobal(end_point_)).normalized());
        rubber_band_->show();
        
        // 隐藏之前的按钮
        confirm_button_->hide();
        cancel_button_->hide();
        info_label_->hide();
    } else if (event->button() == Qt::RightButton) {
        cancelSelection();
    }
    
    QWidget::mousePressEvent(event);
}

void SelectionOverlay::mouseMoveEvent(QMouseEvent* event) {
    if (is_selecting_) {
        end_point_ = event->globalPos();
        updateRubberBand();
        showSelectionInfo();
    }
    
    QWidget::mouseMoveEvent(event);
}

void SelectionOverlay::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && is_selecting_) {
        is_selecting_ = false;
        end_point_ = event->globalPos();
        
        selection_rect_ = QRect(start_point_, end_point_).normalized();
        
        // 确保选区有最小尺寸
        if (selection_rect_.width() > 10 && selection_rect_.height() > 10) {
            has_selection_ = true;
            
            // 显示确认和取消按钮
            updateButtonsPosition();
            confirm_button_->show();
            cancel_button_->show();
            info_label_->show();
            
            // 设置焦点以便接收键盘事件
            setFocus();
        } else {
            // 选区太小，继续选择
            has_selection_ = false;
            rubber_band_->hide();
            confirm_button_->hide();
            cancel_button_->hide();
            info_label_->hide();
        }
    }
    
    QWidget::mouseReleaseEvent(event);
}

void SelectionOverlay::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        cancelSelection();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (has_selection_) {
            confirmSelection();
        }
    }
    
    QWidget::keyPressEvent(event);
}

void SelectionOverlay::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    if (has_selection_) {
        // 将全局选区坐标转换为窗口相对坐标
        QRect localSelectionRect(mapFromGlobal(selection_rect_.topLeft()), 
                                  mapFromGlobal(selection_rect_.bottomRight()));
        
        // 绘制半透明背景，但选区区域清晰
        QColor overlayColor(0, 0, 0, 100);
        painter.fillRect(rect(), overlayColor);
        
        // 选区区域保持清晰
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(localSelectionRect, Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        
        // 绘制选区边框
        QPen pen(QColor(74, 144, 217), 2);
        painter.setPen(pen);
        painter.drawRect(localSelectionRect);
    } else {
        // 绘制淡色背景
        QColor overlayColor(0, 0, 0, 30);
        painter.fillRect(rect(), overlayColor);
    }
}

void SelectionOverlay::updateRubberBand() {
    if (rubber_band_) {
        // 将全局坐标转换为窗口相对坐标用于橡皮筋显示
        QRect localRect(mapFromGlobal(start_point_), mapFromGlobal(end_point_));
        rubber_band_->setGeometry(localRect.normalized());
    }
}

void SelectionOverlay::updateButtonsPosition() {
    // 将按钮放在选区右下角（使用全局坐标计算，然后转换为窗口坐标）
    int buttonY = selection_rect_.bottom() + 10;
    int buttonX = selection_rect_.right();
    
    // 计算按钮总宽度
    int buttonWidth = confirm_button_->sizeHint().width() + 10 + cancel_button_->sizeHint().width();
    buttonX -= buttonWidth;
    
    // 确保按钮在屏幕内
    auto screen = QGuiApplication::primaryScreen();
    QRect screenRect = screen->geometry();
    
    if (buttonX < screenRect.left() + 10) buttonX = screenRect.left() + 10;
    if (buttonY + confirm_button_->sizeHint().height() > screenRect.bottom() - 10) {
        buttonY = selection_rect_.top() - confirm_button_->sizeHint().height() - 10;
    }
    
    // 转换为窗口相对坐标
    QPoint buttonPos = mapFromGlobal(QPoint(buttonX, buttonY));
    
    // 定位信息标签（使用全局坐标转换为窗口坐标）
    QPoint infoPos = mapFromGlobal(QPoint(selection_rect_.left(), selection_rect_.top() - 35));
    info_label_->move(infoPos);
    
    // 定位按钮（使用转换后的坐标）
    confirm_button_->move(buttonPos);
    cancel_button_->move(buttonPos.x() + confirm_button_->sizeHint().width() + 10, buttonPos.y());
}

void SelectionOverlay::showSelectionInfo() {
    QString info = tr("选区: %1 x %2 像素")
        .arg(selection_rect_.width())
        .arg(selection_rect_.height());
    info_label_->setText(info);
    info_label_->adjustSize();
    QPoint infoPos = mapFromGlobal(QPoint(selection_rect_.left(), selection_rect_.top() - 35));
    info_label_->move(infoPos);
}

} // namespace DesktopTranslate
