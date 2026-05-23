#include "OCRService.h"
#include "Config.h"
#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QDebug>
#include <QScreen>
#include <QGuiApplication>
#include <QPixmap>
#include <QPainter>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace DesktopTranslate {

namespace {

int clamp8(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return value;
}

QImage preprocessForOcr(const QImage& input) {
    if (input.isNull()) {
        return {};
    }

    QImage flattened(input.size(), QImage::Format_RGB32);
    flattened.fill(Qt::white);
    {
        QPainter painter(&flattened);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawImage(0, 0, input);
    }

    const int maxDim = std::max(flattened.width(), flattened.height());
    int targetMaxDim = maxDim;
    if (maxDim < 1400) {
        targetMaxDim = 1400;
    } else if (maxDim > 2200) {
        targetMaxDim = 2200;
    }

    QImage scaled = flattened;
    if (targetMaxDim != maxDim && maxDim > 0) {
        const double ratio = static_cast<double>(targetMaxDim) / static_cast<double>(maxDim);
        const int newW = std::max(1, static_cast<int>(std::lround(flattened.width() * ratio)));
        const int newH = std::max(1, static_cast<int>(std::lround(flattened.height() * ratio)));
        scaled = flattened.scaled(newW, newH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    QImage contrast = scaled.convertToFormat(QImage::Format_RGB32);
    constexpr double contrastFactor = 1.18;
    for (int y = 0; y < contrast.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(contrast.scanLine(y));
        for (int x = 0; x < contrast.width(); ++x) {
            const int r = qRed(line[x]);
            const int g = qGreen(line[x]);
            const int b = qBlue(line[x]);
            const int nr = clamp8(static_cast<int>(std::lround((r - 128) * contrastFactor + 128)));
            const int ng = clamp8(static_cast<int>(std::lround((g - 128) * contrastFactor + 128)));
            const int nb = clamp8(static_cast<int>(std::lround((b - 128) * contrastFactor + 128)));
            line[x] = qRgb(nr, ng, nb);
        }
    }

    return contrast;
}

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

    const QImage preparedImage = preprocessForOcr(image);
    qDebug() << "OCR preprocess: original =" << image.width() << "x" << image.height()
             << "prepared =" << preparedImage.width() << "x" << preparedImage.height();

    // 将图片转为base64
    QString base64Image = imageToBase64(preparedImage.isNull() ? image : preparedImage);
    
    // 构建请求体 - OpenAI Vision API格式
    nlohmann::json requestBody = {
        {"model", model_},
        {"messages", {
            {
                {"role", "user"},
                {"content", {
                    {
                        {"type", "text"},
                        {"text", "You are an OCR engine. Extract every visible text element from the image, including titles, labels, units, and small annotations. Keep the original language and casing. Output plain text only (no markdown, no code fences, no explanations). Use natural reading order (top-to-bottom, left-to-right). Put each distinct text block on its own line. Preserve meaningful line breaks and list items. If there is no readable text, output exactly: No text found"}
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
        {"max_tokens", 4096},
        {"temperature", 0}
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

QImage OCRService::captureScreenArea(int x, int y, int width, int height) {
    qDebug() << "Input screen area:" << x << y << width << height;

    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return {};
    }

    QRect screenGeometry = screen->geometry();
    qDebug() << "Screen geometry:" << screenGeometry;
    qDebug() << "Device pixel ratio:" << screen->devicePixelRatio();

    QPixmap pixmap = screen->grabWindow(0, x, y, width, height);
    if (pixmap.isNull()) {
        return {};
    }

    QImage image = pixmap.toImage();
    qDebug() << "Captured image size:" << image.width() << "x" << image.height();
    return image;
}

OCRResult OCRService::recognizeScreenArea(int x, int y, int width, int height) {
    OCRResult result;
    QImage image = captureScreenArea(x, y, width, height);
    if (image.isNull()) {
        result.error = "Failed to capture screen area";
        return result;
    }

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
