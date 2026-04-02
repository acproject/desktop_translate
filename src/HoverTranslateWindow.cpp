#include "HoverTranslateWindow.h"
#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

namespace DesktopTranslate {

namespace {

QString hoverText(QClipboard* clipboard) {
    if (!clipboard) {
        return {};
    }

    if (clipboard->supportsSelection()) {
        return clipboard->text(QClipboard::Selection).trimmed();
    }

    return clipboard->text(QClipboard::Clipboard).trimmed();
}

QString emptyPreviewText(QClipboard* clipboard, const QObject* context) {
    if (clipboard && clipboard->supportsSelection()) {
        return context->tr("当前 PRIMARY 选区为空");
    }

    return context->tr("当前剪贴板为空");
}

QString hoverHintText(QClipboard* clipboard, const QObject* context) {
    if (clipboard && clipboard->supportsSelection()) {
        return context->tr("拖放文本到这里，或翻译当前选中的 PRIMARY 文本");
    }

    return context->tr("拖放文本到这里，或翻译当前剪贴板中的文本");
}

}

HoverTranslateWindow::HoverTranslateWindow(QWidget* parent)
    : QWidget(parent)
{
    setupUI();

    auto* clipboard = QApplication::clipboard();
    if (clipboard->supportsSelection()) {
        connect(clipboard, &QClipboard::selectionChanged, this, &HoverTranslateWindow::onPrimarySelectionChanged);
    } else {
        connect(clipboard, &QClipboard::dataChanged, this, &HoverTranslateWindow::onPrimarySelectionChanged);
    }
    connect(translate_primary_button_, &QPushButton::clicked, this, &HoverTranslateWindow::emitPrimarySelectionTranslation);
    connect(hide_button_, &QPushButton::clicked, this, &QWidget::hide);

    auto_translate_timer_ = new QTimer(this);
    auto_translate_timer_->setSingleShot(true);
    connect(auto_translate_timer_, &QTimer::timeout, this, &HoverTranslateWindow::triggerPendingPrimaryTranslation);

    updatePrimaryPreview();
}

void HoverTranslateWindow::setBusy(bool busy) {
    busy_ = busy;
    translate_primary_button_->setEnabled(!busy);
    if (busy) {
        hint_label_->setText(tr("翻译中..."));
        auto_translate_timer_->stop();
        return;
    }

    hint_label_->setText(hoverHintText(QApplication::clipboard(), this));
    updatePrimaryPreview();
    if (!pending_primary_text_.isEmpty()) {
        auto_translate_timer_->start(120);
    }
}

void HoverTranslateWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (!busy_ && event->mimeData()->hasText()) {
        event->acceptProposedAction();
        hint_label_->setText(tr("松开鼠标开始翻译"));
        return;
    }

    event->ignore();
}

void HoverTranslateWindow::dropEvent(QDropEvent* event) {
    if (busy_) {
        event->ignore();
        return;
    }

    const QString text = event->mimeData()->text().trimmed();
    if (text.isEmpty()) {
        event->ignore();
        updatePrimaryPreview();
        return;
    }

    hint_label_->setText(tr("已接收拖放文本"));
    emit translateRequested(text, mapToGlobal(event->position().toPoint()));
    event->acceptProposedAction();
}

void HoverTranslateWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        drag_offset_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
    QWidget::mousePressEvent(event);
}

void HoverTranslateWindow::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - drag_offset_);
    }
    QWidget::mouseMoveEvent(event);
}

