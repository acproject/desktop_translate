#include "TranslationResultWindow.h"
#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QTextDocument>
#include <QScreen>
#include <QTimer>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace DesktopTranslate {

TranslationResultWindow::TranslationResultWindow(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    setupConnections();
}

void TranslationResultWindow::setupUI() {
    setWindowTitle(tr("翻译结果"));
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
    resize(360, 160);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    status_label_ = new QLabel(this);
    status_label_->setStyleSheet("font-weight: 600; font-size: 12px;");
    mainLayout->addWidget(status_label_);

    original_text_ = new QLabel(this);
    original_text_->setWordWrap(true);
    original_text_->setStyleSheet("color: #444444; font-size: 12px;");
    mainLayout->addWidget(original_text_);

    translated_text_ = new QTextBrowser(this);
    translated_text_->setReadOnly(true);
    translated_text_->setFrameShape(QFrame::NoFrame);
    translated_text_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    translated_text_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    translated_text_->setOpenExternalLinks(false);
    translated_text_->setStyleSheet(
        "QTextBrowser {"
        "background-color: rgba(255, 255, 255, 0.92);"
        "border: none;"
        "border-radius: 10px;"
        "padding: 8px;"
        "color: #111111;"
        "font-size: 14px;"
        "}"
    );
    mainLayout->addWidget(translated_text_);

    auto* buttonLayout = new QHBoxLayout();
    pin_button_ = new QPushButton(tr("钉住"), this);
    pin_button_->setStyleSheet(
        "QPushButton { background-color: rgba(74, 144, 217, 0.14); color: #1D4F91; border: 1px solid rgba(74, 144, 217, 0.30); border-radius: 6px; padding: 6px 12px; }"
        "QPushButton:hover { background-color: rgba(74, 144, 217, 0.22); }"
    );
    buttonLayout->addWidget(pin_button_);

    copy_button_ = new QPushButton(tr("复制"), this);
    copy_button_->setStyleSheet(
        "QPushButton { background-color: #4A90D9; color: white; border: none; border-radius: 6px; padding: 6px 12px; }"
        "QPushButton:hover { background-color: #357ABD; }"
    );
    buttonLayout->addWidget(copy_button_);

    buttonLayout->addStretch();

    close_button_ = new QPushButton(tr("×"), this);
    close_button_->setFixedSize(28, 28);
    close_button_->setStyleSheet(
        "QPushButton { background-color: rgba(0, 0, 0, 0.08); color: #333333; border: none; border-radius: 14px; font-size: 15px; }"
        "QPushButton:hover { background-color: rgba(0, 0, 0, 0.14); }"
    );
    buttonLayout->addWidget(close_button_);

    mainLayout->addLayout(buttonLayout);

    setStyleSheet(
        "TranslationResultWindow {"
        "background-color: rgba(255, 255, 255, 0.97);"
        "border: 1px solid rgba(0, 0, 0, 0.12);"
        "border-radius: 14px;"
        "font-family: 'Microsoft YaHei', 'WenQuanYi Micro Hei', sans-serif;"
        "font-size: 13px;"
        "}"
    );

    auto_close_timer_ = new QTimer(this);
    auto_close_timer_->setSingleShot(true);
    updatePinState();
}

void TranslationResultWindow::setupConnections() {
    connect(pin_button_, &QPushButton::clicked, this, [this]() {
        pinned_ = !pinned_;
        updatePinState();
        if (pinned_) {
            auto_close_timer_->stop();
        } else {
            auto_close_timer_->start(12000);
        }
    });

    connect(copy_button_, &QPushButton::clicked, this, [this]() {
        QString text = translated_text_->toPlainText();
        QApplication::clipboard()->setText(text);
        emit copyRequested(text);
        copy_button_->setText(tr("已复制"));
        QTimer::singleShot(2000, [this]() {
            copy_button_->setText(tr("复制"));
        });
    });

    connect(close_button_, &QPushButton::clicked, this, &QWidget::close);
    connect(auto_close_timer_, &QTimer::timeout, this, &QWidget::hide);
}

void TranslationResultWindow::setResult(const std::string& original, const std::string& translated, bool success) {
    QString originalText = QString::fromStdString(original).trimmed();
    QString translatedText = QString::fromStdString(translated).trimmed();

    if (originalText.size() > 140) {
        originalText = originalText.left(140) + "...";
    }

    original_text_->setText(originalText);
    original_text_->setVisible(!originalText.isEmpty());

    if (success) {
        translated_text_->setPlainText(translatedText);
        status_label_->setText(tr("翻译结果"));
        status_label_->setStyleSheet("font-weight: 600; font-size: 12px; color: #188038;");
    } else {
        translated_text_->setPlainText(translatedText);
        status_label_->setText(tr("翻译失败"));
        status_label_->setStyleSheet("font-weight: 600; font-size: 12px; color: #C62828;");
    }

    updateBubbleSize();
    if (pinned_) {
        auto_close_timer_->stop();
    } else {
        auto_close_timer_->start(12000);
    }
}

void TranslationResultWindow::showNear(const QPoint& position) {
    auto screen = QGuiApplication::screenAt(position);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    const QRect screenGeometry = screen->availableGeometry();

    QPoint showPos = position + QPoint(16, 16);
    if (showPos.x() + width() > screenGeometry.right()) {
        showPos.setX(position.x() - width() - 16);
    }
    if (showPos.y() + height() > screenGeometry.bottom()) {
        showPos.setY(position.y() - height() - 16);
    }
    if (showPos.x() < screenGeometry.left()) {
        showPos.setX(screenGeometry.left() + 8);
    }
    if (showPos.y() < screenGeometry.top()) {
        showPos.setY(screenGeometry.top() + 8);
    }

    move(showPos);
    show();
    raise();
}

void TranslationResultWindow::clear() {
    original_text_->clear();
    translated_text_->clear();
    status_label_->clear();
    auto_close_timer_->stop();
    pinned_ = false;
    updatePinState();
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

void TranslationResultWindow::updateBubbleSize() {
    constexpr int bubbleWidth = 360;
    constexpr int minTextHeight = 64;
    constexpr int maxTextHeight = 220;

    translated_text_->setFixedWidth(bubbleWidth - 24);
    translated_text_->document()->setTextWidth(translated_text_->viewport()->width() - 8);

    const int textHeight = qBound(
        minTextHeight,
        static_cast<int>(translated_text_->document()->size().height()) + 16,
        maxTextHeight
    );

    translated_text_->setFixedHeight(textHeight);
    setFixedWidth(bubbleWidth);
    adjustSize();
}

void TranslationResultWindow::updatePinState() {
    if (!pin_button_) {
        return;
    }

    pin_button_->setText(pinned_ ? tr("取消钉住") : tr("钉住"));
}

} // namespace DesktopTranslate
