#include "ModelServiceManager.h"
#include "Config.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>
#include <QDebug>

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace DesktopTranslate {

namespace {

QString firstExistingPath(const QStringList& candidates) {
    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return QDir::cleanPath(candidate);
        }
    }

    return {};
}

QString firstModelFile(const QString& directoryPath, const QStringList& filters, const QString& excludedToken = {}) {
    QDir directory(directoryPath);
    const QFileInfoList files = directory.entryInfoList(filters, QDir::Files, QDir::Name);
    for (const QFileInfo& file : files) {
        if (!excludedToken.isEmpty() && file.fileName().contains(excludedToken, Qt::CaseInsensitive)) {
            continue;
        }
        return file.absoluteFilePath();
    }

    return {};
}

QString trimLogLine(QString line) {
    line.replace('\r', "");
    return line.trimmed();
}

void configureProcessWindowBehavior(QProcess* process) {
#if defined(Q_OS_WIN)
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* arguments) {
        arguments->flags |= CREATE_NO_WINDOW;
        arguments->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
        arguments->startupInfo->wShowWindow = SW_HIDE;
    });
#else
    Q_UNUSED(process)
#endif
}

bool llamaServerHasOffloadDevice(const QString& executablePath) {
    QProcess probe;
    configureProcessWindowBehavior(&probe);
    probe.setProgram(executablePath);
    probe.setArguments({"--list-devices"});
    probe.setProcessChannelMode(QProcess::MergedChannels);
    probe.start();

    if (!probe.waitForStarted(3000)) {
        qWarning().noquote() << QStringLiteral("Failed to probe llama.cpp devices: %1").arg(probe.errorString());
        return false;
    }

    if (!probe.waitForFinished(5000)) {
        probe.kill();
        probe.waitForFinished(1000);
        qWarning() << "Timed out while probing llama.cpp devices";
        return false;
    }

    const QString output = QString::fromLocal8Bit(probe.readAllStandardOutput());
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (QString line : lines) {
        line = trimLogLine(line);
        if (line.isEmpty() || line == "Available devices:") {
            continue;
        }
        return true;
    }

    return false;
}

}

ModelServiceManager::ModelServiceManager(QObject* parent)
    : QObject(parent)
{
}

ModelServiceManager::~ModelServiceManager() {
    stopAll();
}

void ModelServiceManager::startAsync() {
    if (started_) {
        return;
    }

    started_ = true;
    QTimer::singleShot(0, this, &ModelServiceManager::startAll);
}

void ModelServiceManager::stopAll() {
    stopProcess(translation_process_);
    stopProcess(ocr_process_);
}

void ModelServiceManager::startAll() {
    startService(translationServiceSpec());
    startService(ocrServiceSpec());
}

void ModelServiceManager::startService(const ServiceSpec& spec) {
    const QString executablePath = llamaServerPath();
    if (executablePath.isEmpty()) {
        qWarning() << "llama-server executable not found under third_party/llama.cpp/build";
        return;
    }

    if (spec.modelPath.isEmpty()) {
        qWarning() << "Model file not found for service:" << spec.name;
        return;
    }

    if (spec.kind == ServiceKind::OCR && spec.mmprojPath.isEmpty()) {
        qWarning() << "Multimodal projector file not found for service:" << spec.name;
        return;
    }

    QProcess* process = ensureProcess(spec.kind, spec.name);
    if (!process || process->state() != QProcess::NotRunning) {
        return;
    }

    const bool useGpuOffload = llamaServerHasOffloadDevice(executablePath);
    QStringList arguments = {
        "--host", "127.0.0.1",
        "--port", QString::number(spec.port),
        "--model", spec.modelPath,
        "--alias", spec.alias,
        "--ctx-size", QString::number(spec.contextSize)
    };

    if (useGpuOffload) {
        arguments << "--n-gpu-layers" << "all" << "--flash-attn" << "on";
    }

    if (!spec.mmprojPath.isEmpty()) {
        arguments << "--mmproj" << spec.mmprojPath;
    }

    if (!spec.chatTemplatePath.isEmpty()) {
        arguments << "--jinja" << "--chat-template-file" << spec.chatTemplatePath;
    }

    process->setProgram(executablePath);
    process->setArguments(arguments);
    process->setWorkingDirectory(QFileInfo(executablePath).absolutePath());
    process->start();

    qInfo().noquote() << QStringLiteral("Starting %1 model service on port %2 with model %3")
                             .arg(spec.name)
                             .arg(spec.port)
                             .arg(QDir::toNativeSeparators(spec.modelPath));
    qInfo().noquote() << QStringLiteral("%1 model service backend: %2")
                             .arg(spec.name, useGpuOffload ? QStringLiteral("GPU") : QStringLiteral("CPU"));
}

