#include "filelogger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>

namespace {
QMutex loggerMutex;
QString logDirectory;
QtMessageHandler previousHandler = nullptr;

QString pathForToday()
{
    return QDir(logDirectory).filePath(
        QStringLiteral("TeaMonitorFusion_%1.log").arg(QDate::currentDate().toString(QStringLiteral("yyyyMMdd"))));
}

void appendLine(const QString &level, const QString &message)
{
    if (logDirectory.isEmpty()) {
        return;
    }
    QFile file(pathForToday());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
           << " [" << level << "] " << message << '\n';
}

void qtMessageToFile(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    QString level = QStringLiteral("DEBUG");
    if (type == QtInfoMsg) {
        level = QStringLiteral("INFO");
    } else if (type == QtWarningMsg) {
        level = QStringLiteral("WARN");
    } else if (type == QtCriticalMsg || type == QtFatalMsg) {
        level = QStringLiteral("ERROR");
    }
    {
        QMutexLocker locker(&loggerMutex);
        appendLine(level, message);
    }
    if (previousHandler) {
        previousHandler(type, {}, message);
    }
}
}

void FileLogger::initialize()
{
    QMutexLocker locker(&loggerMutex);
    logDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                       .filePath(QStringLiteral("logs"));
    QDir().mkpath(logDirectory);
    const QDateTime retentionBoundary = QDateTime::currentDateTime().addDays(-14);
    const QFileInfoList oldLogs = QDir(logDirectory).entryInfoList(
        {QStringLiteral("TeaMonitorFusion_*.log")}, QDir::Files, QDir::Time);
    for (const QFileInfo &fileInfo : oldLogs) {
        if (fileInfo.lastModified() < retentionBoundary) {
            QFile::remove(fileInfo.absoluteFilePath());
        }
    }
    appendLine(QStringLiteral("INFO"), QStringLiteral("应用启动"));
    previousHandler = qInstallMessageHandler(qtMessageToFile);
}

void FileLogger::shutdown()
{
    QMutexLocker locker(&loggerMutex);
    appendLine(QStringLiteral("INFO"), QStringLiteral("应用退出"));
    qInstallMessageHandler(previousHandler);
    previousHandler = nullptr;
}

void FileLogger::write(const QString &message, const QString &level)
{
    QMutexLocker locker(&loggerMutex);
    appendLine(level, message);
}

QString FileLogger::currentLogPath()
{
    QMutexLocker locker(&loggerMutex);
    return pathForToday();
}
