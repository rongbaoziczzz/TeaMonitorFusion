#pragma once

#include <QString>

class FileLogger
{
public:
    static void initialize();
    static void shutdown();
    static void write(const QString &message, const QString &level = QStringLiteral("INFO"));
    static QString currentLogPath();
};
