#include "DictionaryWindow.h"
#include "DictionaryService.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScreen>
#include <QGuiApplication>
#include <QApplication>
#include <QClipboard>
#include <QScrollBar>
#include <QDateTime>
#include <QKeyEvent>
#include <QTimer>
#include <QDebug>

namespace DesktopTranslate {

DictionaryWindow::DictionaryWindow(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void DictionaryWindow::setupUI() {
    setWindowTitle(tr("字典查询"));
    setMinimumSize(600, 500);
    resize(650, 600);

    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // 搜索区域
    auto* searchGroup = new QGroupBox(tr("查询"), this);
    auto* searchLayout = new QHBoxLayout(searchGroup);
    searchLayout->setSpacing(8);

    search_input_ = new QLineEdit(this);
    search_input_->setPlaceholderText(tr("输入要查询的单词或词组..."));
    search_input_->setStyleSheet(
        "QLineEdit { padding: 6px 8px; border: 1px solid #ccc; border-radius: 4px; font-size: 14px; }"
        "QLineEdit:focus { border-color: #4A90D9; }"
    );

    language_combo_ = new QComboBox(this);
    language_combo_->setStyleSheet(
        "QComboBox { padding: 6px 8px; border: 1px solid #ccc; border-radius: 4px; min-width: 100px; }"
    );
    // 语言选项: 显示文本, 数据为语言代码
    language_combo_->addItem(tr("自动"), "auto");
    language_combo_->addItem(tr("中文"), "zh");
    language_combo_->addItem(tr("英语"), "en");
    language_combo_->addItem(tr("日语"), "ja");
    language_combo_->addItem(tr("韩语"), "ko");
    language_combo_->addItem(tr("法语"), "fr");
    language_combo_->addItem(tr("德语"), "de");
    language_combo_->addItem(tr("西班牙语"), "es");
    language_combo_->addItem(tr("俄语"), "ru");
    language_combo_->addItem(tr("藏语"), "bo");

    search_button_ = new QPushButton(tr("查询"), this);
    search_button_->setStyleSheet(
        "QPushButton { background-color: #4A90D9; color: white; border: none; border-radius: 4px; padding: 8px 20px; font-size: 14px; }"
        "QPushButton:hover { background-color: #357ABD; }"
        "QPushButton:pressed { background-color: #2D6DB3; }"
        "QPushButton:disabled { background-color: #aaa; }"
    );

    searchLayout->addWidget(search_input_, 1);
    searchLayout->addWidget(language_combo_);
    searchLayout->addWidget(search_button_);

    mainLayout->addWidget(searchGroup);

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

    // 结果显示区域
    auto* resultGroup = new QGroupBox(tr("查询结果"), this);
    auto* resultLayout = new QVBoxLayout(resultGroup);

    result_display_ = new QTextBrowser(this);
    result_display_->setOpenExternalLinks(false);
    result_display_->setStyleSheet(
        "QTextBrowser {"
        "background-color: #fafafa;"
        "border: 1px solid #ddd;"
        "border-radius: 4px;"
        "padding: 10px;"
        "font-size: 14px;"
        "color: #222;"
        "}"
    );
    result_display_->setHtml(
        QString("<div style='color:#999; text-align:center; padding:40px;'>%1</div>")
            .arg(tr("在上方输入单词并点击查询按钮"))
    );

    resultLayout->addWidget(result_display_);
    mainLayout->addWidget(resultGroup, 1);

    // 底部按钮区域
    auto* buttonLayout = new QHBoxLayout();

    copy_button_ = new QPushButton(tr("复制结果"), this);
    copy_button_->setStyleSheet(
        "QPushButton { background-color: #28a745; color: white; border: none; border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background-color: #218838; }"
        "QPushButton:pressed { background-color: #1e7e34; }"
    );

    clear_button_ = new QPushButton(tr("清空"), this);
    clear_button_->setStyleSheet(
        "QPushButton { background-color: #6c757d; color: white; border: none; border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background-color: #5a6268; }"
        "QPushButton:pressed { background-color: #545b62; }"
    );

    buttonLayout->addWidget(copy_button_);
    buttonLayout->addWidget(clear_button_);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    // 连接信号
    connect(search_button_, &QPushButton::clicked, this, [this]() {
        QString word = search_input_->text().trimmed();
        if (word.isEmpty()) {
            setStatus(tr("请输入查询内容"), "orange");
            return;
        }
        QString lang = language_combo_->currentData().toString();
        emit lookupRequested(word, lang);
    });

    // 回车触发查询
    connect(search_input_, &QLineEdit::returnPressed, this, [this]() {
        search_button_->click();
    });

    connect(copy_button_, &QPushButton::clicked, this, [this]() {
        QString text = result_display_->toPlainText();
        if (text.isEmpty()) {
            return;
        }
        QApplication::clipboard()->setText(text);
        copy_button_->setText(tr("已复制"));
        QTimer::singleShot(2000, [this]() {
            copy_button_->setText(tr("复制结果"));
        });
    });

    connect(clear_button_, &QPushButton::clicked, this, [this]() {
        clear();
    });

    // 将窗口移动到屏幕中央
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }
}

QString DictionaryWindow::escapeHtml(const QString& text) const {
    QString escaped = text;
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    escaped.replace("'", "&#39;");
    // 保留换行符
    escaped.replace("\n", "<br>");
    return escaped;
}

QString DictionaryWindow::formatResultAsHtml(const DictionaryResult& result) const {
    QString word = QString::fromStdString(result.word);

    QString html;
    html.reserve(1024);

    // 词条标题行：word phonetic [lang] translation
    html += QString("<div style='margin-bottom:12px; padding-bottom:8px; border-bottom:1px solid #e0e0e0;'>");

    html += QString("<span style='font-size:22px; font-weight:bold; color:#1a1a1a;'>%1</span>")
                .arg(escapeHtml(word));

    QString phonetic = QString::fromStdString(result.phonetic);
    if (!phonetic.isEmpty()) {
        html += QString(" <span style='color:#666; font-size:16px; font-style:italic;'>%1</span>")
                    .arg(escapeHtml(phonetic));
    }

    // 语言标签
    QString langDisplay;
    QString langCode = QString::fromStdString(result.language);
    if (langCode == "auto") langDisplay = tr("自动检测");
    else if (langCode == "zh") langDisplay = tr("中文");
    else if (langCode == "en") langDisplay = tr("英语");
    else if (langCode == "ja") langDisplay = tr("日语");
    else if (langCode == "ko") langDisplay = tr("韩语");
    else if (langCode == "fr") langDisplay = tr("法语");
    else if (langCode == "de") langDisplay = tr("德语");
    else if (langCode == "es") langDisplay = tr("西班牙语");
    else if (langCode == "ru") langDisplay = tr("俄语");
    else if (langCode == "bo") langDisplay = tr("藏语");
    else langDisplay = langCode;

    html += QString(" <span style='color:#999; font-size:12px; margin-left:8px;'>[%1]</span>").arg(escapeHtml(langDisplay));

    // 翻译结果
    QString translation = QString::fromStdString(result.translation);
    if (!translation.isEmpty()) {
        html += QString(" <span style='font-size:20px; font-weight:bold; color:#333;'>%1</span>")
                    .arg(escapeHtml(translation));
    }

    html += "</div>";

    // 词性
    QString pos = QString::fromStdString(result.part_of_speech);
    if (!pos.isEmpty()) {
        html += QString("<p style='margin:4px 0;'><b style='color:#4A90D9;'>%1:</b> %2</p>")
                    .arg(tr("词性"))
                    .arg(escapeHtml(pos));
    }

    // 释义
    QString defs = QString::fromStdString(result.definitions);
    if (!defs.isEmpty()) {
        html += QString("<p style='margin:8px 0 4px 0;'><b style='color:#4A90D9;'>%1:</b></p>")
                    .arg(tr("释义"));
        html += QString("<div style='margin-left:12px; line-height:1.8;'>%1</div>")
                    .arg(escapeHtml(defs));
    }

    // 例句
    QString examples = QString::fromStdString(result.examples);
    if (!examples.isEmpty()) {
        html += QString("<p style='margin:8px 0 4px 0;'><b style='color:#4A90D9;'>%1:</b></p>")
                    .arg(tr("例句"));
        html += QString("<div style='margin-left:12px; line-height:1.8; color:#444;'>%1</div>")
                    .arg(escapeHtml(examples));
    }

    // 备注
    QString notes = QString::fromStdString(result.notes);
    if (!notes.isEmpty()) {
        html += QString("<p style='margin:8px 0 4px 0;'><b style='color:#4A90D9;'>%1:</b></p>")
                    .arg(tr("备注"));
        html += QString("<div style='margin-left:12px; line-height:1.6; color:#666; font-style:italic;'>%1</div>")
                    .arg(escapeHtml(notes));
    }

    // 如果结构化字段都为空，显示原始响应
    if (translation.isEmpty() && phonetic.isEmpty() && pos.isEmpty() && defs.isEmpty()
        && examples.isEmpty() && notes.isEmpty()
        && !result.raw_response.empty()) {
        html += QString("<div style='line-height:1.8;'>%1</div>")
                    .arg(escapeHtml(QString::fromStdString(result.raw_response)));
    }

    return html;
}

void DictionaryWindow::setResult(const DictionaryResult& result) {
    if (result.success) {
        result_display_->setHtml(formatResultAsHtml(result));
        setStatus(tr("查询成功"), "green");
    } else {
        QString errorMsg = QString::fromStdString(result.error_message);
        result_display_->setHtml(
            QString("<div style='color:#C62828; padding:20px;'>"
                    "<b>%1:</b><br>%2</div>")
                .arg(tr("查询失败"))
                .arg(escapeHtml(errorMsg))
        );
        setStatus(tr("查询失败"), "red");
    }

    // 滚动到顶部
    result_display_->verticalScrollBar()->setValue(0);
}

void DictionaryWindow::setStatus(const QString& status, const QString& color) {
    status_label_->setText(status);
    status_label_->setStyleSheet(
        QString("color: %1; font-weight: bold; padding: 5px; background-color: #e8f4fd; border-radius: 3px;")
            .arg(color)
    );
}

void DictionaryWindow::clear() {
    search_input_->clear();
    result_display_->setHtml(
        QString("<div style='color:#999; text-align:center; padding:40px;'>%1</div>")
            .arg(tr("在上方输入单词并点击查询按钮"))
    );
    setStatus(tr("就绪"), "blue");
    search_input_->setFocus();
}

} // namespace DesktopTranslate
