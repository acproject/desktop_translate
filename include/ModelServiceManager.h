#ifndef MODELSERVICEMANAGER_H
#define MODELSERVICEMANAGER_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QTimer>

class QProcess;

namespace DesktopTranslate {

class ModelServiceManager : public QObject {
    Q_OBJECT

public:
    static ModelServiceManager& instance();
    ~ModelServiceManager() override;

    void startAsync();
    void stopAll();

    // 切换翻译模型（高级/标准）
    void switchTranslationModel(bool advanced);

private:
    ModelServiceManager(QObject* parent = nullptr);
    ModelServiceManager(const ModelServiceManager&) = delete;
    ModelServiceManager& operator=(const ModelServiceManager&) = delete;
    enum class ServiceKind {
        Translation,
        OCR,
    };

    struct ServiceSpec {
        ServiceKind kind;
        QString name;
        QString alias;
        QString modelPath;
        QString mmprojPath;
        QString chatTemplatePath;
        int port{0};
        int contextSize{4096};
    };

    void startAll();
    void startService(const ServiceSpec& spec);
    void restartService(const ServiceSpec& spec);
    QProcess* ensureProcess(ServiceKind kind, const QString& name);
    void stopProcess(QProcess* process);
    QString projectRootPath() const;
    QString llamaServerPath() const;
    QString translationModelPath() const;
    QString advancedTranslationModelPath() const;
    QString ocrModelPath() const;
    QString ocrProjectorPath() const;
    QString ocrChatTemplatePath() const;
    ServiceSpec translationServiceSpec() const;
    ServiceSpec ocrServiceSpec() const;

    QProcess* translation_process_{nullptr};
    QProcess* ocr_process_{nullptr};
    bool started_{false};

    // 崩溃自动重启
    QHash<ServiceKind, ServiceSpec> stored_specs_;
    QHash<ServiceKind, int> restart_counts_;
    QHash<ServiceKind, QTimer*> restart_timers_;
    static constexpr int kMaxRestartAttempts = 5;
    static constexpr int kRestartDelayMs = 3000;
};

} // namespace DesktopTranslate

#endif // MODELSERVICEMANAGER_H
