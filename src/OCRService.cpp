#include "OCRService.h"
#include "Config.h"
#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QDebug>
#include <QScreen>
#include <QGuiApplication>
#include <QPixmap>
#include <QStringList>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace DesktopTranslate {

namespace {

QString removeThinkBlocks(QString text) {
    const QString openTag = "<think>";
    const QString closeTag = "</think>";
    int openPos = text.indexOf(openTag);

    while (openPos >= 0) {
        const int closePos = text.indexOf(closeTag, openPos + openTag.size());
        if (closePos < 0) {
            text.remove(openPos, text.size() - openPos);
            break;
        }

        text.remove(openPos, closePos + closeTag.size() - openPos);
        openPos = text.indexOf(openTag);
    }

    return text.trimmed();
}

QString extractMessageText(const nlohmann::json& content) {
    if (content.is_string()) {
        return QString::fromStdString(content.get<std::string>());
    }

    if (content.is_null()) {
        return {};
    }

    if (content.is_array()) {
        QString combined;
        for (const auto& item : content) {
            if (item.is_string()) {
                combined += QString::fromStdString(item.get<std::string>());
                continue;
            }

            if (!item.is_object()) {
                continue;
            }

            if (item.contains("text") && item["text"].is_string()) {
                combined += QString::fromStdString(item["text"].get<std::string>());
                continue;
            }

            if (item.contains("content") && item["content"].is_string()) {
                combined += QString::fromStdString(item["content"].get<std::string>());
            }
        }
        return combined;
    }

    if (content.is_object()) {
        if (content.contains("text") && content["text"].is_string()) {
            return QString::fromStdString(content["text"].get<std::string>());
        }

        if (content.contains("content") && content["content"].is_string()) {
            return QString::fromStdString(content["content"].get<std::string>());
        }
    }

    return QString::fromStdString(content.dump());
}

QString normalizeOcrText(const QString& rawText) {
    QString normalized = removeThinkBlocks(rawText);
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');

    const QStringList lines = normalized.split('\n');
    QStringList normalizedLines;
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty()) {
            if (!normalizedLines.isEmpty() && !normalizedLines.back().isEmpty()) {
                normalizedLines.append("");
            }
            continue;
        }

        normalizedLines.append(line);
    }

    return normalizedLines.join("\n").trimmed();
}

bool isNoTextMarker(const QString& text) {
    const QString normalized = text.trimmed().toLower();
    return normalized == "no text found"
        || normalized == "no text"
        || normalized == "未发现文本"
        || normalized == "未检测到文本";
}

}

OCRService& OCRService::instance() {
    static OCRService instance;
    return instance;
}

OCRService::OCRService() {
    auto& config = Config::instance();
    api_host_ = config.getOcrEndpoint();
    api_port_ = config.getOcrPort();
    api_key_ = config.getOcrApiKey();
    model_ = config.getOcrModel();
}

void OCRService::setEndpoint(const std::string& host, int port) {
    api_host_ = host;
    api_port_ = port;
}

void OCRService::setApiKey(const std::string& key) {
    api_key_ = key;
}

void OCRService::setModel(const std::string& model) {
    model_ = model;
}

QString OCRService::imageToBase64(const QImage& image) {
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return QString::fromLatin1(byteArray.toBase64());
}

// CURL回调函数
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

