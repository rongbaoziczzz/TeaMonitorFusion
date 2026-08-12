#include "auditstore.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace {
constexpr auto connectionName = "tea_monitor_audit";
QString activeDatabasePath;
}

bool AuditStore::initialize(const QString &databasePath, QString *errorMessage)
{
    if (QSqlDatabase::contains(QString::fromLatin1(connectionName))) {
        shutdown();
    }

    activeDatabasePath = databasePath;
    if (activeDatabasePath.isEmpty()) {
        const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        QDir().mkpath(dataDirectory);
        activeDatabasePath = QDir(dataDirectory).filePath(QStringLiteral("detection_records.sqlite"));
    } else {
        QDir().mkpath(QFileInfo(activeDatabasePath).absolutePath());
    }

    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QString::fromLatin1(connectionName));
    database.setDatabaseName(activeDatabasePath);
    if (!database.open()) {
        if (errorMessage) {
            *errorMessage = database.lastError().text();
        }
        return false;
    }

    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS detection_records ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "sample_id TEXT NOT NULL, batch_id TEXT, operator_name TEXT,"
            "device_id TEXT NOT NULL, device_name TEXT NOT NULL, data_type TEXT NOT NULL,"
            "captured_at TEXT NOT NULL, exported_at TEXT NOT NULL, file_path TEXT NOT NULL,"
            "parameters_json TEXT NOT NULL, calibrated INTEGER NOT NULL DEFAULT 0)"))) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_detection_sample ON detection_records(sample_id)"));
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_detection_batch ON detection_records(batch_id)"));
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_detection_time ON detection_records(captured_at)"));
    query.exec(QStringLiteral("PRAGMA user_version = 1"));
    return true;
}

void AuditStore::shutdown()
{
    if (!QSqlDatabase::contains(QString::fromLatin1(connectionName))) {
        return;
    }
    {
        QSqlDatabase database = QSqlDatabase::database(QString::fromLatin1(connectionName), false);
        database.close();
    }
    QSqlDatabase::removeDatabase(QString::fromLatin1(connectionName));
}

bool AuditStore::recordExport(const QString &sampleId,
                              const QString &batchId,
                              const QString &operatorName,
                              const QString &deviceId,
                              const QString &deviceName,
                              const QString &dataType,
                              const QString &capturedAt,
                              const QString &filePath,
                              const QJsonObject &parameters,
                              bool calibrated,
                              QString *errorMessage)
{
    QSqlDatabase database = QSqlDatabase::database(QString::fromLatin1(connectionName), false);
    if (!database.isValid() || !database.isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("审计数据库未打开。");
        }
        return false;
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO detection_records (sample_id,batch_id,operator_name,device_id,device_name,"
        "data_type,captured_at,exported_at,file_path,parameters_json,calibrated) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?)"));
    query.addBindValue(sampleId);
    query.addBindValue(batchId);
    query.addBindValue(operatorName);
    query.addBindValue(deviceId);
    query.addBindValue(deviceName);
    query.addBindValue(dataType);
    query.addBindValue(capturedAt);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    query.addBindValue(filePath);
    query.addBindValue(QString::fromUtf8(QJsonDocument(parameters).toJson(QJsonDocument::Compact)));
    query.addBindValue(calibrated ? 1 : 0);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}

QString AuditStore::databasePath()
{
    return activeDatabasePath;
}

QVector<QStringList> AuditStore::recentExports(int limit, QString *errorMessage)
{
    QVector<QStringList> rows;
    QSqlDatabase database = QSqlDatabase::database(QString::fromLatin1(connectionName), false);
    if (!database.isValid() || !database.isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("审计数据库未打开。");
        }
        return rows;
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT sample_id,batch_id,operator_name,device_name,data_type,captured_at,"
        "exported_at,file_path,calibrated FROM detection_records ORDER BY id DESC LIMIT ?"));
    query.addBindValue(qBound(1, limit, 1000));
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return rows;
    }
    while (query.next()) {
        QStringList row;
        for (int column = 0; column < 8; ++column) {
            row.append(query.value(column).toString());
        }
        row.append(query.value(8).toBool() ? QStringLiteral("是") : QStringLiteral("否"));
        rows.append(row);
    }
    return rows;
}
