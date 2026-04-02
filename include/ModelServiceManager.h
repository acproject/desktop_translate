#ifndef MODELSERVICEMANAGER_H
#define MODELSERVICEMANAGER_H

#include <QObject>
#include <QString>

class QProcess;

namespace DesktopTranslate {

class ModelServiceManager : public QObject {
    Q_OBJECT

public:
    explicit ModelServiceManager(QObject* parent = nullptr);
    ~ModelServiceManager() override;

    void startAsync();
    void stopAll();

private:
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
    QProcess* ensureProcess(ServiceKind kind, const QString& name);
    void stopProcess(QProcess* process);
    QString projectRootPath() const;
    QString llamaServerPath() const;
    QString translationModelPath() const;
    QString ocrModelPath() const;
    QString ocrProjectorPath() const;
    QString ocrChatTemplatePath() const;
    ServiceSpec translationServiceSpec() const;
    ServiceSpec ocrServiceSpec() const;

    QProcess* translation_process_{nullptr};
    QProcess* ocr_process_{nullptr};
    bool started_{false};
};

} // namespace DesktopTranslate

#endif // MODELSERVICEMANAGER_H
