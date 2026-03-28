#include "TestWindow.h"
#include "Config.h"
#include <QDateTime>
#include <QScrollBar>
#include <QGroupBox>
#include <QScreen>
#include <QGuiApplication>
#include <QDebug>

namespace DesktopTranslate {

TestWindow::TestWindow(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    start_time_ = std::chrono::steady_clock::now();
}

void TestWindow::setupUI() {
    setWindowTitle(tr("翻译工具 - 测试窗口"));
    setMinimumSize(600, 500);
    resize(700, 600);
    
    // 设置窗口标志为独立顶层窗口
    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
    
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);
    
    // 状态栏
    auto* statusLayout = new QHBoxLayout();
    auto* statusTitleLabel = new QLabel(tr("状态: "), this);
    statusTitleLabel->setStyleSheet("font-weight: bold;");
    statusLayout->addWidget(statusTitleLabel);
    
    status_label_ = new QLabel(tr("就绪"), this);
    status_label_->setStyleSheet("color: blue; font-weight: bold; padding: 5px; background-color: #e8f4fd; border-radius: 3px;");
    statusLayout->addWidget(status_label_);
    statusLayout->addStretch();
    mainLayout->addLayout(statusLayout);
    
    // 配置信息区域
    auto* configGroup = new QGroupBox(tr("当前配置"), this);
    auto* configLayout = new QVBoxLayout(configGroup);
    
    config_label_ = new QLabel(this);
    config_label_->setStyleSheet(
        "QLabel { background-color: #f5f5f5; padding: 8px; border-radius: 4px; font-family: monospace; }"
    );
    config_label_->setWordWrap(true);
    config_label_->setText(tr("加载中..."));
    configLayout->addWidget(config_label_);
    
    mainLayout->addWidget(configGroup);
    
    // 日志区域
    auto* logGroup = new QGroupBox(tr("操作日志"), this);
    auto* logLayout = new QVBoxLayout(logGroup);
    
    log_text_ = new QTextEdit(this);
    log_text_->setReadOnly(true);
    log_text_->setStyleSheet(
        "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; font-family: monospace; font-size: 12px; border: 1px solid #333; border-radius: 4px; padding: 5px; }"
    );
    logLayout->addWidget(log_text_);
    
    mainLayout->addWidget(logGroup);
    
    // 按钮区域
    auto* buttonLayout = new QHBoxLayout();
    
    test_api_button_ = new QPushButton(tr("测试API连接"), this);
    test_api_button_->setStyleSheet(
        "QPushButton { background-color: #28a745; color: white; border: none; border-radius: 4px; padding: 8px 16px; min-width: 120px; }"
        "QPushButton:hover { background-color: #218838; }"
        "QPushButton:pressed { background-color: #1e7e34; }"
    );
    buttonLayout->addWidget(test_api_button_);
    
    clear_button_ = new QPushButton(tr("清空日志"), this);
    clear_button_->setStyleSheet(
        "QPushButton { background-color: #6c757d; color: white; border: none; border-radius: 4px; padding: 8px 16px; min-width: 100px; }"
        "QPushButton:hover { background-color: #5a6268; }"
        "QPushButton:pressed { background-color: #545b62; }"
    );
    buttonLayout->addWidget(clear_button_);
    
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
    
    // 手动输入测试区域
    auto* testGroup = new QGroupBox(tr("手动测试"), this);
    auto* testLayout = new QVBoxLayout(testGroup);
    
    auto* testInputLabel = new QLabel(tr("输入要翻译的文本:"), this);
    testLayout->addWidget(testInputLabel);
    
    auto* testInputEdit = new QTextEdit(this);
    testInputEdit->setMaximumHeight(80);
    testInputEdit->setPlaceholderText(tr("在此输入文本测试翻译功能..."));
    testLayout->addWidget(testInputEdit);
    
    auto* translateBtn = new QPushButton(tr("翻译"), this);
    translateBtn->setStyleSheet(
        "QPushButton { background-color: #007bff; color: white; border: none; border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background-color: #0056b3; }"
    );
    testLayout->addWidget(translateBtn);
    
    mainLayout->addWidget(testGroup);
    
    // 连接信号 - 使用Qt5风格的连接方式确保兼容性
    connect(clear_button_, &QPushButton::clicked, this, [this]() {
        qDebug() << "清空日志按钮被点击";
        if (log_text_) {
            log_text_->setText("");
            log_text_->append("<span style='color: #6bcb77;'>[INFO] 日志已清空</span>");
        }
    });
    
    connect(test_api_button_, &QPushButton::clicked, this, [this]() {
        qDebug() << "测试API连接按钮被点击";
        if (log_text_) {
            log_text_->append("<span style='color: #74b9ff;'>[INFO] 测试API连接按钮被点击</span>");
        }
        emit testApiRequested();
    });
    
    // 翻译按钮连接 - 需要获取输入框内容
    connect(translateBtn, &QPushButton::clicked, this, [this, testInputEdit]() {
        QString text = testInputEdit->toPlainText().trimmed();
        if (text.isEmpty()) {
            log_text_->append("<span style='color: #ffd93d;'>[WARN] 请输入要翻译的文本</span>");
            return;
        }
        log_text_->append(QString("<span style='color: #74b9ff;'>[INFO] 手动翻译: %1</span>").arg(text.left(50)));
        emit translateTextRequested(text);
    });
    
    // 初始日志 - 直接设置文本确保可见
    auto& config = Config::instance();
    log_text_->setText("");
    log_text_->append("<span style='color: #6bcb77;'>[INFO] 测试窗口已启动</span>");
    log_text_->append(QString("<span style='color: #74b9ff;'>[INFO] 快捷键: %1 框选翻译</span>")
        .arg(QString::fromStdString(config.getShortcutSelectTranslate())));
    log_text_->append(QString("<span style='color: #74b9ff;'>[INFO] 快捷键: %1 剪贴板翻译</span>")
        .arg(QString::fromStdString(config.getShortcutClipboardTranslate())));
    
    // 将窗口移动到屏幕中央
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }
    
    qDebug() << "TestWindow setupUI completed, window will be at screen center";
}

void TestWindow::log(const QString& message, const QString& level) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    
    QString colorCode;
    if (level == "ERROR") {
        colorCode = "#ff6b6b";
    } else if (level == "WARN") {
        colorCode = "#ffd93d";
    } else if (level == "SUCCESS") {
        colorCode = "#6bcb77";
    } else {
        colorCode = "#74b9ff";
    }
    
    QString logEntry = QString("<span style='color: #888;'>[%1]</span> <span style='color: %2;'>[%3]</span> %4")
        .arg(timestamp)
        .arg(colorCode)
        .arg(level)
        .arg(message);
    
    log_text_->append(logEntry);
    
    // 自动滚动到底部
    QScrollBar* sb = log_text_->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void TestWindow::clearLog() {
    log_text_->clear();
    log(tr("日志已清空"), "INFO");
}

void TestWindow::setStatus(const QString& status, const QString& color) {
    status_label_->setText(status);
    status_label_->setStyleSheet(QString("color: %1; font-weight: bold; padding: 5px; background-color: #e8f4fd; border-radius: 3px;").arg(color));
}

void TestWindow::showConfig(const QString& configInfo) {
    config_label_->setText(configInfo);
}

} // namespace DesktopTranslate
