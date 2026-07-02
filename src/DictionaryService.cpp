#include "DictionaryService.h"
#include "Config.h"
#include <string>
#include <QByteArray>
#include <QString>
#include <curl/curl.h>
#include <cctype>
#include <sstream>
#include <iostream>
#include <string_view>
#include <vector>
#include <algorithm>
#include <QDebug>
#include <nlohmann/json.hpp>

namespace DesktopTranslate {

namespace {

// CURL 写回调函数
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t totalSize = size * nmemb;
    userp->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

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

std::string getLanguageName(const std::string& code) {
    if (code == "auto") return "auto-detected";
    if (code == "zh") return "Chinese";
    if (code == "en") return "English";
    if (code == "ja") return "Japanese";
    if (code == "ko") return "Korean";
    if (code == "fr") return "French";
    if (code == "de") return "German";
    if (code == "es") return "Spanish";
    if (code == "ru") return "Russian";
    if (code == "bo") return "Tibetan";
    return code;
}

std::string buildSystemPrompt() {
    std::ostringstream prompt;
    prompt << "You are a professional multilingual dictionary engine.\n"
           << "For the queried word or phrase, provide dictionary information "
           << "in the EXACT format below.\n\n"
           << "Format:\n"
           << "[language] The detected language of the queried word. "
           << "Output a 2-letter ISO code: zh for Chinese, en for English, ja for Japanese, "
           << "ko for Korean, fr for French, de for German, es for Spanish, ru for Russian, bo for Tibetan.\n"
           << "[translation] The direct translation of the word into Chinese (e.g. 你好). "
           << "If the queried word is already Chinese, provide its English translation instead.\n"
           << "[pronunciation] Pinyin with tone marks of the Chinese translation (e.g. nǐ hǎo). "
           << "If the queried word is Chinese, provide IPA of the original word instead.\n"
           << "[part_of_speech] Part of speech (noun, verb, adjective, etc.).\n"
           << "[definitions] Bilingual definitions: first in the original language, then in Chinese. "
           << "Each definition on its own line.\n"
           << "[examples] Bilingual example sentences: original language first, then Chinese translation. "
           << "Each example on its own line.\n"
           << "[notes] Bilingual notes: etymology, usage notes, or cultural context. "
           << "Original language first, then Chinese.\n\n"
           << "Rules:\n"
           << "1. Each section must start with the marker in square brackets on its own line.\n"
           << "2. Output ONLY the dictionary entry, no reasoning, no commentary.\n"
           << "3. If a section is not applicable, output the marker followed by a dash.\n"
           << "4. Definitions and examples should be thorough and accurate.\n"
           << "5. The [language] section MUST come first and contain ONLY the 2-letter code.\n";
    return prompt.str();
}

std::string buildUserPrompt(const std::string& word, const std::string& language) {
    std::ostringstream prompt;
    prompt << "Query: \"" << word << "\"\n"
           << "Language: " << getLanguageName(language) << "\n"
           << "Please provide the dictionary entry for this word/phrase.";
    return prompt.str();
}

} // namespace

std::string DictionaryService::extractField(const std::string& text, const std::string& marker) {
    // 查找 [marker] 标记
    std::string searchMarker = "[" + marker + "]";
    size_t startPos = text.find(searchMarker);
    if (startPos == std::string::npos) {
        // 尝试中文全角方括号
        searchMarker = "\xe3\x80\x90" + marker + "\xe3\x80\x91"; // 【marker】
        startPos = text.find(searchMarker);
        if (startPos == std::string::npos) {
            return "";
        }
    }

    size_t contentStart = startPos + searchMarker.size();
    // 查找下一个标记或文本结尾
    size_t nextMarker = std::string::npos;

    // 搜索所有可能的下一个标记
    const std::vector<std::string> allMarkers = {
        "[language]", "[translation]", "[pronunciation]", "[part_of_speech]", "[definitions]", "[examples]", "[notes]",
        "\xe3\x80\x90" "language" "\xe3\x80\x91",
        "\xe3\x80\x90" "translation" "\xe3\x80\x91",
        "\xe3\x80\x90" "pronunciation" "\xe3\x80\x91",
        "\xe3\x80\x90" "part_of_speech" "\xe3\x80\x91",
        "\xe3\x80\x90" "definitions" "\xe3\x80\x91",
        "\xe3\x80\x90" "examples" "\xe3\x80\x91",
        "\xe3\x80\x90" "notes" "\xe3\x80\x91",
        "\xe3\x80\x90\xe8\xaf\xad\xe8\xa8\x80\xe3\x80\x91", // 【语言】
        "\xe3\x80\x90\xe7\xbf\xbb\xe8\xaf\x91\xe3\x80\x91", // 【翻译】
        "\xe3\x80\x90\xe5\x8f\x91\xe9\x9f\xb3\xe3\x80\x91", // 【发音】
        "\xe3\x80\x90\xe8\xaf\x8d\xe6\x80\xa7\xe3\x80\x91", // 【词性】
        "\xe3\x80\x90\xe9\x87\x8a\xe4\xb9\x89\xe3\x80\x91", // 【释义】
        "\xe3\x80\x90\xe4\xbe\x8b\xe5\x8f\xa5\xe3\x80\x91", // 【例句】
        "\xe3\x80\x90\xe5\xa4\x87\xe6\xb3\xa8\xe3\x80\x91", // 【备注】
    };

    for (const auto& m : allMarkers) {
        if (m == searchMarker) {
            continue;
        }
        size_t pos = text.find(m, contentStart);
        if (pos != std::string::npos && (nextMarker == std::string::npos || pos < nextMarker)) {
            nextMarker = pos;
        }
    }

    std::string content;
    if (nextMarker != std::string::npos) {
        content = text.substr(contentStart, nextMarker - contentStart);
    } else {
        content = text.substr(contentStart);
    }

    // 清理内容：去掉前导换行和空格
    content = trimWhitespace(content);

    // 去掉开头的冒号（有些模型会在标记后加冒号）
    if (!content.empty() && content[0] == ':') {
        content = trimWhitespace(content.substr(1));
    }

    return content;
}

DictionaryService& DictionaryService::instance() {
    static DictionaryService service;
    return service;
}

DictionaryService::DictionaryService() {
    auto& config = Config::instance();
    api_host_ = config.getApiEndpoint();
    api_port_ = config.getApiPort();
    api_key_ = config.getApiKey();
    model_ = config.getModel();
    timeout_ = config.getApiTimeout();

    qDebug() << "DictionaryService initialized:";
    qDebug() << "  API Host:" << QString::fromStdString(api_host_);
    qDebug() << "  API Port:" << api_port_;
    qDebug() << "  Model:" << QString::fromStdString(model_);

    curl_global_init(CURL_GLOBAL_ALL);
}

std::string DictionaryService::buildRequestBody(const std::string& word, const std::string& language) {
    using json = nlohmann::json;

    const std::string systemPrompt = buildSystemPrompt();
    const std::string userPrompt = buildUserPrompt(word, language);

    json requestBody = {
        {"model", model_},
        {"messages", json::array({
            {{"role", "system"}, {"content", systemPrompt}},
            {{"role", "user"}, {"content", userPrompt}}
        })},
        {"stream", false},
        {"temperature", 0.3}
    };

    qDebug() << "=== Dictionary request ===";
    qDebug() << "Word:" << QString::fromStdString(word);
    qDebug() << "Language:" << QString::fromStdString(language);

    return requestBody.dump();
}

std::string DictionaryService::sendHttpRequest(const std::string& body) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response;
    long httpCode = 0;