void HoverTranslateWindow::setupUI() {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAcceptDrops(true);
    setWindowTitle(tr("悬浮翻译"));
    resize(280, 170);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    title_label_ = new QLabel(tr("悬浮翻译"), this);
    title_label_->setStyleSheet("font-weight: bold; font-size: 15px; color: white;");
    layout->addWidget(title_label_);

    hint_label_ = new QLabel(hoverHintText(QApplication::clipboard(), this), this);
    hint_label_->setWordWrap(true);
    hint_label_->setStyleSheet("color: rgba(255, 255, 255, 0.9);");
    layout->addWidget(hint_label_);

    selection_preview_label_ = new QLabel(this);
    selection_preview_label_->setWordWrap(true);
    selection_preview_label_->setMinimumHeight(50);
    selection_preview_label_->setStyleSheet(
        "background-color: rgba(255, 255, 255, 0.14);"
        "border-radius: 8px;"
        "padding: 8px;"
        "color: white;"
    );
    layout->addWidget(selection_preview_label_);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);

    translate_primary_button_ = new QPushButton(tr("翻译选中"), this);
    translate_primary_button_->setStyleSheet(
        "QPushButton { background-color: #4A90D9; color: white; border: none; border-radius: 6px; padding: 8px 12px; }"
        "QPushButton:hover { background-color: #357ABD; }"
        "QPushButton:disabled { background-color: #6C7A89; }"
    );
    buttonLayout->addWidget(translate_primary_button_);

    hide_button_ = new QPushButton(tr("隐藏"), this);
    hide_button_->setStyleSheet(
        "QPushButton { background-color: rgba(255, 255, 255, 0.18); color: white; border: none; border-radius: 6px; padding: 8px 12px; }"
        "QPushButton:hover { background-color: rgba(255, 255, 255, 0.28); }"
    );
    buttonLayout->addWidget(hide_button_);

    layout->addLayout(buttonLayout);

    setStyleSheet(
        "HoverTranslateWindow {"
        "background-color: rgba(27, 38, 59, 0.92);"
        "border: 1px solid rgba(255, 255, 255, 0.18);"
        "border-radius: 12px;"
        "font-family: 'Microsoft YaHei', 'WenQuanYi Micro Hei', sans-serif;"
        "font-size: 13px;"
        "}"
    );

    auto* screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect geometry = screen->availableGeometry();
        move(geometry.right() - width() - 24, geometry.center().y() - height() / 2);
    }
}

void HoverTranslateWindow::onPrimarySelectionChanged() {
    auto* clipboard = QApplication::clipboard();
    const QString text = hoverText(clipboard);

    if (text.isEmpty()) {
        pending_primary_text_.clear();
        last_auto_translated_text_.clear();
        auto_translate_timer_->stop();
        updatePrimaryPreview();
        hide();
        return;
    }

    pending_primary_text_ = text;
    updatePrimaryPreview();
    if (isVisible()) {
        showNearCursor();
        raise();
    }

    if (busy_) {
        hint_label_->setText(tr("检测到新的选中文本"));
        return;
    }

    hint_label_->setText(
        clipboard->supportsSelection()
            ? tr("检测到 PRIMARY 选区，准备自动翻译")
            : tr("检测到新的剪贴板文本，准备自动翻译")
    );
    auto_translate_timer_->start(350);
}

void HoverTranslateWindow::updatePrimaryPreview() {
    auto* clipboard = QApplication::clipboard();
    QString text = hoverText(clipboard);

    if (text.isEmpty()) {
        selection_preview_label_->setText(emptyPreviewText(clipboard, this));
        return;
    }

    if (text.size() > 120) {
        text = text.left(120) + "...";
    }
    selection_preview_label_->setText(text);
}

void HoverTranslateWindow::emitPrimarySelectionTranslation() {
    auto* clipboard = QApplication::clipboard();
    const QString text = hoverText(clipboard);
    if (text.isEmpty()) {
        hint_label_->setText(tr("当前没有选中文本"));
        selection_preview_label_->setText(emptyPreviewText(clipboard, this));
        return;
    }

    hint_label_->setText(clipboard->supportsSelection() ? tr("已读取 PRIMARY 选区") : tr("已读取剪贴板文本"));
    pending_primary_text_.clear();
    last_auto_translated_text_ = text;
    emit translateRequested(text, frameGeometry().center());
}

void HoverTranslateWindow::triggerPendingPrimaryTranslation() {
    if (busy_) {
        return;
    }

    const QString text = pending_primary_text_.trimmed();
    if (text.isEmpty()) {
        return;
    }

    if (text == last_auto_translated_text_) {
        return;
    }

    last_auto_translated_text_ = text;
    hint_label_->setText(
        QApplication::clipboard()->supportsSelection()
            ? tr("检测到 PRIMARY 选区，开始自动翻译")
            : tr("检测到新的剪贴板文本，开始自动翻译")
    );
    emit translateRequested(text, QCursor::pos());
}

void HoverTranslateWindow::showNearCursor() {
    const QPoint cursorPos = QCursor::pos();
    auto* screen = QGuiApplication::screenAt(cursorPos);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return;
    }

    const QRect geometry = screen->availableGeometry();
    QPoint pos = cursorPos + QPoint(18, 18);

    if (pos.x() + width() > geometry.right()) {
        pos.setX(cursorPos.x() - width() - 18);
    }
    if (pos.y() + height() > geometry.bottom()) {
        pos.setY(cursorPos.y() - height() - 18);
    }
    if (pos.x() < geometry.left()) {
        pos.setX(geometry.left() + 8);
    }
    if (pos.y() < geometry.top()) {
        pos.setY(geometry.top() + 8);
    }

    move(pos);
}

} // namespace DesktopTranslate
