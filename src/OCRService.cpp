#include "OCRService.h"
#include "Config.h"
#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QDebug>
#include <QScreen>
#include <QGuiApplication>
#include <QPixmap>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace DesktopTranslate {

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
                        {"text", "Please extract all text from this image. Only output the extracted text, no explanations. If there is no text, output 'No text found'."}
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
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // 设置请求头
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
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
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        result.error = QString("CURL error: ") + curl_easy_strerror(res);
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
            result.text = QString::fromStdString(
                responseJson["choices"][0]["message"]["content"].get<std::string>()
            );
            result.success = true;
            result.text = result.text.trimmed();
            
            // 移除可能的 "No text found" 标记
            if (result.text == "No text found") {
                result.text.clear();
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
    
    qDebug() << "Capturing screen area:" << x << y << width << height;
    
    // 截取屏幕区域
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        result.error = "No screen available";
        return result;
    }
    
    // 考虑设备像素比（高DPI屏幕）
    qreal devicePixelRatio = screen->devicePixelRatio();
    int scaledX = static_cast<int>(x * devicePixelRatio);
    int scaledY = static_cast<int>(y * devicePixelRatio);
    int scaledWidth = static_cast<int>(width * devicePixelRatio);
    int scaledHeight = static_cast<int>(height * devicePixelRatio);
    
    qDebug() << "Device pixel ratio:" << devicePixelRatio;
    qDebug() << "Scaled area:" << scaledX << scaledY << scaledWidth << scaledHeight;
    
    QPixmap pixmap = screen->grabWindow(0, scaledX, scaledY, scaledWidth, scaledHeight);
    
    if (pixmap.isNull()) {
        result.error = "Failed to capture screen area";
        return result;
    }
    
    QImage image = pixmap.toImage();
    qDebug() << "Captured image size:" << image.width() << "x" << image.height();
    
    return recognizeText(image);
}

} // namespace DesktopTranslate
