#ifndef OCRSERVICE_H
#define OCRSERVICE_H

#include <QObject>
#include <QString>
#include <QImage>
#include <memory>

namespace DesktopTranslate {

/**
 * @brief OCR识别结果
 */
struct OCRResult {
    bool success{false};
    QString text;
    QString error;
    QImage screenshot;  // 截图图像
};

/**
 * @brief OCR识别服务 - 支持多种OCR后端
 */
class OCRService : public QObject {
    Q_OBJECT

public:
    static OCRService& instance();

    // 设置API端点（使用OpenAI兼容的Vision API）
    void setEndpoint(const std::string& host, int port);
    void setApiKey(const std::string& key);
    void setModel(const std::string& model);
    
    // 识别图片中的文字
    OCRResult recognizeText(const QImage& image);
    OCRResult recognizeText(const QString& imagePath);
    
    // 识别屏幕区域
    OCRResult recognizeScreenArea(int x, int y, int width, int height);

private:
    OCRService();
    ~OCRService() = default;
    OCRService(const OCRService&) = delete;
    OCRService& operator=(const OCRService&) = delete;
    
    std::string api_host_{"http://127.0.0.1"};
    int api_port_{8111};
    std::string api_key_;
    std::string model_{"PaddleOCR-VL-1.5-GGUF"};
    
    QString imageToBase64(const QImage& image);
};

} // namespace DesktopTranslate

#endif // OCRSERVICE_H
