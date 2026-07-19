#include "MainWindow.h"
#include "SelectionOverlay.h"
#include "TranslationResultWindow.h"
#include "TranslationService.h"
#include "Config.h"
#include "ClipboardManager.h"
#include "TestWindow.h"
#include "GlobalShortcut.h"
#include "OCRService.h"
#include "DictionaryWindow.h"
#include "DictionaryService.h"
#include "ModelServiceManager.h"
#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QFile>
#include <QIcon>
#include <QMessageBox>
#include <QInputDialog>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QDebug>
#include <thread>
#include <QStyle>
#include <QGroupBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDialog>
#include <QDateTime>
#include <QLineEdit>
#include <QPointer>
#include <QCheckBox>
#include <QLabel>

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace DesktopTranslate {

namespace {

QIcon loadTrayIcon() {
    const QStringList candidates = {
        QStringLiteral(DESKTOP_TRANSLATE_SOURCE_ICON),
        QApplication::applicationDirPath() + "/icons8-translate-100.png",
        QApplication::applicationDirPath() + "/desktop-translate.png",
        "/usr/share/pixmaps/desktop-translate.png"
    };

    for (const QString& path : candidates) {
        if (QFile::exists(path)) {
            QIcon icon(path);
            if (!icon.isNull()) {
                return icon;
            }
        }
    }

    return QApplication::windowIcon();
}

QString currentHoverSourceText(QClipboard* clipboard) {
    if (!clipboard) {
        return {};
    }

    if (clipboard->supportsSelection()) {
        return clipboard->text(QClipboard::Selection).trimmed();
    }

    return clipboard->text(QClipboard::Clipboard).trimmed();
}

QString hoverSourceDescription(QClipboard* clipboard, const QObject* context) {
    if (clipboard && clipboard->supportsSelection()) {
        return context->tr("选中文本");
    }

    return context->tr("剪贴板文本");
}

// 检测文本是否像错误消息，避免悬浮翻译反馈循环
bool looksLikeErrorMessage(const QString& text) {
    static const QStringList errorMarkers = {
        QStringLiteral("查询失败"),
        QStringLiteral("翻译失败"),
        QStringLiteral("CURL error"),
        QStringLiteral("模型服务未运行"),
        QStringLiteral("JSON parse error"),
        QStringLiteral("Empty translation"),
        QStringLiteral("HTTP error"),
        QStringLiteral("Failed to initialize")
    };
    for (const auto& marker : errorMarkers) {
        if (text.contains(marker, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);

    setupUI();
    setupSystemTray();
    setupShortcuts();
    setupConnections();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI() {
    // 初始化选区覆盖层
    selection_overlay_ = std::make_unique<SelectionOverlay>(this);
    
    // 初始化翻译结果窗口
    result_window_ = std::make_unique<TranslationResultWindow>(this);
    result_window_->setWindowIcon(QApplication::windowIcon());
    
    // 初始化测试窗口 - 使用nullptr作为父窗口使其成为独立窗口
    test_window_ = std::make_unique<TestWindow>(nullptr);
    test_window_->setWindowIcon(QApplication::windowIcon());

    // 初始化字典窗口 - 独立顶层窗口
    dictionary_window_ = std::make_unique<DictionaryWindow>(nullptr);
    dictionary_window_->setWindowIcon(QApplication::windowIcon());

    result_window_->hide();

    primary_selection_timer_ = new QTimer(this);
    primary_selection_timer_->setSingleShot(true);
    windows_hover_poll_timer_ = new QTimer(this);
    windows_hover_poll_timer_->setInterval(120);
}

void MainWindow::setupSystemTray() {
    auto& config = Config::instance();
    
    tray_icon_ = std::make_unique<QSystemTrayIcon>(this);
    
    QIcon icon = loadTrayIcon();
    if (icon.isNull()) {
        icon = QIcon::fromTheme("edit-copy", QApplication::style()->standardIcon(QStyle::SP_FileDialogContentsView));
    }
    tray_icon_->setIcon(icon);
    tray_icon_->setToolTip(tr("桌面翻译"));
    
    tray_menu_ = std::make_unique<QMenu>();
    
    action_select_translate_ = new QAction(tr("框选翻译 (&S)"), this);
    action_select_translate_->setShortcut(QKeySequence(QString::fromStdString(config.getShortcutSelectTranslate())));
    action_select_translate_->setVisible(true);
    action_select_translate_->setEnabled(true);
    
    action_clipboard_translate_ = new QAction(tr("剪贴板翻译 (&C)"), this);
    action_clipboard_translate_->setShortcut(QKeySequence(QString::fromStdString(config.getShortcutClipboardTranslate())));
    action_clipboard_translate_->setVisible(true);
    action_clipboard_translate_->setEnabled(true);
    
    action_test_window_ = new QAction(tr("显示测试窗口 (&T)"), this);
    action_test_window_->setVisible(true);
    action_test_window_->setEnabled(true);
    
    action_dictionary_ = new QAction(tr("字典查询 (&D)"), this);
    action_dictionary_->setShortcut(QKeySequence(QString::fromStdString(config.getShortcutDictionary())));
    action_dictionary_->setVisible(true);
    action_dictionary_->setEnabled(true);
    
    action_hover_window_ = new QAction(tr("悬浮气泡翻译 (&H)"), this);
    action_hover_window_->setShortcut(QKeySequence(QString::fromStdString(config.getShortcutHoverTranslationToggle())));
    action_hover_window_->setCheckable(true);
    action_hover_window_->setChecked(hover_translation_enabled_);
    action_hover_window_->setVisible(true);
    action_hover_window_->setEnabled(true);
    
    action_settings_ = new QAction(tr("设置 (&O)"), this);
    action_settings_->setVisible(true);
    action_settings_->setEnabled(true);
    
    action_about_ = new QAction(tr("关于 (&A)"), this);
    action_about_->setVisible(true);
    action_about_->setEnabled(true);
    
    action_exit_ = new QAction(tr("退出 (&X)"), this);
    action_exit_->setVisible(true);
    action_exit_->setEnabled(true);
    
    tray_menu_->addAction(action_select_translate_);
    tray_menu_->addAction(action_clipboard_translate_);
    tray_menu_->addAction(action_test_window_);
    tray_menu_->addAction(action_dictionary_);
    tray_menu_->addAction(action_hover_window_);
    tray_menu_->addSeparator();
    tray_menu_->addAction(action_settings_);
    tray_menu_->addAction(action_about_);
    tray_menu_->addSeparator();
    tray_menu_->addAction(action_exit_);
    
    tray_icon_->setContextMenu(tray_menu_.get());
    tray_icon_->show();

#if defined(Q_OS_WIN)
    QTimer::singleShot(100, this, [this]() {
        if (tray_icon_) {
            tray_icon_->setContextMenu(nullptr);
            tray_icon_->setContextMenu(tray_menu_.get());
        }
    });
#endif
    
    qDebug() << "Tray menu actions count:" << tray_menu_->actions().size();
    for (QAction* act : tray_menu_->actions()) {
        qDebug() << "  -" << act->text() << "visible:" << act->isVisible() << "enabled:" << act->isEnabled();
    }
}

void MainWindow::setupShortcuts() {
    auto& config = Config::instance();
    
    qDebug() << "Setting up global shortcuts...";
    qDebug() << "Select shortcut:" << QString::fromStdString(config.getShortcutSelectTranslate());
    qDebug() << "Clipboard shortcut:" << QString::fromStdString(config.getShortcutClipboardTranslate());
    qDebug() << "Hover toggle shortcut:" << QString::fromStdString(config.getShortcutHoverTranslationToggle());
    
    auto& globalShortcut = GlobalShortcut::instance();
    
    // 注册框选翻译快捷键
    if (!globalShortcut.registerShortcut(
            QString::fromStdString(config.getShortcutSelectTranslate()), "select_translate")) {
        qWarning() << "Failed to register select shortcut";
    }
    
    // 注册剪贴板翻译快捷键
    if (!globalShortcut.registerShortcut(
            QString::fromStdString(config.getShortcutClipboardTranslate()), "clipboard_translate")) {
        qWarning() << "Failed to register clipboard shortcut";
    }

    if (!globalShortcut.registerShortcut(
            QString::fromStdString(config.getShortcutHoverTranslationToggle()), "hover_translation_toggle")) {
        qWarning() << "Failed to register hover translation toggle shortcut";
    }

    if (!globalShortcut.registerShortcut(
            QString::fromStdString(config.getShortcutDictionary()), "dictionary_lookup")) {
        qWarning() << "Failed to register dictionary shortcut";
    }
    
    // 连接全局快捷键信号
    connect(&globalShortcut, &GlobalShortcut::shortcutActivated, 
            this, [this](const QString& id) {
        qDebug() << "Global shortcut triggered:" << id;
        if (id == "select_translate") {
            startSelectionTranslation();
        } else if (id == "clipboard_translate") {
            translateFromClipboard();
        } else if (id == "hover_translation_toggle") {
            action_hover_window_->setChecked(!action_hover_window_->isChecked());
        } else if (id == "dictionary_lookup") {
            showDictionaryWindow();
        }
    });
    
    qDebug() << "Global shortcuts registered successfully";
}

void MainWindow::setupConnections() {
    // 托盘菜单连接
    connect(action_select_translate_, &QAction::triggered, this, &MainWindow::startSelectionTranslation);
    connect(action_clipboard_translate_, &QAction::triggered, this, &MainWindow::translateFromClipboard);
    connect(action_test_window_, &QAction::triggered, this, &MainWindow::showTestWindow);
    connect(action_dictionary_, &QAction::triggered, this, &MainWindow::showDictionaryWindow);
    connect(action_hover_window_, &QAction::toggled, this, &MainWindow::toggleHoverTranslation);
    connect(action_settings_, &QAction::triggered, this, &MainWindow::onSettingsAction);
    connect(action_about_, &QAction::triggered, this, &MainWindow::onAboutAction);
    connect(action_exit_, &QAction::triggered, this, &MainWindow::onExitAction);
    
    // 选区覆盖层连接
    connect(selection_overlay_.get(), &SelectionOverlay::selectionComplete, 
            this, &MainWindow::onSelectionComplete);
    connect(selection_overlay_.get(), &SelectionOverlay::selectionCancelled,
            this, &MainWindow::onSelectionCancelled);
    
    // 托盘图标点击
    connect(tray_icon_.get(), &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            startSelectionTranslation();
        }
    });
    
    // 测试窗口连接
    connect(test_window_.get(), &TestWindow::testApiRequested, this, &MainWindow::onTestApiConnection);

    // 字典窗口连接
    connect(dictionary_window_.get(), &DictionaryWindow::lookupRequested,
            this, [this](const QString& word, const QString& language) {
        dictionary_window_->setStatus(tr("查询中..."), "blue");
        DictionaryService::instance().lookUpWithCallback(
            word.toStdString(),
            language.toStdString(),
            [this](const DictionaryResult& result) {
                QMetaObject::invokeMethod(this, [this, result]() {
                    dictionary_window_->setResult(result);
                }, Qt::QueuedConnection);
            }
        );
    });
    connect(test_window_.get(), &TestWindow::translateTextRequested, this, [this](const QString& text) {
        current_selection_pos_ = QPoint(100, 100);  // 默认位置
        performTranslation(text);
    });
    connect(&ClipboardManager::instance(), &ClipboardManager::selectionCaptureLogged,
            test_window_.get(), &TestWindow::log);

    connect(result_window_.get(), &TranslationResultWindow::translateRequested,
            this, &MainWindow::onBubbleTranslateRequested);
    connect(primary_selection_timer_, &QTimer::timeout,
            this, &MainWindow::triggerPendingPrimaryTranslation);
    if (auto* clipboard = QApplication::clipboard()) {
        if (clipboard->supportsSelection()) {
            connect(clipboard, &QClipboard::selectionChanged,
                    this, &MainWindow::onPrimarySelectionChanged);
        } else {
            connect(clipboard, &QClipboard::dataChanged,
                    this, &MainWindow::onPrimarySelectionChanged);
        }
    }

#if defined(Q_OS_WIN)
    connect(windows_hover_poll_timer_, &QTimer::timeout,
            this, &MainWindow::pollWindowsHoverSelection);
    if (hover_translation_enabled_) {
        windows_hover_poll_timer_->start();
    }
#endif
    
    // 更新测试窗口配置显示
    updateConfigDisplay();
}

void MainWindow::startSelectionTranslation() {
    test_window_->log(tr("开始框选翻译..."), "INFO");
    test_window_->setStatus(tr("等待框选"), "orange");
    
    // 显示选区覆盖层
    selection_overlay_->startSelection();
    
    // 显示提示
    tray_icon_->showMessage(tr("框选翻译"), tr("请在屏幕上框选要翻译的文本区域"), QSystemTrayIcon::Information, 2000);
    test_window_->log(tr("选区覆盖层已显示，请在屏幕上框选"), "INFO");
}

void MainWindow::translateFromClipboard() {
    test_window_->log(tr("从剪贴板翻译..."), "INFO");
    
    QString text = ClipboardManager::instance().getFromClipboard();
    
    if (text.isEmpty()) {
        test_window_->log(tr("剪贴板为空"), "WARN");
        tray_icon_->showMessage(tr("提示"), tr("剪贴板为空"), QSystemTrayIcon::Warning, 2000);
        return;
    }
    
    test_window_->log(tr("剪贴板内容: %1").arg(text.left(50) + (text.length() > 50 ? "..." : "")), "INFO");
    performTranslation(text);
}

void MainWindow::onSelectionComplete(const QRect& rect) {
    qDebug() << "MainWindow::onSelectionComplete called, rect:" << rect;
    
    test_window_->log(tr("选区完成: (%1,%2) %3x%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()), "INFO");
    test_window_->setStatus(tr("OCR识别中"), "blue");
    
    current_selection_pos_ = rect.bottomRight();
    
    // 使用 OCR 识别选区文本
    test_window_->log(tr("正在对选区进行 OCR 识别..."), "INFO");
    
    // 延迟截图，确保覆盖层完全隐藏
    QTimer::singleShot(100, this, [this, rect]() {
        const QImage screenshot = OCRService::instance().captureScreenArea(
            rect.x(), rect.y(), rect.width(), rect.height()
        );

        if (screenshot.isNull()) {
            test_window_->log(tr("OCR 识别失败: %1").arg(tr("截图失败")), "ERROR");
            test_window_->setStatus(tr("OCR 失败"), "red");
            tray_icon_->showMessage(tr("OCR 识别失败"), tr("截图失败"), QSystemTrayIcon::Warning, 2000);
            return;
        }

        test_window_->showScreenshot(screenshot);

        QPointer<MainWindow> self(this);
        std::thread([self, screenshot]() {
            OCRResult result;
            result.screenshot = screenshot;
            auto ocrResult = OCRService::instance().recognizeText(screenshot);
            result.success = ocrResult.success;
            result.text = ocrResult.text;
            result.error = ocrResult.error;

            if (!self) {
                return;
            }

            QMetaObject::invokeMethod(self.data(), [self, result]() {
                if (!self) {
                    return;
                }

                if (result.success && !result.text.isEmpty()) {
                    self->test_window_->log(self->tr("OCR 识别成功，获取到文本: %1").arg(
                        result.text.left(50) + (result.text.length() > 50 ? "..." : "")), "SUCCESS");
                    self->test_window_->setStatus(self->tr("翻译中"), "blue");
                    self->performTranslation(result.text);
                } else {
                    self->test_window_->log(self->tr("OCR 识别失败: %1").arg(result.error), "ERROR");
                    self->test_window_->setStatus(self->tr("OCR 失败"), "red");
                    self->tray_icon_->showMessage(self->tr("OCR 识别失败"), result.error, QSystemTrayIcon::Warning, 2000);
                }
            }, Qt::QueuedConnection);
        }).detach();
    });
}

void MainWindow::onSelectionCancelled() {
    test_window_->log(tr("选区已取消"), "WARN");
    test_window_->setStatus(tr("已取消"), "orange");
    tray_icon_->showMessage(tr("提示"), tr("已取消选区"), QSystemTrayIcon::Information, 1000);
}

void MainWindow::performTranslation(const QString& text) {
    test_window_->log(tr("开始翻译..."), "INFO");
    test_window_->setStatus(tr("翻译中"), "blue");
    hover_translation_busy_ = true;
    
    // 显示翻译中提示
    tray_icon_->showMessage(tr("翻译中"), tr("正在请求翻译服务..."), QSystemTrayIcon::Information, 1000);
    
    auto& config = Config::instance();
    test_window_->log(tr("请求API: %1:%2/v1/chat/completions").arg(QString::fromStdString(config.getApiEndpoint())).arg(config.getApiPort()), "INFO");
    
    // 异步翻译
    TranslationService::instance().translateWithCallback(
        text.toStdString(),
        [this](const TranslationResult& result) {
            // 在主线程更新UI
            QMetaObject::invokeMethod(this, [this, result]() {
                onTranslationComplete(result);
            }, Qt::QueuedConnection);
        }
    );
}

void MainWindow::onTranslationComplete(const TranslationResult& result) {
    hover_translation_busy_ = false;

    if (result.success) {
        test_window_->log(tr("翻译成功!"), "SUCCESS");
        test_window_->setStatus(tr("翻译成功"), "green");
        test_window_->log(tr("原文: %1").arg(QString::fromStdString(result.original_text).left(100)), "INFO");
        test_window_->log(tr("译文: %1").arg(QString::fromStdString(result.translated_text).left(100)), "INFO");
        
        result_window_->setResult(result.original_text, result.translated_text, true);
        result_window_->showNear(current_selection_pos_);
    } else {
        test_window_->log(tr("翻译失败: %1").arg(QString::fromStdString(result.error_message)), "ERROR");
        test_window_->setStatus(tr("翻译失败"), "red");
        
        result_window_->setResult(result.original_text, result.error_message, false);
        result_window_->showNear(current_selection_pos_);
    }

    if (hover_translation_enabled_
        && !pending_primary_text_.isEmpty()
        && pending_primary_text_ != last_primary_text_) {
        primary_selection_timer_->start(200);
    }
}

void MainWindow::onBubbleTranslateRequested(const QString& text, const QPoint& globalPosition) {
    current_selection_pos_ = globalPosition;
    test_window_->log(tr("收到气泡拖放文本: %1").arg(text.left(50) + (text.length() > 50 ? "..." : "")), "INFO");
    performTranslation(text);
}

void MainWindow::toggleHoverTranslation(bool enabled) {
    hover_translation_enabled_ = enabled;
    if (!enabled) {
        pending_primary_text_.clear();
        last_primary_text_.clear();
        primary_selection_timer_->stop();
        left_mouse_button_down_ = false;
        left_mouse_button_pressed_at_ms_ = 0;
        left_mouse_button_pressed_pos_ = QPoint();
#if defined(Q_OS_WIN)
        windows_hover_poll_timer_->stop();
#endif
    } else {
#if defined(Q_OS_WIN)
        windows_hover_poll_timer_->start();
#endif
        onPrimarySelectionChanged();
    }

    tray_icon_->showMessage(
        tr("悬浮气泡翻译"),
        enabled ? tr("已开启，选中文本后将自动显示翻译气泡") : tr("已关闭"),
        QSystemTrayIcon::Information,
        1200
    );
}

void MainWindow::onPrimarySelectionChanged() {
    if (!hover_translation_enabled_) {
        return;
    }

    if (suppress_hover_clipboard_events_) {
        return;
    }

    auto* clipboard = QApplication::clipboard();
    if (!clipboard) {
        return;
    }

    const QString text = currentHoverSourceText(clipboard);
    if (text.isEmpty()) {
        pending_primary_text_.clear();
        last_primary_text_.clear();
        primary_selection_timer_->stop();
        return;
    }

    pending_primary_text_ = text;
    if (hover_translation_busy_) {
        return;
    }

    primary_selection_timer_->start(350);
}

void MainWindow::triggerPendingPrimaryTranslation() {
    if (!hover_translation_enabled_ || hover_translation_busy_) {
        return;
    }

    const QString text = pending_primary_text_.trimmed();
    if (text.isEmpty() || text == last_primary_text_) {
        return;
    }

    // 跳过错误消息，避免反馈循环
    if (looksLikeErrorMessage(text)) {
        qDebug() << "Skipping error-like text in hover translation:" << text.left(50);
        return;
    }

    last_primary_text_ = text;
    current_selection_pos_ = QCursor::pos();
    test_window_->log(
        tr("检测到%1: %2")
            .arg(hoverSourceDescription(QApplication::clipboard(), this))
            .arg(text.left(50) + (text.length() > 50 ? "..." : "")),
        "INFO"
    );
    performTranslation(text);
}

void MainWindow::pollWindowsHoverSelection() {
#if defined(Q_OS_WIN)
    const bool leftButtonDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (leftButtonDown && !left_mouse_button_down_) {
        left_mouse_button_down_ = true;
        left_mouse_button_pressed_at_ms_ = now;
        left_mouse_button_pressed_pos_ = QCursor::pos();
        return;
    }

    if (leftButtonDown) {
        return;
    }

    if (!left_mouse_button_down_) {
        return;
    }

    left_mouse_button_down_ = false;
    if (!hover_translation_enabled_ || hover_translation_busy_) {
        return;
    }

    if (left_mouse_button_pressed_at_ms_ == 0 || now - left_mouse_button_pressed_at_ms_ < 180) {
        return;
    }

    const QPoint releasePos = QCursor::pos();
    if ((releasePos - left_mouse_button_pressed_pos_).manhattanLength() < 8) {
        left_mouse_button_pressed_at_ms_ = 0;
        return;
    }

    left_mouse_button_pressed_at_ms_ = 0;
    suppress_hover_clipboard_events_ = true;
    const QString text = ClipboardManager::instance().captureSelectedTextFromActiveWindow();
    suppress_hover_clipboard_events_ = false;

    const QString trimmedText = text.trimmed();
    if (trimmedText.isEmpty() || trimmedText == last_primary_text_) {
        return;
    }

    pending_primary_text_ = trimmedText;
    primary_selection_timer_->start(120);
#endif
}

void MainWindow::onSettingsAction() {
    auto& config = Config::instance();
    
    QDialog dialog(this);
    dialog.setWindowTitle(tr("设置"));
    dialog.setMinimumWidth(400);
    
    auto* layout = new QVBoxLayout(&dialog);
    
    // API 设置
    auto* apiGroup = new QGroupBox(tr("翻译 API 设置"), &dialog);
    auto* apiLayout = new QFormLayout(apiGroup);
    
    auto* endpointEdit = new QLineEdit(QString::fromStdString(config.getApiEndpoint()), &dialog);
    auto* portSpin = new QSpinBox(&dialog);
    portSpin->setRange(1, 65535);
    portSpin->setValue(config.getApiPort());
    auto* modelEdit = new QLineEdit(QString::fromStdString(config.getModel()), &dialog);
    auto* keyEdit = new QLineEdit(QString::fromStdString(config.getApiKey()), &dialog);
    keyEdit->setEchoMode(QLineEdit::Password);
    auto* timeoutSpin = new QSpinBox(&dialog);
    timeoutSpin->setRange(30, 600);  // 最小30秒，最大10分钟
    timeoutSpin->setValue(config.getApiTimeout());
    timeoutSpin->setSuffix(tr(" 秒"));
    
    apiLayout->addRow(tr("API 地址:"), endpointEdit);
    apiLayout->addRow(tr("端口:"), portSpin);
    apiLayout->addRow(tr("模型:"), modelEdit);
    apiLayout->addRow(tr("API Key:"), keyEdit);
    apiLayout->addRow(tr("超时时间:"), timeoutSpin);
    
    layout->addWidget(apiGroup);
    
    // OCR 设置
    auto* ocrGroup = new QGroupBox(tr("OCR 服务设置"), &dialog);
    auto* ocrLayout = new QFormLayout(ocrGroup);
    
    auto* ocrEndpointEdit = new QLineEdit(QString::fromStdString(config.getOcrEndpoint()), &dialog);
    auto* ocrPortSpin = new QSpinBox(&dialog);
    ocrPortSpin->setRange(1, 65535);
    ocrPortSpin->setValue(config.getOcrPort());
    auto* ocrModelEdit = new QLineEdit(QString::fromStdString(config.getOcrModel()), &dialog);
    auto* ocrKeyEdit = new QLineEdit(QString::fromStdString(config.getOcrApiKey()), &dialog);
    ocrKeyEdit->setEchoMode(QLineEdit::Password);
    
    ocrLayout->addRow(tr("OCR 地址:"), ocrEndpointEdit);
    ocrLayout->addRow(tr("端口:"), ocrPortSpin);
    ocrLayout->addRow(tr("模型:"), ocrModelEdit);
    ocrLayout->addRow(tr("API Key:"), ocrKeyEdit);
    
    layout->addWidget(ocrGroup);
    
    // 翻译设置
    auto* transGroup = new QGroupBox(tr("翻译设置"), &dialog);
    auto* transLayout = new QFormLayout(transGroup);
    
    auto* sourceCombo = new QComboBox(&dialog);
    sourceCombo->addItems({"auto", "en", "zh", "ja", "ko", "fr", "de", "es"});
    sourceCombo->setCurrentText(QString::fromStdString(config.getSourceLanguage()));
    
    auto* targetCombo = new QComboBox(&dialog);
    targetCombo->addItems({"zh", "en", "ja", "ko", "fr", "de", "es"});
    targetCombo->setCurrentText(QString::fromStdString(config.getTargetLanguage()));
    
    transLayout->addRow(tr("源语言:"), sourceCombo);
    transLayout->addRow(tr("目标语言:"), targetCombo);
    
    layout->addWidget(transGroup);
    
    // 高级翻译器设置
    auto* advGroup = new QGroupBox(tr("高级翻译器"), &dialog);
    auto* advLayout = new QHBoxLayout(advGroup);
    advLayout->setSpacing(10);
    
    auto* advancedToggle = new QCheckBox(tr("开启高级翻译功能"), &dialog);
    advancedToggle->setChecked(config.getUseAdvancedModel());
    
    auto* advHintLabel = new QLabel(tr("（使用该功能需要至少8GB显存）"), &dialog);
    advHintLabel->setStyleSheet("color: #e67e22; font-size: 12px;");
    
    auto* keepSettingCheck = new QCheckBox(tr("是否保留此设置"), &dialog);
    keepSettingCheck->setChecked(config.getKeepAdvancedSetting());
    keepSettingCheck->setEnabled(advancedToggle->isChecked());
    
    // 当开关状态改变时，启用/禁用“保留”勾选框
    connect(advancedToggle, &QCheckBox::toggled, keepSettingCheck, [keepSettingCheck](bool checked) {
        keepSettingCheck->setEnabled(checked);
    });
    
    advLayout->addWidget(advancedToggle);
    advLayout->addWidget(advHintLabel);
    advLayout->addStretch();
    advLayout->addWidget(keepSettingCheck);
    
    layout->addWidget(advGroup);
    
    // 快捷键设置
    auto* shortcutGroup = new QGroupBox(tr("快捷键设置"), &dialog);
    auto* shortcutLayout = new QFormLayout(shortcutGroup);
    
    auto* selectShortcutEdit = new QLineEdit(QString::fromStdString(config.getShortcutSelectTranslate()), &dialog);
    selectShortcutEdit->setPlaceholderText(tr("例如: Ctrl+F3"));
    auto* clipboardShortcutEdit = new QLineEdit(QString::fromStdString(config.getShortcutClipboardTranslate()), &dialog);
    clipboardShortcutEdit->setPlaceholderText(tr("例如: Ctrl+F4"));
    auto* hoverShortcutEdit = new QLineEdit(QString::fromStdString(config.getShortcutHoverTranslationToggle()), &dialog);
    hoverShortcutEdit->setPlaceholderText(tr("例如: Ctrl+F8"));
    auto* dictionaryShortcutEdit = new QLineEdit(QString::fromStdString(config.getShortcutDictionary()), &dialog);
    dictionaryShortcutEdit->setPlaceholderText(tr("例如: Ctrl+F5"));
    
    shortcutLayout->addRow(tr("框选翻译:"), selectShortcutEdit);
    shortcutLayout->addRow(tr("剪贴板翻译:"), clipboardShortcutEdit);
    shortcutLayout->addRow(tr("悬浮气泡翻译开关:"), hoverShortcutEdit);
    shortcutLayout->addRow(tr("字典查询:"), dictionaryShortcutEdit);
    
    layout->addWidget(shortcutGroup);
    
    // 按钮
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    if (dialog.exec() == QDialog::Accepted) {
        // 保存翻译 API 配置
        config.setApiEndpoint(endpointEdit->text().toStdString());
        config.setApiPort(portSpin->value());
        config.setApiKey(keyEdit->text().toStdString());
        config.setModel(modelEdit->text().toStdString());
        config.setApiTimeout(timeoutSpin->value());
        
        // 保存 OCR 配置
        config.setOcrEndpoint(ocrEndpointEdit->text().toStdString());
        config.setOcrPort(ocrPortSpin->value());
        config.setOcrApiKey(ocrKeyEdit->text().toStdString());
        config.setOcrModel(ocrModelEdit->text().toStdString());
        
        // 保存翻译语言配置
        config.setSourceLanguage(sourceCombo->currentText().toStdString());
        config.setTargetLanguage(targetCombo->currentText().toStdString());
        
        // 保存快捷键设置
        config.setShortcutSelectTranslate(selectShortcutEdit->text().toStdString());
        config.setShortcutClipboardTranslate(clipboardShortcutEdit->text().toStdString());
        config.setShortcutHoverTranslationToggle(hoverShortcutEdit->text().toStdString());
        config.setShortcutDictionary(dictionaryShortcutEdit->text().toStdString());
        
        // 处理高级翻译器设置
        bool originalUseAdvanced = config.getUseAdvancedModel();
        bool newUseAdvanced = advancedToggle->isChecked();
        bool keepSetting = keepSettingCheck->isChecked();
        
        if (newUseAdvanced != originalUseAdvanced) {
            // 开关状态发生变化
            config.setUseAdvancedModel(newUseAdvanced);
            if (newUseAdvanced) {
                config.setModel("HY-MT1.5-7B-Q8_0");
            } else {
                config.setModel("HY-MT1.5-1.8B-Q8_0");
            }
            config.setKeepAdvancedSetting(keepSetting);
            
            // 切换模型服务（停止当前，启动新的）
            ModelServiceManager::instance().switchTranslationModel(newUseAdvanced);
            
            if (!keepSetting && newUseAdvanced) {
                // 不保留设置：先保存（use_advanced_model=false），再恢复内存状态
                config.setUseAdvancedModel(false);
                config.save();
                config.setUseAdvancedModel(true); // 仅内存中生效，不持久化
            } else {
                // 保留设置或取消高级：正常保存
                config.save();
            }
        } else {
            // 状态未变化，正常保存
            config.save();
        }
        
        // 更新翻译服务模型名
        std::string currentModel = config.getModel();
        
        // 更新翻译服务配置
        TranslationService::instance().setEndpoint(endpointEdit->text().toStdString(), portSpin->value());
        TranslationService::instance().setApiKey(keyEdit->text().toStdString());
        TranslationService::instance().setModel(currentModel);
        TranslationService::instance().setTimeout(timeoutSpin->value());
        TranslationService::instance().setLanguages(sourceCombo->currentText().toStdString(), 
                                                     targetCombo->currentText().toStdString());
        
        // 更新字典服务配置
        DictionaryService::instance().setModel(currentModel);
        DictionaryService::instance().setEndpoint(endpointEdit->text().toStdString(), portSpin->value());
        
        // 更新 OCR 服务配置
        OCRService::instance().setEndpoint(ocrEndpointEdit->text().toStdString(), ocrPortSpin->value());
        OCRService::instance().setApiKey(ocrKeyEdit->text().toStdString());
        OCRService::instance().setModel(ocrModelEdit->text().toStdString());
        
        // 更新全局快捷键
        auto& globalShortcut = GlobalShortcut::instance();
        globalShortcut.unregisterShortcut("select_translate");
        globalShortcut.unregisterShortcut("clipboard_translate");
        globalShortcut.unregisterShortcut("hover_translation_toggle");
        globalShortcut.unregisterShortcut("dictionary_lookup");
        globalShortcut.registerShortcut(selectShortcutEdit->text(), "select_translate");
        globalShortcut.registerShortcut(clipboardShortcutEdit->text(), "clipboard_translate");
        globalShortcut.registerShortcut(hoverShortcutEdit->text(), "hover_translation_toggle");
        globalShortcut.registerShortcut(dictionaryShortcutEdit->text(), "dictionary_lookup");
        action_select_translate_->setShortcut(QKeySequence(selectShortcutEdit->text()));
        action_clipboard_translate_->setShortcut(QKeySequence(clipboardShortcutEdit->text()));
        action_hover_window_->setShortcut(QKeySequence(hoverShortcutEdit->text()));
        action_dictionary_->setShortcut(QKeySequence(dictionaryShortcutEdit->text()));
        
        // 更新测试窗口配置显示
        updateConfigDisplay();
        test_window_->log(tr("配置已更新并保存"), "SUCCESS");
        
        tray_icon_->showMessage(tr("设置"), tr("设置已保存"), QSystemTrayIcon::Information, 2000);
    }
}

void MainWindow::onAboutAction() {
    QMessageBox::about(this, tr("关于"),
        tr("<h3>桌面翻译工具</h3>"
           "<p>版本: 1.5.0</p>"
           "<p>一个简单的桌面翻译工具，支持框选翻译。</p>"
           "<p>使用本地大模型API（OpenAI兼容）进行翻译。</p>"
           "<hr>"
           "<p><b>快捷键:</b></p>"
           "<p>%1 - 框选翻译</p>"
           "<p>%2 - 剪贴板翻译</p>"
           "<p>%3 - 悬浮气泡翻译开关</p>"
           "<p>%4 - 字典查询</p>")
            .arg(QString::fromStdString(Config::instance().getShortcutSelectTranslate()))
            .arg(QString::fromStdString(Config::instance().getShortcutClipboardTranslate()))
            .arg(QString::fromStdString(Config::instance().getShortcutHoverTranslationToggle()))
            .arg(QString::fromStdString(Config::instance().getShortcutDictionary())));
}

void MainWindow::onExitAction() {
    QApplication::quit();
}

void MainWindow::showDictionaryWindow() {
    qDebug() << "showDictionaryWindow called";

    if (!dictionary_window_) {
        qDebug() << "ERROR: dictionary_window_ is null!";
        return;
    }

    dictionary_window_->show();
    dictionary_window_->showNormal();
    dictionary_window_->activateWindow();
    dictionary_window_->raise();
    dictionary_window_->setWindowState(Qt::WindowActive);
    qDebug() << "Dictionary window shown, isVisible:" << dictionary_window_->isVisible();
}

void MainWindow::showTestWindow() {
    qDebug() << "showTestWindow called";
    qDebug() << "test_window_ pointer:" << test_window_.get();
    
    if (test_window_) {
        test_window_->show();
        test_window_->activateWindow();
        test_window_->raise();
        
        // 确保窗口在最前面
        test_window_->setWindowState(Qt::WindowActive);
        
        qDebug() << "Test window shown, isVisible:" << test_window_->isVisible();
        qDebug() << "Test window geometry:" << test_window_->geometry();
        
        updateConfigDisplay();
    } else {
        qDebug() << "ERROR: test_window_ is null!";
    }
}

void MainWindow::onTestApiConnection() {
    test_window_->log(tr("测试API连接..."), "INFO");
    test_window_->setStatus(tr("测试中"), "blue");
    
    auto& config = Config::instance();
    QString endpoint = QString::fromStdString(config.getApiEndpoint());
    int port = config.getApiPort();
    
    test_window_->log(tr("连接: %1:%2").arg(endpoint).arg(port), "INFO");
    
    // 发送一个简单的测试请求
    TranslationService::instance().translateWithCallback(
        "Hello",
        [this](const TranslationResult& result) {
            QMetaObject::invokeMethod(this, [this, result]() {
                if (result.success) {
                    test_window_->log(tr("API连接成功!"), "SUCCESS");
                    test_window_->setStatus(tr("连接成功"), "green");
                    test_window_->log(tr("测试翻译结果: %1").arg(QString::fromStdString(result.translated_text)), "INFO");
                } else {
                    test_window_->log(tr("API连接失败: %1").arg(QString::fromStdString(result.error_message)), "ERROR");
                    test_window_->setStatus(tr("连接失败"), "red");
                }
            }, Qt::QueuedConnection);
        }
    );
}

void MainWindow::updateConfigDisplay() {
    auto& config = Config::instance();
    QString configInfo = QString(
        "API地址: %1\n"
        "端口: %2\n"
        "模型: %3\n"
        "源语言: %4\n"
        "目标语言: %5\n"
        "超时: %6秒\n"
        "高级翻译器: %7\n"
        "框选翻译快捷键: %8\n"
        "剪贴板翻译快捷键: %9\n"
        "悬浮气泡翻译开关快捷键: %10\n"
        "字典查询快捷键: %11"
    ).arg(QString::fromStdString(config.getApiEndpoint()))
     .arg(config.getApiPort())
     .arg(QString::fromStdString(config.getModel()))
     .arg(QString::fromStdString(config.getSourceLanguage()))
     .arg(QString::fromStdString(config.getTargetLanguage()))
     .arg(config.getApiTimeout())
     .arg(config.getUseAdvancedModel() ? tr("已开启") : tr("未开启"))
     .arg(QString::fromStdString(config.getShortcutSelectTranslate()))
     .arg(QString::fromStdString(config.getShortcutClipboardTranslate()))
     .arg(QString::fromStdString(config.getShortcutHoverTranslationToggle()))
     .arg(QString::fromStdString(config.getShortcutDictionary()));
    
    test_window_->showConfig(configInfo);
}

void MainWindow::updateTestWindowConfig() {
    updateConfigDisplay();
}

} // namespace DesktopTranslate
