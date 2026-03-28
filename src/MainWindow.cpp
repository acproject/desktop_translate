#include "MainWindow.h"
#include "SelectionOverlay.h"
#include "TranslationResultWindow.h"
#include "TranslationService.h"
#include "Config.h"
#include "ClipboardManager.h"
#include "TestWindow.h"
#include "GlobalShortcut.h"
#include "OCRService.h"
#include <QApplication>
#include <QMessageBox>
#include <QInputDialog>
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
#include <QLineEdit>

namespace DesktopTranslate {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUI();
    setupSystemTray();
    setupShortcuts();
    setupConnections();
    
    // 隐藏主窗口（托盘应用）
    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI() {
    // 初始化选区覆盖层
    selection_overlay_ = std::make_unique<SelectionOverlay>(this);
    
    // 初始化翻译结果窗口
    result_window_ = std::make_unique<TranslationResultWindow>(this);
    
    // 初始化测试窗口 - 使用nullptr作为父窗口使其成为独立窗口
    test_window_ = std::make_unique<TestWindow>(nullptr);
}

void MainWindow::setupSystemTray() {
    auto& config = Config::instance();
    
    // 创建系统托盘图标
    tray_icon_ = std::make_unique<QSystemTrayIcon>(this);
    
    // 设置图标（使用系统默认图标或自定义图标）
    QIcon icon = QIcon::fromTheme("edit-copy", QApplication::style()->standardIcon(QStyle::SP_FileDialogContentsView));
    tray_icon_->setIcon(icon);
    tray_icon_->setToolTip(tr("桌面翻译"));
    
    // 创建托盘菜单
    tray_menu_ = std::make_unique<QMenu>(this);
    
    action_select_translate_ = new QAction(tr("框选翻译 (&S)"), this);
    action_select_translate_->setShortcut(QKeySequence(QString::fromStdString(config.getShortcutSelectTranslate())));
    action_clipboard_translate_ = new QAction(tr("剪贴板翻译 (&C)"), this);
    action_clipboard_translate_->setShortcut(QKeySequence(QString::fromStdString(config.getShortcutClipboardTranslate())));
    action_test_window_ = new QAction(tr("显示测试窗口 (&T)"), this);
    action_settings_ = new QAction(tr("设置 (&O)"), this);
    action_about_ = new QAction(tr("关于 (&A)"), this);
    action_exit_ = new QAction(tr("退出 (&X)"), this);
    
    tray_menu_->addAction(action_select_translate_);
    tray_menu_->addAction(action_clipboard_translate_);
    tray_menu_->addAction(action_test_window_);
    tray_menu_->addSeparator();
    tray_menu_->addAction(action_settings_);
    tray_menu_->addAction(action_about_);
    tray_menu_->addSeparator();
    tray_menu_->addAction(action_exit_);
    
    tray_icon_->setContextMenu(tray_menu_.get());
    tray_icon_->show();
}

void MainWindow::setupShortcuts() {
    auto& config = Config::instance();
    
    qDebug() << "Setting up global shortcuts using X11...";
    qDebug() << "Select shortcut:" << QString::fromStdString(config.getShortcutSelectTranslate());
    qDebug() << "Clipboard shortcut:" << QString::fromStdString(config.getShortcutClipboardTranslate());
    
    // 使用 X11 全局快捷键
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
    
    // 连接全局快捷键信号
    connect(&globalShortcut, &GlobalShortcut::shortcutActivated, 
            this, [this](const QString& id) {
        qDebug() << "Global shortcut triggered:" << id;
        if (id == "select_translate") {
            startSelectionTranslation();
        } else if (id == "clipboard_translate") {
            translateFromClipboard();
        }
    });
    
    qDebug() << "Global shortcuts registered successfully";
}

void MainWindow::setupConnections() {
    // 托盘菜单连接
    connect(action_select_translate_, &QAction::triggered, this, &MainWindow::startSelectionTranslation);
    connect(action_clipboard_translate_, &QAction::triggered, this, &MainWindow::translateFromClipboard);
    connect(action_test_window_, &QAction::triggered, this, &MainWindow::showTestWindow);
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
    connect(test_window_.get(), &TestWindow::translateTextRequested, this, [this](const QString& text) {
        current_selection_pos_ = QPoint(100, 100);  // 默认位置
        performTranslation(text);
    });
    
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
        // 在后台线程进行 OCR 识别
        std::thread([this, rect]() {
            auto result = OCRService::instance().recognizeScreenArea(
                rect.x(), rect.y(), rect.width(), rect.height()
            );
            
            // 回到主线程处理结果
            QMetaObject::invokeMethod(this, [this, result, rect]() {
                if (result.success && !result.text.isEmpty()) {
                    test_window_->log(tr("OCR 识别成功，获取到文本: %1").arg(
                        result.text.left(50) + (result.text.length() > 50 ? "..." : "")), "SUCCESS");
                    test_window_->setStatus(tr("翻译中"), "blue");
                    performTranslation(result.text);
                } else {
                    test_window_->log(tr("OCR 识别失败: %1").arg(result.error), "ERROR");
                    test_window_->setStatus(tr("OCR 失败"), "red");
                    tray_icon_->showMessage(tr("OCR 识别失败"), result.error, QSystemTrayIcon::Warning, 2000);
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
    timeoutSpin->setRange(10, 300);
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
    
    // 快捷键设置
    auto* shortcutGroup = new QGroupBox(tr("快捷键设置"), &dialog);
    auto* shortcutLayout = new QFormLayout(shortcutGroup);
    
    auto* selectShortcutEdit = new QLineEdit(QString::fromStdString(config.getShortcutSelectTranslate()), &dialog);
    selectShortcutEdit->setPlaceholderText(tr("例如: Ctrl+F3"));
    auto* clipboardShortcutEdit = new QLineEdit(QString::fromStdString(config.getShortcutClipboardTranslate()), &dialog);
    clipboardShortcutEdit->setPlaceholderText(tr("例如: Ctrl+F4"));
    
    shortcutLayout->addRow(tr("框选翻译:"), selectShortcutEdit);
    shortcutLayout->addRow(tr("剪贴板翻译:"), clipboardShortcutEdit);
    
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
        
        config.save();
        
        // 更新翻译服务配置
        TranslationService::instance().setEndpoint(endpointEdit->text().toStdString(), portSpin->value());
        TranslationService::instance().setApiKey(keyEdit->text().toStdString());
        TranslationService::instance().setModel(modelEdit->text().toStdString());
        TranslationService::instance().setLanguages(sourceCombo->currentText().toStdString(), 
                                                     targetCombo->currentText().toStdString());
        
        // 更新 OCR 服务配置
        OCRService::instance().setEndpoint(ocrEndpointEdit->text().toStdString(), ocrPortSpin->value());
        OCRService::instance().setApiKey(ocrKeyEdit->text().toStdString());
        OCRService::instance().setModel(ocrModelEdit->text().toStdString());
        
        // 更新全局快捷键
        auto& globalShortcut = GlobalShortcut::instance();
        globalShortcut.unregisterShortcut("select_translate");
        globalShortcut.unregisterShortcut("clipboard_translate");
        globalShortcut.registerShortcut(selectShortcutEdit->text(), "select_translate");
        globalShortcut.registerShortcut(clipboardShortcutEdit->text(), "clipboard_translate");
        
        // 更新测试窗口配置显示
        updateConfigDisplay();
        test_window_->log(tr("配置已更新并保存"), "SUCCESS");
        
        tray_icon_->showMessage(tr("设置"), tr("设置已保存"), QSystemTrayIcon::Information, 2000);
    }
}

void MainWindow::onAboutAction() {
    QMessageBox::about(this, tr("关于"),
        tr("<h3>桌面翻译工具</h3>"
           "<p>版本: 1.0.0</p>"
           "<p>一个简单的桌面翻译工具，支持框选翻译。</p>"
           "<p>使用本地大模型API（OpenAI兼容）进行翻译。</p>"
           "<hr>"
           "<p><b>快捷键:</b></p>"
           "<p>Ctrl+Shift+S - 框选翻译</p>"
           "<p>Ctrl+Shift+C - 剪贴板翻译</p>"));
}

void MainWindow::onExitAction() {
    QApplication::quit();
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
        "框选翻译快捷键: %7\n"
        "剪贴板翻译快捷键: %8"
    ).arg(QString::fromStdString(config.getApiEndpoint()))
     .arg(config.getApiPort())
     .arg(QString::fromStdString(config.getModel()))
     .arg(QString::fromStdString(config.getSourceLanguage()))
     .arg(QString::fromStdString(config.getTargetLanguage()))
     .arg(config.getApiTimeout())
     .arg(QString::fromStdString(config.getShortcutSelectTranslate()))
     .arg(QString::fromStdString(config.getShortcutClipboardTranslate()));
    
    test_window_->showConfig(configInfo);
}

void MainWindow::updateTestWindowConfig() {
    updateConfigDisplay();
}

} // namespace DesktopTranslate
