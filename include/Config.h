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

    // OCR 服务配置
    std::string getOcrEndpoint() const;
    int getOcrPort() const;
    std::string getOcrApiKey() const;
    std::string getOcrModel() const;

    // 翻译配置
    std::string getSourceLanguage() const;
    std::string getTargetLanguage() const;

    // UI 配置
    int getSelectionOpacity() const;
    std::string getSelectionColor() const;

    // 高级翻译器配置
    bool getUseAdvancedModel() const;
    bool getKeepAdvancedSetting() const;
    void setUseAdvancedModel(bool use);
    void setKeepAdvancedSetting(bool keep);

    // 快捷键配置
    std::string getShortcutSelectTranslate() const;
    std::string getShortcutClipboardTranslate() const;
    std::string getShortcutHoverTranslationToggle() const;
    std::string getShortcutDictionary() const;
    void setShortcutSelectTranslate(const std::string& shortcut);
    void setShortcutClipboardTranslate(const std::string& shortcut);
    void setShortcutHoverTranslationToggle(const std::string& shortcut);
    void setShortcutDictionary(const std::string& shortcut);

    // 设置方法
    void setApiEndpoint(const std::string& endpoint);
    void setApiPort(int port);
    void setApiKey(const std::string& key);
    void setModel(const std::string& model);
    void setSourceLanguage(const std::string& lang);
    void setTargetLanguage(const std::string& lang);
    void setApiTimeout(int timeout);

    // OCR 设置方法
    void setOcrEndpoint(const std::string& endpoint);
    void setOcrPort(int port);
    void setOcrApiKey(const std::string& key);
    void setOcrModel(const std::string& model);

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
    std::string model_{"HY-MT1.5-1.8B-Q8_0"};
    int api_timeout_{180};  // 默认3分钟，长文本需要更多时间

    // OCR 服务配置
    std::string ocr_endpoint_{"http://127.0.0.1"};
    int ocr_port_{8111};
    std::string ocr_api_key_{""};
    std::string ocr_model_{"PaddleOCR-VL-1.5-GGUF"};

    std::string source_language_{"auto"};
    std::string target_language_{"zh"};

    int selection_opacity_{50};
    std::string selection_color_{"#4A90D9"};

    // 快捷键配置
    std::string shortcut_select_translate_{"Ctrl+F3"};
    std::string shortcut_clipboard_translate_{"Ctrl+F4"};
    std::string shortcut_hover_translation_toggle_{"Ctrl+F8"};
    std::string shortcut_dictionary_{"Ctrl+F5"};

    // 高级翻译器配置
    bool use_advanced_model_{false};
    bool keep_advanced_setting_{false};
};

} // namespace DesktopTranslate

#endif // CONFIG_H