QProcess* ModelServiceManager::ensureProcess(ServiceKind kind, const QString& name) {
    QProcess*& process = kind == ServiceKind::Translation ? translation_process_ : ocr_process_;
    if (process) {
        return process;
    }

    process = new QProcess(this);
    configureProcessWindowBehavior(process);
    process->setProcessChannelMode(QProcess::MergedChannels);

    connect(process, &QProcess::started, this, [name]() {
        qInfo().noquote() << QStringLiteral("%1 model service started").arg(name);
    });

    connect(process, &QProcess::readyRead, this, [process, name]() {
        const QString output = QString::fromLocal8Bit(process->readAll());
        const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        for (QString line : lines) {
            line = trimLogLine(line);
            if (!line.isEmpty()) {
                qInfo().noquote() << QStringLiteral("[%1] %2").arg(name, line);
            }
        }
    });

    connect(process, &QProcess::errorOccurred, this, [process, name](QProcess::ProcessError error) {
        Q_UNUSED(error)
        qWarning().noquote() << QStringLiteral("%1 model service failed: %2").arg(name, process->errorString());
    });

    connect(process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [name](int exitCode, QProcess::ExitStatus exitStatus) {
                qWarning().noquote() << QStringLiteral("%1 model service exited with code %2 status %3")
                                            .arg(name)
                                            .arg(exitCode)
                                            .arg(exitStatus == QProcess::NormalExit ? "normal" : "crash");
            });

    return process;
}

void ModelServiceManager::stopProcess(QProcess* process) {
    if (!process || process->state() == QProcess::NotRunning) {
        return;
    }

    process->terminate();
    if (!process->waitForFinished(3000)) {
        process->kill();
        process->waitForFinished(1000);
    }
}

QString ModelServiceManager::projectRootPath() const {
#ifdef DESKTOP_TRANSLATE_PROJECT_ROOT
    return QDir(QStringLiteral(DESKTOP_TRANSLATE_PROJECT_ROOT)).absolutePath();
#else
    return QDir(QCoreApplication::applicationDirPath()).absolutePath();
#endif
}

QString ModelServiceManager::llamaServerPath() const {
    const QString executableName =
#if defined(Q_OS_WIN)
        QStringLiteral("llama-server.exe");
#else
        QStringLiteral("llama-server");
#endif

    const QString thirdPartyRoot = QDir(projectRootPath()).filePath("third_party/llama.cpp");
    const QString buildRoot = QDir(projectRootPath()).filePath("build");

    return firstExistingPath({
        QDir(buildRoot).filePath("bin/" + executableName),
        QDir(buildRoot).filePath("bin/Release/" + executableName),
        QDir(buildRoot).filePath("bin/Debug/" + executableName),
        QDir(buildRoot).filePath("bin/RelWithDebInfo/" + executableName),
        QDir(buildRoot).filePath("bin/MinSizeRel/" + executableName),
        QDir(thirdPartyRoot).filePath("build/bin/" + executableName),
        QDir(thirdPartyRoot).filePath("build/bin/Release/" + executableName),
        QDir(thirdPartyRoot).filePath("build/bin/Debug/" + executableName),
        QDir(thirdPartyRoot).filePath("build/bin/RelWithDebInfo/" + executableName),
        QDir(thirdPartyRoot).filePath("build/bin/MinSizeRel/" + executableName)
    });
}

QString ModelServiceManager::translationModelPath() const {
    return firstModelFile(QDir(projectRootPath()).filePath("models/HY-MT1.5-1.8B-GGUF"), {"*.gguf"});
}

QString ModelServiceManager::ocrModelPath() const {
    return firstModelFile(QDir(projectRootPath()).filePath("models/PaddleOCR-VL-1.5-GGUF"), {"*.gguf"}, "mmproj");
}

QString ModelServiceManager::ocrProjectorPath() const {
    return firstModelFile(QDir(projectRootPath()).filePath("models/PaddleOCR-VL-1.5-GGUF"), {"*mmproj*.gguf"});
}

QString ModelServiceManager::ocrChatTemplatePath() const {
    return firstExistingPath({
        QDir(projectRootPath()).filePath("models/PaddleOCR-VL-1.5-GGUF/chat_template_llama.jinja")
    });
}

ModelServiceManager::ServiceSpec ModelServiceManager::translationServiceSpec() const {
    const auto& config = Config::instance();
    return {
        ServiceKind::Translation,
        QStringLiteral("Translation"),
        QString::fromStdString(config.getModel()),
        translationModelPath(),
        {},
        {},
        config.getApiPort(),
        4096
    };
}

ModelServiceManager::ServiceSpec ModelServiceManager::ocrServiceSpec() const {
    const auto& config = Config::instance();
    return {
        ServiceKind::OCR,
        QStringLiteral("OCR"),
        QString::fromStdString(config.getOcrModel()),
        ocrModelPath(),
        ocrProjectorPath(),
        ocrChatTemplatePath(),
        config.getOcrPort(),
        8192
    };
}

} // namespace DesktopTranslate
