#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

class AuditStore
{
public:
    static bool initialize(const QString &databasePath = QString(), QString *errorMessage = nullptr);
    static void shutdown();
    static bool recordExport(const QString &sampleId,
                             const QString &batchId,
                             const QString &operatorName,
                             const QString &deviceId,
                             const QString &deviceName,
                             const QString &dataType,
                             const QString &capturedAt,
                             const QString &filePath,
                             const QJsonObject &parameters,
                             bool calibrated,
                             QString *errorMessage = nullptr);
    static QString databasePath();
    static QVector<QStringList> recentExports(int limit = 200, QString *errorMessage = nullptr);
};
