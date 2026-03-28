#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <filesystem>

namespace DesktopTranslate {

/**
 * @brief 应用程序配置管理类
 */
class Config {
public:
    static Config& instance();

    // API 配置
    std::string getApiEndpoint() const;
    std::string getApiKey() const;
    std::string getModel() const;
    int getApiPort() const;
    int getApiTimeout() const;

    // 翻译配置
    std::string getSourceLanguage() const;
    std::string getTargetLanguage() const;

    // UI 配置
    int getSelectionOpacity() const;
    std::string getSelectionColor() const;

    // 快捷键配置
    std::string getShortcutSelectTranslate() const;
    std::string getShortcutClipboardTranslate() const;
    void setShortcutSelectTranslate(const std::string& shortcut);
    void setShortcutClipboardTranslate(const std::string& shortcut);

    // 设置方法
    void setApiEndpoint(const std::string& endpoint);
    void setApiPort(int port);
    void setApiKey(const std::string& key);
    void setModel(const std::string& model);
    void setSourceLanguage(const std::string& lang);
    void setTargetLanguage(const std::string& lang);

    // 加载/保存配置
    bool load();
    bool save();

private:
    Config();
    ~Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    std::filesystem::path getConfigPath() const;

    // 配置数据
    std::string api_endpoint_{"http://127.0.0.1"};
    int api_port_{8110};
    std::string api_key_{""};  // 本地服务通常不需要
    std::string model_{"Qwen3.5-122B-A10B-GGUF"};
    int api_timeout_{60};

    std::string source_language_{"auto"};
    std::string target_language_{"zh"};

    int selection_opacity_{50};
    std::string selection_color_{"#4A90D9"};

    // 快捷键配置
    std::string shortcut_select_translate_{"Ctrl+F3"};
    std::string shortcut_clipboard_translate_{"Ctrl+F4"};
};

} // namespace DesktopTranslate

#endif // CONFIG_H
