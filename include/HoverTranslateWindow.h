#ifndef HOVER_TRANSLATE_WINDOW_H
#define HOVER_TRANSLATE_WINDOW_H

#include <QWidget>
#include <QPoint>
#include <QString>

class QLabel;
class QPushButton;
class QDragEnterEvent;
class QDropEvent;
class QMouseEvent;
class QTimer;

namespace DesktopTranslate {

class HoverTranslateWindow : public QWidget {
    Q_OBJECT

public:
    explicit HoverTranslateWindow(QWidget* parent = nullptr);
    void setBusy(bool busy);

signals:
    void translateRequested(const QString& text, const QPoint& globalPosition);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void setupUI();
    void updatePrimaryPreview();
    void emitPrimarySelectionTranslation();
    void onPrimarySelectionChanged();
    void triggerPendingPrimaryTranslation();
    void showNearCursor();

    QLabel* title_label_{nullptr};
    QLabel* hint_label_{nullptr};
    QLabel* selection_preview_label_{nullptr};
    QPushButton* translate_primary_button_{nullptr};
    QPushButton* hide_button_{nullptr};
    QTimer* auto_translate_timer_{nullptr};
    QPoint drag_offset_;
    bool busy_{false};
    QString pending_primary_text_;
    QString last_auto_translated_text_;
};

} // namespace DesktopTranslate

#endif // HOVER_TRANSLATE_WINDOW_H
