#include "TranslationService.h"
#include "Config.h"
#include <curl/curl.h>
#include <cctype>
#include <sstream>
#include <iostream>
#include <vector>
#include <QDebug>
#include <nlohmann/json.hpp>

namespace DesktopTranslate {

namespace {

std::string trimWhitespace(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(start, end - start);
}

std::string normalizeLineEndings(const std::string& value) {
    std::string normalized;
    normalized.reserve(value.size());

    for (size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '\r') {
            normalized.push_back('\n');
            if (index + 1 < value.size() && value[index + 1] == '\n') {
                ++index;
            }
            continue;
        }

        normalized.push_back(value[index]);
    }

    return normalized;
}

bool isBulletLine(const std::string& line) {
    if (line.empty()) {
        return false;
    }

    const unsigned char first = static_cast<unsigned char>(line.front());
    if (first == '-' || first == '*' || line.rfind("\xE2\x80\xA2", 0) == 0) {
        return true;
    }

    size_t index = 0;
    while (index < line.size() && std::isdigit(static_cast<unsigned char>(line[index]))) {
        ++index;
    }

    return index > 0
        && index < line.size()
        && (line[index] == '.' || line[index] == ')');
}

bool shouldMergeLines(const std::string& previousLine, const std::string& currentLine) {
    if (previousLine.empty() || currentLine.empty()) {
        return false;
    }

    if (isBulletLine(previousLine) || isBulletLine(currentLine)) {
        return false;
    }

    const char lastChar = previousLine.back();
    if (lastChar == ':' || previousLine.ends_with("\xEF\xBC\x9A")) {
        return false;
    }

    return lastChar != '.'
        && lastChar != '!'
        && lastChar != '?'
        && lastChar != ';';
}

std::string preprocessTextForTranslation(const std::string& text) {
    std::istringstream stream(normalizeLineEndings(text));
    std::string line;
    std::vector<std::string> normalizedLines;

    while (std::getline(stream, line)) {
        std::string trimmed = trimWhitespace(line);
        if (trimmed.empty()) {
            if (!normalizedLines.empty() && !normalizedLines.back().empty()) {
                normalizedLines.push_back("");
            }
            continue;
        }

        if (normalizedLines.empty() || normalizedLines.back().empty()) {
            normalizedLines.push_back(trimmed);
            continue;
        }

        std::string& previousLine = normalizedLines.back();
        if (previousLine.back() == '-'
            && std::isalnum(static_cast<unsigned char>(trimmed.front()))) {
            previousLine.pop_back();
            previousLine += trimmed;
            continue;
        }

        if (shouldMergeLines(previousLine, trimmed)) {
            previousLine += " " + trimmed;
            continue;
        }

        normalizedLines.push_back(trimmed);
    }

    std::ostringstream normalizedText;
    for (size_t index = 0; index < normalizedLines.size(); ++index) {
        if (index > 0) {
            normalizedText << '\n';
        }
        normalizedText << normalizedLines[index];
    }

    return trimWhitespace(normalizedText.str());
}

std::string removeThinkBlocks(const std::string& value) {
    std::string cleaned = value;
    const std::string openTag = "<think>";
    const std::string closeTag = "</think>";
    size_t openPos = cleaned.find(openTag);

    while (openPos != std::string::npos) {
        size_t closePos = cleaned.find(closeTag, openPos + openTag.size());
        if (closePos == std::string::npos) {
            cleaned.erase(openPos);
            break;
        }
        cleaned.erase(openPos, closePos + closeTag.size() - openPos);
        openPos = cleaned.find(openTag);
    }

    return trimWhitespace(cleaned);
}

std::string extractContentText(const nlohmann::json& content) {
    using json = nlohmann::json;

    if (content.is_string()) {
        return content.get<std::string>();
    }

    if (content.is_null()) {
        return "";
    }

    if (content.is_array()) {
        std::string combined;
        for (const auto& item : content) {
            if (item.is_string()) {
                combined += item.get<std::string>();
                continue;
            }

            if (!item.is_object()) {
                continue;
            }

            if (item.contains("text") && item["text"].is_string()) {
                combined += item["text"].get<std::string>();
                continue;
            }

            if (item.contains("content") && item["content"].is_string()) {
                combined += item["content"].get<std::string>();
            }
        }
        return combined;
    }

    if (content.is_object()) {
        if (content.contains("text") && content["text"].is_string()) {
            return content["text"].get<std::string>();
        }

        if (content.contains("content") && content["content"].is_string()) {
            return content["content"].get<std::string>();
        }
    }

    return content.dump();
}

std::string extractTranslationFromReasoning(const std::string& reasoning) {
    std::string cleaned = removeThinkBlocks(reasoning);
    if (cleaned.empty()) {
        return "";
    }

    const std::vector<std::string> markers = {
        "**Final Translation:**",
        "Final Translation:",
        "最终翻译：",
        "最终翻译:",
        "译文：",
        "译文:"
    };

    for (const auto& marker : markers) {
        size_t pos = cleaned.rfind(marker);
        if (pos != std::string::npos) {
            return trimWhitespace(cleaned.substr(pos + marker.size()));
        }
    }

    std::istringstream stream(cleaned);
    std::string line;
    std::string lastNonEmptyLine;
    while (std::getline(stream, line)) {
        std::string trimmed = trimWhitespace(line);
        if (!trimmed.empty()) {
            lastNonEmptyLine = trimmed;
        }
    }

    return lastNonEmptyLine.empty() ? cleaned : lastNonEmptyLine;
}

std::string buildSystemPrompt(const std::string& sourceLanguage, const std::string& targetLanguage) {
    std::ostringstream prompt;
    prompt << "You are a professional translation engine. "
           << "Return only the final translated text.\n"
           << "Requirements:\n"
           << "1. Preserve meaning, tone, meaningful paragraph structure, lists, placeholders, markdown, code, numbers, and proper nouns.\n"
           << "2. Do not omit, summarize, explain, answer questions, or add commentary.\n"
           << "3. Ignore obvious OCR hard wraps inside the same paragraph, but keep real paragraph breaks and list structure.\n"
           << "4. Do not output reasoning, analysis, or any XML/HTML style tags.\n";

    if (sourceLanguage == "auto") {
        prompt << "5. Automatically detect the source language and translate into " << targetLanguage << ".";
    } else {
        prompt << "5. Translate from " << sourceLanguage << " into " << targetLanguage << ".";
    }

    return prompt.str();
}

std::string buildUserPrompt(const std::string& text, const std::string& sourceLanguage, const std::string& targetLanguage) {
    std::ostringstream prompt;
    prompt << "Translate the following text";
    if (sourceLanguage != "auto") {
        prompt << " from " << sourceLanguage;
    }
    prompt << " to " << targetLanguage << ".\n"
           << "Output the translation only.\n"
           << "<text>\n"
           << text << "\n"
           << "</text>";
    return prompt.str();
}

}

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

    const std::string systemPrompt = buildSystemPrompt(source_language_, target_language_);
    const std::string userPrompt = buildUserPrompt(text, source_language_, target_language_);

    json requestBody = {
        {"model", model_},
        {"messages", json::array({
            {{"role", "system"}, {"content", systemPrompt}},
            {{"role", "user"}, {"content", userPrompt}}
        })},
        {"stream", false}
    };

    qDebug() << "=== Translation request ===";
    qDebug() << "Original text length:" << text.size();
    qDebug() << "System prompt:" << QString::fromStdString(systemPrompt);
    qDebug() << "User prompt preview:" << QString::fromStdString(userPrompt.substr(0, 300));

    return requestBody.dump();
}

