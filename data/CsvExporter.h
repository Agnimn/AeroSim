#pragma once

#include "../core/Scenario.h"
#include "../core/SimulationState.h"
#include "../core/CommandHistoryEntry.h"
#include "AirbaseRegistry.h"

#include <QHash>
#include <QString>
#include <QVector>

class CsvExporter final
{
public:
    struct ExportSummary
    {
        QString absolutePath;
        int rowCount = 0;
        qint64 fileSizeBytes = 0;
    };

    static bool writeFile(const Scenario &scenario,
                          const QVector<AirbaseRecord> &airbases,
                          const QHash<QString, AircraftSimulationState> &aircraftStates,
                          const QString &filePath,
                          ExportSummary *summary,
                          QString *errorMessage = nullptr);
    static bool writeFile(const Scenario &scenario,
                          const QVector<AirbaseRecord> &airbases,
                          const QHash<QString, AircraftSimulationState> &aircraftStates,
                          const QVector<CommandHistoryEntry> &commandHistory,
                          const QString &filePath,
                          ExportSummary *summary,
                          QString *errorMessage = nullptr);
};
