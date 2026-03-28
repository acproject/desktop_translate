#include "TranslationResultWindow.h"
#include <QApplication>
#include <QClipboard>
#include <QScreen>
#include <QDebug>
#include <QTimer>
#include <QKeyEvent>
#include <QCloseEvent>

namespace DesktopTranslate {

TranslationResultWindow::TranslationResultWindow(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    setupConnections();
}

void TranslationResultWindow::setupUI() {
    // 窗口设置
    setWindowTitle(tr("翻译结果"));
    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
    setMinimumSize(400, 300);
    resize(500, 400);
    
    // 主布局
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);
    
    // 状态标签
    status_label_ = new QLabel(this);
    status_label_->setStyleSheet("font-weight: bold; padding: 5px;");
    mainLayout->addWidget(status_label_);
    
    // 原文区域
    original_label_ = new QLabel(tr("原文:"), this);
    original_label_->setStyleSheet("font-weight: bold; color: #666;");
    mainLayout->addWidget(original_label_);
    
    original_text_ = new QTextEdit(this);
    original_text_->setReadOnly(true);
    original_text_->setMaximumHeight(100);
    original_text_->setStyleSheet(
        "QTextEdit { background-color: #f5f5f5; border: 1px solid #ddd; border-radius: 4px; padding: 5px; }"
    );
    mainLayout->addWidget(original_text_);
    
    // 译文区域
    translated_label_ = new QLabel(tr("译文:"), this);
    translated_label_->setStyleSheet("font-weight: bold; color: #333;");
    mainLayout->addWidget(translated_label_);
    
    translated_text_ = new QTextEdit(this);
    translated_text_->setReadOnly(true);
    translated_text_->setStyleSheet(
        "QTextEdit { background-color: #fff; border: 2px solid #4A90D9; border-radius: 4px; padding: 5px; }"
    );
    mainLayout->addWidget(translated_text_);
    
    // 按钮区域
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    copy_button_ = new QPushButton(tr("复制译文"), this);
    copy_button_->setFixedWidth(100);
    copy_button_->setStyleSheet(
        "QPushButton { background-color: #4A90D9; color: white; border: none; border-radius: 4px; padding: 8px; }"
        "QPushButton:hover { background-color: #357ABD; }"
        "QPushButton:pressed { background-color: #2a5f8f; }"
    );
    buttonLayout->addWidget(copy_button_);
    
    close_button_ = new QPushButton(tr("关闭"), this);
    close_button_->setFixedWidth(80);
    close_button_->setStyleSheet(
        "QPushButton { background-color: #6c757d; color: white; border: none; border-radius: 4px; padding: 8px; }"
        "QPushButton:hover { background-color: #5a6268; }"
        "QPushButton:pressed { background-color: #545b62; }"
    );
    buttonLayout->addWidget(close_button_);
    
    mainLayout->addLayout(buttonLayout);
    
    // 窗口样式
    setStyleSheet(
        "QWidget { font-family: 'Microsoft YaHei', 'WenQuanYi Micro Hei', sans-serif; font-size: 13px; }"
    );
}

void TranslationResultWindow::setupConnections() {
    connect(copy_button_, &QPushButton::clicked, this, [this]() {
        QString text = translated_text_->toPlainText();
        QApplication::clipboard()->setText(text);
        copy_button_->setText(tr("已复制!"));
        QTimer::singleShot(2000, [this]() {
            copy_button_->setText(tr("复制译文"));
        });
    });
    
    connect(close_button_, &QPushButton::clicked, this, &QWidget::close);
}

void TranslationResultWindow::setResult(const std::string& original, const std::string& translated, bool success) {
    original_text_->setPlainText(QString::fromStdString(original));
    
    if (success) {
        translated_text_->setPlainText(QString::fromStdString(translated));
        status_label_->setText(tr("✓ 翻译成功"));
        status_label_->setStyleSheet("font-weight: bold; color: green; padding: 5px;");
    } else {
        translated_text_->setPlainText(QString::fromStdString(translated));
        status_label_->setText(tr("✗ 翻译失败"));
        status_label_->setStyleSheet("font-weight: bold; color: red; padding: 5px;");
    }
}

void TranslationResultWindow::showNear(const QPoint& position) {
    // 获取屏幕信息
    auto screen = QGuiApplication::screenAt(position);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    auto screenGeometry = screen->geometry();
    
    // 计算窗口位置
    QPoint showPos = position;
    
    // 确保窗口在屏幕内
    if (showPos.x() + width() > screenGeometry.right()) {
        showPos.setX(screenGeometry.right() - width());
    }
    if (showPos.y() + height() > screenGeometry.bottom()) {
        showPos.setY(position.y() - height());
    }
    if (showPos.x() < screenGeometry.left()) {
        showPos.setX(screenGeometry.left());
    }
    if (showPos.y() < screenGeometry.top()) {
        showPos.setY(screenGeometry.top());
    }
    
    move(showPos);
    show();
    activateWindow();
    raise();
}

void TranslationResultWindow::clear() {
    original_text_->clear();
    translated_text_->clear();
    status_label_->clear();
}

void TranslationResultWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        close();
    }
    QWidget::keyPressEvent(event);
}

void TranslationResultWindow::closeEvent(QCloseEvent* event) {
    emit closed();
    QWidget::closeEvent(event);
}

} // namespace DesktopTranslate
