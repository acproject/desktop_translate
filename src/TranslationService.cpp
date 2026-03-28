#include "TranslationService.h"
#include "Config.h"
#include <curl/curl.h>
#include <sstream>
#include <iostream>
#include <QDebug>
#include <nlohmann/json.hpp>

namespace DesktopTranslate {

// CURL 写回调函数
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t totalSize = size * nmemb;
    userp->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

TranslationService& TranslationService::instance() {
    static TranslationService service;
    return service;
}

TranslationService::TranslationService() {
    // 从配置加载
    auto& config = Config::instance();
    api_host_ = config.getApiEndpoint();
    api_port_ = config.getApiPort();
    api_key_ = config.getApiKey();
    model_ = config.getModel();
    timeout_ = config.getApiTimeout();
    source_language_ = config.getSourceLanguage();
    target_language_ = config.getTargetLanguage();
    
    qDebug() << "TranslationService initialized:";
    qDebug() << "  API Host:" << QString::fromStdString(api_host_);
    qDebug() << "  API Port:" << api_port_;
    qDebug() << "  Model:" << QString::fromStdString(model_);
    qDebug() << "  Timeout:" << timeout_ << "seconds";
    
    // 初始化CURL
    curl_global_init(CURL_GLOBAL_ALL);
}

std::string TranslationService::buildRequestBody(const std::string& text) {
    using json = nlohmann::json;
    
    // 构建系统提示词
    std::string systemPrompt;
    if (source_language_ == "auto") {
        systemPrompt = "You are a professional translator. Translate the following text to " + 
                       target_language_ + 
                       ". Only output the translation result, no explanations or notes.";
    } else {
        systemPrompt = "You are a professional translator. Translate the following text from " + 
                       source_language_ + " to " + target_language_ + 
                       ". Only output the translation result, no explanations or notes.";
    }
    
    json requestBody = {
        {"model", model_},
        {"messages", json::array({
            {{"role", "system"}, {"content", systemPrompt}},
            {{"role", "user"}, {"content", text}}
        })},
        {"temperature", 0.3},
        {"max_tokens", 4096}
    };
    
    return requestBody.dump();
}

std::string TranslationService::sendHttpRequest(const std::string& body) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    
    std::string response;
    
    // 构建URL
    std::string url = api_host_ + ":" + std::to_string(api_port_) + "/v1/chat/completions";
    
    qDebug() << "Sending request to:" << QString::fromStdString(url);
    qDebug() << "Request body size:" << body.size() << "bytes";
    qDebug() << "Timeout setting:" << timeout_ << "seconds";
    
    // 设置CURL选项
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // 设置请求头
    struct curl_slist* headers = nullptr;
    std::string contentType = "Content-Type: application/json";
    headers = curl_slist_append(headers, contentType.c_str());
    
    // 如果API Key不为空，添加两种认证头
    if (!api_key_.empty()) {
        std::string authHeader = "Authorization: Bearer " + api_key_;
        headers = curl_slist_append(headers, authHeader.c_str());
        
        // 添加 api-key 头 (某些服务如Azure OpenAI需要)
        std::string apiKeyHeader = "api-key: " + api_key_;
        headers = curl_slist_append(headers, apiKeyHeader.c_str());
        
        qDebug() << "API Key set, adding Authorization and api-key headers";
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // 执行请求
    CURLcode res = curl_easy_perform(curl);
    
    // 清理
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("CURL error: ") + curl_easy_strerror(res));
    }
    
    return response;
}

TranslationResult TranslationService::parseResponse(const std::string& response, const std::string& originalText) {
    using json = nlohmann::json;
    
    TranslationResult result;
    result.original_text = originalText;
    
    try {
        json responseJson = json::parse(response);
        
        // 检查错误
        if (responseJson.contains("error")) {
            result.success = false;
            result.error_message = responseJson["error"]["message"].get<std::string>();
            return result;
        }
        
        // 提取翻译结果
        if (responseJson.contains("choices") && !responseJson["choices"].empty()) {
            result.translated_text = responseJson["choices"][0]["message"]["content"].get<std::string>();
            result.success = true;
        } else {
            result.success = false;
            result.error_message = "Invalid response format";
        }
    } catch (const json::exception& e) {
        result.success = false;
        result.error_message = std::string("JSON parse error: ") + e.what();
    }
    
    return result;
}

TranslationResult TranslationService::translate(const std::string& text) {
    TranslationResult result;
    result.original_text = text;
    
    try {
        std::string requestBody = buildRequestBody(text);
        std::string response = sendHttpRequest(requestBody);
        result = parseResponse(response, text);
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
    }
    
    return result;
}

std::future<TranslationResult> TranslationService::translateAsync(const std::string& text) {
    return std::async(std::launch::async, [this, text]() {
        return translate(text);
    });
}

void TranslationService::translateWithCallback(const std::string& text, TranslationCallback callback) {
    std::thread([this, text, callback]() {
        auto result = translate(text);
        if (callback) {
            callback(result);
        }
    }).detach();
}

void TranslationService::setEndpoint(const std::string& host, int port) {
    api_host_ = host;
    api_port_ = port;
}

void TranslationService::setApiKey(const std::string& key) {
    api_key_ = key;
}

void TranslationService::setModel(const std::string& model) {
    model_ = model;
}

void TranslationService::setTimeout(int seconds) {
    timeout_ = seconds;
}

void TranslationService::setLanguages(const std::string& source, const std::string& target) {
    source_language_ = source;
    target_language_ = target;
}

} // namespace DesktopTranslate