    std::string url = api_host_ + ":" + std::to_string(api_port_) + "/v1/chat/completions";

    qDebug() << "Sending dictionary request to:" << QString::fromStdString(url);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    struct curl_slist* headers = nullptr;
    std::string contentType = "Content-Type: application/json; charset=utf-8";
    headers = curl_slist_append(headers, contentType.c_str());
    headers = curl_slist_append(headers, "Accept: application/json");

    if (!api_key_.empty()) {
        std::string authHeader = "Authorization: Bearer " + api_key_;
        headers = curl_slist_append(headers, authHeader.c_str());
        std::string apiKeyHeader = "api-key: " + api_key_;
        headers = curl_slist_append(headers, apiKeyHeader.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

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

DictionaryResult DictionaryService::parseResponse(const std::string& response, const std::string& word, const std::string& language) {
    using json = nlohmann::json;

    DictionaryResult result;
    result.word = word;
    result.language = language;

    qDebug() << "=== Parsing dictionary API response ===";
    qDebug() << "Response length:" << response.size() << "bytes";

    try {
        json responseJson = json::parse(response);

        if (responseJson.contains("error")) {
            result.success = false;
            result.error_message = responseJson["error"]["message"].get<std::string>();
            qDebug() << "API returned error:" << QString::fromStdString(result.error_message);
            return result;
        }

        if (responseJson.contains("choices") && !responseJson["choices"].empty()) {
            auto& message = responseJson["choices"][0]["message"];
            auto content = message.contains("content") ? message["content"] : nlohmann::json{};

            std::string rawText = removeThinkBlocks(extractContentText(content));
            result.raw_response = rawText;

            // 提取LLM检测到的语言，覆盖用户选择的语言标签
            std::string detectedLang = extractField(rawText, "language");
            // 也尝试中文标记
            if (detectedLang.empty()) {
                detectedLang = extractField(rawText, "\xe8\xaf\xad\xe8\xa8\x80"); // 语言
            }
            if (!detectedLang.empty()) {
                // 取前2个字符作为语言代码
                if (detectedLang.size() >= 2) {
                    detectedLang = detectedLang.substr(0, 2);
                }
                // 转小写
                std::transform(detectedLang.begin(), detectedLang.end(), detectedLang.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                result.language = detectedLang;
            }

            // 提取翻译结果
            result.translation = extractField(rawText, "translation");
            if (result.translation.empty()) {
                result.translation = extractField(rawText, "\xe7\xbf\xbb\xe8\xaf\x91"); // 翻译
            }

            // 尝试从结构化标记中提取字段
            result.phonetic = extractField(rawText, "pronunciation");
            if (result.phonetic.empty()) {
                result.phonetic = extractField(rawText, "\xe5\x8f\x91\xe9\x9f\xb3"); // 发音
            }

            result.part_of_speech = extractField(rawText, "part_of_speech");
            if (result.part_of_speech.empty()) {
                result.part_of_speech = extractField(rawText, "\xe8\xaf\x8d\xe6\x80\xa7"); // 词性
            }

            result.definitions = extractField(rawText, "definitions");
            if (result.definitions.empty()) {
                result.definitions = extractField(rawText, "\xe9\x87\x8a\xe4\xb9\x89"); // 释义
            }

            result.examples = extractField(rawText, "examples");
            if (result.examples.empty()) {
                result.examples = extractField(rawText, "\xe4\xbe\x8b\xe5\x8f\xa5"); // 例句
            }

            result.notes = extractField(rawText, "notes");
            if (result.notes.empty()) {
                result.notes = extractField(rawText, "\xe5\xa4\x87\xe6\xb3\xa8"); // 备注
            }

            // 去除占位符dash
            if (result.translation == "-" || result.translation == "—") result.translation.clear();
            if (result.phonetic == "-" || result.phonetic == "—") result.phonetic.clear();
            if (result.part_of_speech == "-" || result.part_of_speech == "—") result.part_of_speech.clear();
            if (result.definitions == "-" || result.definitions == "—") result.definitions.clear();
            if (result.examples == "-" || result.examples == "—") result.examples.clear();
            if (result.notes == "-" || result.notes == "—") result.notes.clear();

            result.success = !rawText.empty();
            if (!result.success) {
                result.error_message = "Empty dictionary content in response";
            }

            qDebug() << "Dictionary parse result:";
            qDebug() << "  Phonetic:" << QString::fromStdString(result.phonetic);
            qDebug() << "  Part of speech:" << QString::fromStdString(result.part_of_speech);
            qDebug() << "  Definitions length:" << result.definitions.size();
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

DictionaryResult DictionaryService::lookUp(const std::string& word, const std::string& language) {
    DictionaryResult result;
    result.word = word;
    result.language = language;

    try {
        if (word.empty()) {
            result.success = false;
            result.error_message = "查询内容为空";
            return result;
        }

        std::string requestBody = buildRequestBody(word, language);
        std::string response = sendHttpRequest(requestBody);
        result = parseResponse(response, word, language);
    } catch (const std::exception& e) {
        result.success = false;
        std::string errMsg = e.what();

        // 检测常见的连接失败错误，提供更友好的提示
        if (errMsg.find("Server returned nothing") != std::string::npos
            || errMsg.find("couldn't connect to server") != std::string::npos
            || errMsg.find("Connection refused") != std::string::npos) {
            result.error_message = "模型服务未运行或已崩溃，请等待服务自动重启或检查模型配置\n"
                                   "(详细错误: " + errMsg + ")";
        } else if (errMsg.find("Timeout was reached") != std::string::npos) {
            result.error_message = "查询超时，模型服务可能正在加载或忙碌，请稍后重试\n"
                                   "(详细错误: " + errMsg + ")";
        } else {
            result.error_message = errMsg;
        }
    }

    return result;
}

std::future<DictionaryResult> DictionaryService::lookUpAsync(const std::string& word, const std::string& language) {
    return std::async(std::launch::async, [this, word, language]() {
        return lookUp(word, language);
    });
}

void DictionaryService::lookUpWithCallback(const std::string& word, const std::string& language, DictionaryCallback callback) {
    std::thread([this, word, language, callback]() {
        auto result = lookUp(word, language);
        if (callback) {
            callback(result);
        }
    }).detach();
}

void DictionaryService::setEndpoint(const std::string& host, int port) {
    api_host_ = host;
    api_port_ = port;
}

void DictionaryService::setApiKey(const std::string& key) {
    api_key_ = key;
}

void DictionaryService::setModel(const std::string& model) {
    model_ = model;
}

void DictionaryService::setTimeout(int seconds) {
    timeout_ = seconds;
}

} // namespace DesktopTranslate
