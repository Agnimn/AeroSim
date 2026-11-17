#pragma once

#include "../core/Scenario.h"
#include "../core/CommandHistoryEntry.h"
#include "AirbaseRegistry.h"

#include <QString>
#include <QVector>

class GeoJsonExporter final
{
public:
    struct ExportSummary
    {
        QString absolutePath;
        int featureCount = 0;
        qint64 fileSizeBytes = 0;
    };

    // Entity and airbase point features are emitted with any supported routes.
    static QByteArray toJson(const Scenario &scenario, QString *errorMessage = nullptr);
    static QByteArray toJson(const Scenario &scenario,
                             const QVector<AirbaseRecord> &airbases,
                             QString *errorMessage = nullptr,
                             int *featureCount = nullptr);
    static QByteArray toJson(const Scenario &scenario,
                             const QVector<AirbaseRecord> &airbases,
                             const QVector<CommandHistoryEntry> &commandHistory,
                             QString *errorMessage = nullptr,
                             int *featureCount = nullptr);

    // Writes, commits, then reopens and validates the final file.
    static bool writeFile(const Scenario &scenario,
                          const QVector<AirbaseRecord> &airbases,
                          const QString &filePath,
                          ExportSummary *summary,
                          QString *errorMessage = nullptr);
    static bool writeFile(const Scenario &scenario,
                          const QVector<AirbaseRecord> &airbases,
                          const QVector<CommandHistoryEntry> &commandHistory,
                          const QString &filePath,
                          ExportSummary *summary,
                          QString *errorMessage = nullptr);
};