std::string TranslationService::sendHttpRequest(const std::string& body) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    
    std::string response;
    long httpCode = 0;
    
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
    headers = curl_slist_append(headers, "Accept: application/json");
    
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
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    
    // 清理
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("CURL error: ") + curl_easy_strerror(res));
    }

    if (httpCode < 200 || httpCode >= 300) {
        throw std::runtime_error("HTTP error " + std::to_string(httpCode) + ": " + response);
    }
    
    return response;
}

TranslationResult TranslationService::parseResponse(const std::string& response, const std::string& originalText) {
    using json = nlohmann::json;
    
    TranslationResult result;
    result.original_text = originalText;
    
    qDebug() << "=== Parsing API response ===";
    qDebug() << "Response length:" << response.size() << "bytes";
    qDebug() << "Response preview:" << QString::fromStdString(response.substr(0, 500));
    
    try {
        json responseJson = json::parse(response);
        
        // 检查错误
        if (responseJson.contains("error")) {
            result.success = false;
            result.error_message = responseJson["error"]["message"].get<std::string>();
            qDebug() << "API returned error:" << QString::fromStdString(result.error_message);
            return result;
        }
        
        // 提取翻译结果
        if (responseJson.contains("choices") && !responseJson["choices"].empty()) {
            auto& message = responseJson["choices"][0]["message"];
            auto content = message.contains("content") ? message["content"] : nlohmann::json{};
            qDebug() << "Content type:" << content.type_name();

            result.translated_text = trimWhitespace(removeThinkBlocks(extractContentText(content)));

            if (result.translated_text.empty() && message.contains("reasoning_content")) {
                auto reasoningContent = message["reasoning_content"];
                if (reasoningContent.is_string()) {
                    result.translated_text = extractTranslationFromReasoning(reasoningContent.get<std::string>());
                    qDebug() << "Extracted from reasoning_content:" << QString::fromStdString(result.translated_text.substr(0, 200));
                }
            }
            
            qDebug() << "Translated text length:" << result.translated_text.length();
            qDebug() << "Translated text preview:" << QString::fromStdString(result.translated_text.substr(0, 200));

            result.success = !result.translated_text.empty();
            if (!result.success) {
                result.error_message = "Empty translation content in response";
            }
        } else {
            result.success = false;
            result.error_message = "Invalid response format: no choices";
            qDebug() << "No choices in response";
        }
    } catch (const json::exception& e) {
        result.success = false;
        result.error_message = std::string("JSON parse error: ") + e.what();
        qDebug() << "JSON parse error:" << e.what();
    }
    
    return result;
}

TranslationResult TranslationService::translate(const std::string& text) {
    TranslationResult result;
    result.original_text = text;
    
    try {
        const std::string normalizedText = preprocessTextForTranslation(text);
        const std::string& requestText = normalizedText.empty() ? text : normalizedText;

        if (requestText != text) {
            qDebug() << "Normalized translation input preview:" << QString::fromStdString(requestText.substr(0, 300));
        }

        std::string requestBody = buildRequestBody(requestText);
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
    if (model_.find("nllb") != std::string::npos) {
        if (target == "zh") {
            target_language_ = "zho_Hans";
        } else {
            target_language_ = target;
        }
    } else {
        target_language_ = target;
    }
}

} // namespace DesktopTranslate
