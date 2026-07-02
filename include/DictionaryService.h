#ifndef DICTIONARY_SERVICE_H
#define DICTIONARY_SERVICE_H

#include <string>
#include <functional>
#include <future>
#include <memory>

namespace DesktopTranslate {

/**
 * @brief 字典查询结果结构
 */
struct DictionaryResult {
    bool success{false};
    std::string word;
    std::string language;
    std::string translation;    // 直接翻译结果（如：你好）
    std::string phonetic;       // 翻译结果的拼音（如：nǐ hǎo）
    std::string part_of_speech; // 词性
    std::string definitions;    // 释义（双语）
    std::string examples;       // 例句（双语）
    std::string notes;          // 备注（双语）
    std::string raw_response;   // 原始LLM响应（兜底显示）
    std::string error_message;
};

/**
 * @brief 字典服务类 - 调用本地大模型API进行多语言字典查询
 *
 * 复用翻译服务的本地LLM端点，通过精心设计的Prompt
 * 让模型扮演多语言字典，输出带拼音/国际音标、词性、释义、例句的结构化结果。
 */
class DictionaryService {
public:
    using DictionaryCallback = std::function<void(const DictionaryResult&)>;

    static DictionaryService& instance();

    // 同步查询
    DictionaryResult lookUp(const std::string& word, const std::string& language);

    // 异步查询
    std::future<DictionaryResult> lookUpAsync(const std::string& word, const std::string& language);

    // 回调方式查询
    void lookUpWithCallback(const std::string& word, const std::string& language, DictionaryCallback callback);

    // 设置API配置
    void setEndpoint(const std::string& host, int port);
    void setApiKey(const std::string& key);
    void setModel(const std::string& model);
    void setTimeout(int seconds);

private:
    DictionaryService();
    ~DictionaryService() = default;
    DictionaryService(const DictionaryService&) = delete;
    DictionaryService& operator=(const DictionaryService&) = delete;

    // 构建请求体
    std::string buildRequestBody(const std::string& word, const std::string& language);

    // 发送HTTP请求
    std::string sendHttpRequest(const std::string& body);

    // 解析响应
    DictionaryResult parseResponse(const std::string& response, const std::string& word, const std::string& language);

    // 从LLM响应中按【标记】提取字段
    static std::string extractField(const std::string& text, const std::string& marker);

    std::string api_host_{"http://127.0.0.1"};
    int api_port_{8110};
    std::string api_key_{""};
    std::string model_{"HY-MT1.5-1.8B-Q8_0"};
    int timeout_{180};
};

} // namespace DesktopTranslate

#endif // DICTIONARY_SERVICE_H
