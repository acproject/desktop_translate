#ifndef DICTIONARY_WINDOW_H
#define DICTIONARY_WINDOW_H

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QLabel>
#include <string>

namespace DesktopTranslate {

struct DictionaryResult;

/**
 * @brief 字典查询窗口 - 独立顶层窗口
 *
 * 提供搜索输入、语言选择、结果展示（HTML格式），
 * 支持回车触发查询，结果包含拼音/国际音标、词性、释义、例句。
 */
class DictionaryWindow : public QWidget {
    Q_OBJECT

public:
    explicit DictionaryWindow(QWidget* parent = nullptr);
    ~DictionaryWindow() override = default;

    // 设置查询结果显示
    void setResult(const DictionaryResult& result);

    // 设置状态提示
    void setStatus(const QString& status, const QString& color = "blue");

    // 清空内容
    void clear();

signals:
    // 查询请求信号
    void lookupRequested(const QString& word, const QString& language);

private:
    void setupUI();
    QString formatResultAsHtml(const DictionaryResult& result) const;
    QString escapeHtml(const QString& text) const;

    QLineEdit* search_input_{nullptr};
    QComboBox* language_combo_{nullptr};
    QPushButton* search_button_{nullptr};
    QTextBrowser* result_display_{nullptr};
    QLabel* status_label_{nullptr};
    QPushButton* copy_button_{nullptr};
    QPushButton* clear_button_{nullptr};
};

} // namespace DesktopTranslate

#endif // DICTIONARY_WINDOW_H
