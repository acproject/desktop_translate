#ifndef TRANSLATION_SERVICE_H
#define TRANSLATION_SERVICE_H

#include <string>
#include <functional>
#include <future>
#include <memory>

namespace DesktopTranslate {

/**
 * @brief 翻译结果结构
 */
struct TranslationResult {
    bool success{false};
    std::string original_text;
    std::string translated_text;
    std::string error_message;
};

/**
 * @brief 翻译服务类 - 调用本地大模型API（OpenAI兼容）
 */
class TranslationService {
public:
    using TranslationCallback = std::function<void(const TranslationResult&)>;

    static TranslationService& instance();

    // 同步翻译
    TranslationResult translate(const std::string& text);

    // 异步翻译
    std::future<TranslationResult> translateAsync(const std::string& text);
    
    // 回调方式翻译
    void translateWithCallback(const std::string& text, TranslationCallback callback);

    // 设置API配置
    void setEndpoint(const std::string& host, int port);
    void setApiKey(const std::string& key);
    void setModel(const std::string& model);
    void setTimeout(int seconds);
    void setLanguages(const std::string& source, const std::string& target);

private:
    TranslationService();
    ~TranslationService() = default;
    TranslationService(const TranslationService&) = delete;
    TranslationService& operator=(const TranslationService&) = delete;

    // 构建请求
    std::string buildRequestBody(const std::string& text);
    
    // 发送HTTP请求
    std::string sendHttpRequest(const std::string& body);
    
    // 解析响应
    TranslationResult parseResponse(const std::string& response, const std::string& originalText);

    std::string api_host_{"http://127.0.0.1"};
    int api_port_{8110};
    std::string api_key_{""};
    std::string model_{"HY-MT1.5-1.8B-Q8_0"};
    int timeout_{180};  // 默认3分钟
    std::string source_language_{"auto"};
    std::string target_language_{"zh"};
};

} // namespace DesktopTranslate

#endif // TRANSLATION_SERVICE_H