OCRResult OCRService::recognizeText(const QImage& image) {
    OCRResult result;
    
    if (image.isNull()) {
        result.error = "Invalid image";
        return result;
    }
    
    // 将图片转为base64
    QString base64Image = imageToBase64(image);
    
    // 构建请求体 - OpenAI Vision API格式
    nlohmann::json requestBody = {
        {"model", model_},
        {"messages", {
            {
                {"role", "user"},
                {"content", {
                    {
                        {"type", "text"},
                        {"text", "Extract all visible text from this image in natural reading order. Output plain text only. Preserve meaningful paragraph breaks and list items. Do not describe the image, do not add explanations, and do not wrap the result in markdown or code fences. If there is no readable text, output 'No text found'."}
                    },
                    {
                        {"type", "image_url"},
                        {"image_url", {
                            {"url", "data:image/png;base64," + base64Image.toStdString()}
                        }}
                    }
                }}
            }
        }},
        {"max_tokens", 4096}
    };
    
    std::string body = requestBody.dump();
    
    // 发送HTTP请求
    CURL* curl = curl_easy_init();
    if (!curl) {
        result.error = "Failed to initialize CURL";
        return result;
    }
    
    std::string response;
    std::string url = api_host_ + ":" + std::to_string(api_port_) + "/v1/chat/completions";
    long httpCode = 0;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // 设置请求头
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    
    if (!api_key_.empty()) {
        std::string authHeader = "Authorization: Bearer " + api_key_;
        headers = curl_slist_append(headers, authHeader.c_str());
        std::string apiKeyHeader = "api-key: " + api_key_;
        headers = curl_slist_append(headers, apiKeyHeader.c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    qDebug() << "Sending OCR request to:" << QString::fromStdString(url);
    qDebug() << "Image size:" << image.width() << "x" << image.height();
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        result.error = QString("CURL error: ") + curl_easy_strerror(res);
        qWarning() << "OCR request failed:" << result.error;
        return result;
    }

    if (httpCode < 200 || httpCode >= 300) {
        result.error = QString("HTTP error %1: %2").arg(httpCode).arg(QString::fromStdString(response));
        qWarning() << "OCR request failed:" << result.error;
        return result;
    }
    
    // 解析响应
    try {
        auto responseJson = nlohmann::json::parse(response);
        
        if (responseJson.contains("error")) {
            result.error = QString::fromStdString(responseJson["error"]["message"].get<std::string>());
            qWarning() << "OCR API error:" << result.error;
            return result;
        }
        
        if (responseJson.contains("choices") && !responseJson["choices"].empty()) {
            const auto& message = responseJson["choices"][0]["message"];
            const auto content = message.contains("content") ? message["content"] : nlohmann::json{};
            result.text = normalizeOcrText(extractMessageText(content));

            if (result.text.isEmpty() && message.contains("reasoning_content") && message["reasoning_content"].is_string()) {
                result.text = normalizeOcrText(QString::fromStdString(message["reasoning_content"].get<std::string>()));
            }

            if (isNoTextMarker(result.text)) {
                result.text.clear();
            }

            result.success = !result.text.isEmpty();
            if (!result.success) {
                result.error = "No text found in response";
            }
        } else {
            result.error = "No text found in response";
        }
    } catch (const std::exception& e) {
        result.error = QString("JSON parse error: ") + e.what();
        qWarning() << "Failed to parse OCR response:" << result.error;
    }
    
    return result;
}

OCRResult OCRService::recognizeText(const QString& imagePath) {
    QImage image(imagePath);
    return recognizeText(image);
}

OCRResult OCRService::recognizeScreenArea(int x, int y, int width, int height) {
    OCRResult result;
    
    qDebug() << "Input screen area:" << x << y << width << height;
    
    // 截取屏幕区域
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        result.error = "No screen available";
        return result;
    }
    
    // 获取屏幕几何信息
    QRect screenGeometry = screen->geometry();
    qDebug() << "Screen geometry:" << screenGeometry;
    qDebug() << "Device pixel ratio:" << screen->devicePixelRatio();
    
    // grabWindow 使用的是物理像素坐标
    // 在高DPI屏幕上，需要乘以 devicePixelRatio
    // 但是 grabWindow 的坐标原点在虚拟屏幕的左上角
    // 直接使用逻辑坐标进行截图，不缩放
    // Qt 会自动处理坐标转换
    QPixmap pixmap = screen->grabWindow(0, x, y, width, height);
    
    if (pixmap.isNull()) {
        result.error = "Failed to capture screen area";
        return result;
    }
    
    QImage image = pixmap.toImage();
    qDebug() << "Captured image size:" << image.width() << "x" << image.height();
    
    // 保存截图到结果中
    result.screenshot = image;
    
    auto ocrResult = recognizeText(image);
    result.success = ocrResult.success;
    result.text = ocrResult.text;
    if (!ocrResult.success) {
        result.error = ocrResult.error;
    }
    
    return result;
}

} // namespace DesktopTranslate
