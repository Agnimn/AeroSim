#include "CsvExporter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
void setError(QString *errorMessage, const QString &message)
{
    if (!message.isEmpty())
        qWarning().noquote() << "[CSV EXPORT] Error:" << message;
    if (errorMessage)
        *errorMessage = message;
}

bool normalizedExportPath(const QString &inputPath, QString *absolutePath,
                          QString *errorMessage)
{
    if (inputPath.trimmed().isEmpty()) {
        setError(errorMessage, QStringLiteral("Invalid path: export path is empty."));
        return false;
    }

    QString path = inputPath;
    if (inputPath.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)) {
        const QUrl fileUrl(inputPath);
        if (!fileUrl.isValid() || !fileUrl.isLocalFile()) {
            setError(errorMessage,
                     QStringLiteral("Invalid path: expected a local file URL."));
            return false;
        }
        path = fileUrl.toLocalFile();
    }

    if (path.trimmed().isEmpty()) {
        setError(errorMessage, QStringLiteral("Invalid path: local file path is empty."));
        return false;
    }

    QFileInfo selectedInfo(path);
    if (selectedInfo.fileName().isEmpty()) {
        setError(errorMessage, QStringLiteral("Invalid path: a filename is required."));
        return false;
    }

    if (selectedInfo.suffix().compare(QStringLiteral("csv"),
                                      Qt::CaseInsensitive) != 0) {
        path = selectedInfo.dir().filePath(
            selectedInfo.completeBaseName() + QStringLiteral(".csv"));
        selectedInfo.setFile(path);
    }

    const QString resolvedPath = QDir::cleanPath(selectedInfo.absoluteFilePath());
    const QFileInfo directoryInfo(QFileInfo(resolvedPath).absolutePath());
    if (!directoryInfo.exists() || !directoryInfo.isDir()) {
        setError(errorMessage,
                 QStringLiteral("Directory unavailable: %1")
                     .arg(directoryInfo.absoluteFilePath()));
        return false;
    }

    *absolutePath = resolvedPath;
    qInfo().noquote() << "[CSV EXPORT] Normalized absolute path:" << resolvedPath;
    return true;
}

QString csvField(const QString &value)
{
    if (!value.contains(QLatin1Char(',')) &&
        !value.contains(QLatin1Char('"')) &&
        !value.contains(QLatin1Char('\r')) &&
        !value.contains(QLatin1Char('\n')))
        return value;

    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QString numberField(double value)
{
    return QString::number(value, 'g', std::numeric_limits<double>::max_digits10);
}

bool validCoordinate(double latitude, double longitude, double altitude)
{
    return std::isfinite(latitude) && std::isfinite(longitude) &&
           std::isfinite(altitude) && latitude >= -90.0 && latitude <= 90.0 &&
           longitude >= -180.0 && longitude <= 180.0;
}

bool parseCsv(const QByteArray &bytes, QVector<QStringList> *rows)
{
    const QString text = QString::fromUtf8(bytes);
    QStringList row;
    QString field;
    bool insideQuotes = false;
    bool justClosedQuote = false;
    rows->clear();

    for (qsizetype index = 0; index < text.size(); ++index) {
        const QChar character = text.at(index);
        if (insideQuotes) {
            if (character == QLatin1Char('"')) {
                if (index + 1 < text.size() &&
                    text.at(index + 1) == QLatin1Char('"')) {
                    field.append(QLatin1Char('"'));
                    ++index;
                } else {
                    insideQuotes = false;
                    justClosedQuote = true;
                }
            } else {
                field.append(character);
            }
            continue;
        }

        if (character == QLatin1Char('"')) {
            if (!field.isEmpty() || justClosedQuote)
                return false;
            insideQuotes = true;
        } else if (character == QLatin1Char(',')) {
            row.append(field);
            field.clear();
            justClosedQuote = false;
        } else if (character == QLatin1Char('\n') ||
                   character == QLatin1Char('\r')) {
            row.append(field);
            field.clear();
            justClosedQuote = false;
            rows->append(row);
            row.clear();
            if (character == QLatin1Char('\r') &&
                index + 1 < text.size() &&
                text.at(index + 1) == QLatin1Char('\n'))
                ++index;
        } else {
            if (justClosedQuote)
                return false;
            field.append(character);
        }
    }

    if (insideQuotes)
        return false;
    if (!field.isEmpty() || !row.isEmpty() || justClosedQuote) {
        row.append(field);
        rows->append(row);
    }
    return true;
}
}

bool CsvExporter::writeFile(
    const Scenario &scenario,
    const QVector<AirbaseRecord> &airbases,
    const QHash<QString, AircraftSimulationState> &aircraftStates,
    const QString &filePath,
    ExportSummary *summary,
    QString *errorMessage)
{
    return writeFile(scenario, airbases, aircraftStates, {}, filePath,
                     summary, errorMessage);
}

bool CsvExporter::writeFile(
    const Scenario &scenario,
    const QVector<AirbaseRecord> &airbases,
    const QHash<QString, AircraftSimulationState> &aircraftStates,
    const QVector<CommandHistoryEntry> &commandHistory,
    const QString &filePath,
    ExportSummary *summary,
    QString *errorMessage)
{
    int aircraftCount = 0;
    int routeCount = 0;
    for (const Entity &entity : scenario.entities()) {
        if (entity.type() != QStringLiteral("aircraft"))
            continue;
        ++aircraftCount;
        if (entity.route().size() >= 2)
            ++routeCount;
    }
    const int airbaseCount = static_cast<int>(std::count_if(
        airbases.cbegin(), airbases.cend(),
        [](const AirbaseRecord &airbase) { return airbase.coordinateAvailable; }));
    qInfo().noquote() << "[CSV EXPORT] writeFile requestedPath="
                      << filePath << "aircraft=" << aircraftCount
                      << "airbases=" << airbaseCount << "routes=" << routeCount
                      << "commands=" << commandHistory.size();

    QString absolutePath;
    if (!normalizedExportPath(filePath, &absolutePath, errorMessage))
        return false;

    QStringList rows;
    rows.reserve(static_cast<qsizetype>(scenario.entityCount()) + airbases.size() + 1);
    rows.append(QStringLiteral(
        "ID,Callsign,EntityType,Latitude,Longitude,Altitude,Heading,Speed,Status,FormationID,"
        "RecordType,CommandSequence,Prompt,Timestamp,ProcessingStatus,ParsedOperation"));

    for (const Entity &entity : scenario.entities()) {
        if (entity.type() != QStringLiteral("aircraft"))
            continue;

        const auto stateIt = aircraftStates.constFind(entity.id());
        if (stateIt == aircraftStates.cend()) {
            setError(errorMessage,
                     QStringLiteral("CSV export failure: aircraft %1 has no current simulation state.")
                         .arg(entity.id()));
            return false;
        }
        const AircraftSimulationState &state = stateIt.value();
        if (!validCoordinate(state.latitude, state.longitude, state.altitude) ||
            !std::isfinite(state.heading) || !std::isfinite(state.speed)) {
            setError(errorMessage,
                     QStringLiteral("CSV export failure: aircraft %1 has invalid simulation data.")
                         .arg(entity.id()));
            return false;
        }

        const QStringList fields = {
            entity.id(),
            entity.callsign(),
            entity.type(),
            numberField(state.latitude),
            numberField(state.longitude),
            numberField(state.altitude),
            numberField(state.heading),
            numberField(state.speed),
            state.status,
            entity.formationId(),
            QStringLiteral("Aircraft"),
            QString(),
            QString(),
            QString(),
            QString(),
            QString()
        };
        QStringList escapedFields;
        escapedFields.reserve(fields.size());
        for (const QString &field : fields)
            escapedFields.append(csvField(field));
        rows.append(escapedFields.join(QLatin1Char(',')));
    }

    for (const AirbaseRecord &airbase : airbases) {
        if (!airbase.coordinateAvailable)
            continue;
        const double latitude = airbase.coordinate.latitude();
        const double longitude = airbase.coordinate.longitude();
        if (!validCoordinate(latitude, longitude, 0.0)) {
            setError(errorMessage,
                     QStringLiteral("CSV export failure: airbase %1 has an invalid position.")
                         .arg(airbase.id));
            return false;
        }

        const QStringList fields = {
            airbase.id,
            QString(),
            QStringLiteral("airbase"),
            numberField(latitude),
            numberField(longitude),
            QString(),
            QString(),
            QString(),
            QString(),
            QString(),
            QStringLiteral("Airbase"),
            QString(),
            QString(),
            QString(),
            QString(),
            QString()
        };
        QStringList escapedFields;
        escapedFields.reserve(fields.size());
        for (const QString &field : fields)
            escapedFields.append(csvField(field));
        rows.append(escapedFields.join(QLatin1Char(',')));
    }

    for (const ScenarioRoute &route : scenario.routes()) {
        for (int index = 0; index < route.waypoints.size(); ++index) {
            const Waypoint &waypoint = route.waypoints.at(index);
            if (!validCoordinate(waypoint.latitude, waypoint.longitude,
                                 waypoint.altitude)) {
                setError(errorMessage,
                         QStringLiteral("CSV export failure: route %1 has invalid waypoint data.")
                             .arg(route.id));
                return false;
            }
            const QStringList fields = {
                route.id, route.name, QStringLiteral("route"),
                numberField(waypoint.latitude), numberField(waypoint.longitude),
                numberField(waypoint.altitude), QString(), QString(),
                route.status, route.aircraftId,
                index == 0 ? QStringLiteral("Route") : QStringLiteral("Waypoint"),
                QString(), waypoint.name, QString::number(index + 1),
                QString(), QString()};
            QStringList escapedFields;
            for (const QString &field : fields)
                escapedFields.append(csvField(field));
            rows.append(escapedFields.join(QLatin1Char(',')));
        }
    }

    for (const CommandHistoryEntry &command : commandHistory) {
        const QStringList fields = {
            QString(), QString(), QString(), QString(), QString(),
            QString(), QString(), QString(), QString(), QString(),
            QStringLiteral("Command"),
            QString::number(command.sequenceNumber),
            command.prompt,
            command.timestamp,
            command.processingStatus,
            command.parsedOperation
        };
        QStringList escapedFields;
        escapedFields.reserve(fields.size());
        for (const QString &field : fields)
            escapedFields.append(csvField(field));
        rows.append(escapedFields.join(QLatin1Char(',')));
    }

    const int rowCount = rows.size() - 1;
    const QByteArray csv = (rows.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8();
    qInfo().noquote() << "[CSV EXPORT] Serialized bytes=" << csv.size()
                      << "rows=" << rowCount;
    if (csv.isEmpty()) {
        setError(errorMessage, QStringLiteral("CSV serialization failure: output is empty."));
        return false;
    }
    if (rowCount == 0) {
        setError(errorMessage,
                 QStringLiteral("No exportable aircraft or airbases are available."));
        return false;
    }

    QSaveFile file(absolutePath);
    const bool opened = file.open(QIODevice::WriteOnly);
    qInfo().noquote() << "[CSV EXPORT] Open result=" << opened
                      << "path=" << absolutePath;
    if (!opened) {
        setError(errorMessage,
                 QStringLiteral("File write failure for %1: %2")
                     .arg(absolutePath, file.errorString()));
        return false;
    }

    const qint64 bytesWritten = file.write(csv);
    qInfo().noquote() << "[CSV EXPORT] Bytes written=" << bytesWritten
                      << "expected=" << csv.size();
    if (bytesWritten != csv.size()) {
        setError(errorMessage,
                 QStringLiteral("File write failure for %1: wrote %2 of %3 bytes (%4).")
                     .arg(absolutePath)
                     .arg(bytesWritten)
                     .arg(csv.size())
                     .arg(file.errorString()));
        file.cancelWriting();
        return false;
    }
    if (!file.flush()) {
        setError(errorMessage,
                 QStringLiteral("File write failure for %1: %2")
                     .arg(absolutePath, file.errorString()));
        file.cancelWriting();
        return false;
    }
    const bool committed = file.commit();
    qInfo().noquote() << "[CSV EXPORT] Commit result=" << committed
                      << "path=" << absolutePath
                      << "error=" << file.errorString();
    if (!committed) {
        setError(errorMessage,
                 QStringLiteral("File write failure committing %1: %2")
                     .arg(absolutePath, file.errorString()));
        return false;
    }

    const QFileInfo finalInfo(absolutePath);
    qInfo().noquote() << "[CSV EXPORT] Final file size="
                      << (finalInfo.exists() ? finalInfo.size() : -1)
                      << "path=" << absolutePath;
    QFile verifyFile(absolutePath);
    if (!finalInfo.exists() || !finalInfo.isFile() ||
        finalInfo.size() != csv.size() || !verifyFile.open(QIODevice::ReadOnly)) {
        setError(errorMessage,
                 QStringLiteral("File verification failure for %1: %2")
                     .arg(absolutePath, verifyFile.errorString()));
        return false;
    }
    const QByteArray savedCsv = verifyFile.readAll();
    QVector<QStringList> parsedRows;
    const bool csvParsed = parseCsv(savedCsv, &parsedRows);
    const QStringList expectedHeader = {
        QStringLiteral("ID"), QStringLiteral("Callsign"),
        QStringLiteral("EntityType"), QStringLiteral("Latitude"),
        QStringLiteral("Longitude"), QStringLiteral("Altitude"),
        QStringLiteral("Heading"), QStringLiteral("Speed"),
        QStringLiteral("Status"), QStringLiteral("FormationID"),
        QStringLiteral("RecordType"), QStringLiteral("CommandSequence"),
        QStringLiteral("Prompt"), QStringLiteral("Timestamp"),
        QStringLiteral("ProcessingStatus"), QStringLiteral("ParsedOperation")};
    int verifiedCommandCount = 0;
    int commandIndex = 0;
    bool commandHistoryMatches = true;
    for (auto row = parsedRows.cbegin() + (parsedRows.isEmpty() ? 0 : 1);
         row != parsedRows.cend(); ++row) {
        if (row->size() == expectedHeader.size() &&
            row->at(10) == QStringLiteral("Command")) {
            ++verifiedCommandCount;
            if (commandIndex >= commandHistory.size()) {
                commandHistoryMatches = false;
                continue;
            }
            const CommandHistoryEntry &expected = commandHistory.at(commandIndex++);
            commandHistoryMatches =
                commandHistoryMatches &&
                row->at(11) == QString::number(expected.sequenceNumber) &&
                row->at(12) == expected.prompt &&
                row->at(13) == expected.timestamp &&
                row->at(14) == expected.processingStatus &&
                row->at(15) == expected.parsedOperation;
        }
    }
    commandHistoryMatches = commandHistoryMatches &&
                            commandIndex == commandHistory.size();
    qInfo().noquote() << "[CSV EXPORT] Read-back validation bytes="
                      << savedCsv.size() << "parsed=" << csvParsed
                      << "rows=" << (parsedRows.isEmpty() ? 0 : parsedRows.size() - 1);
    if (savedCsv != csv || !csvParsed || parsedRows.isEmpty() ||
        parsedRows.first() != expectedHeader ||
        parsedRows.size() != rowCount + 1 ||
        verifiedCommandCount != commandHistory.size() ||
        !commandHistoryMatches ||
        std::any_of(parsedRows.cbegin() + 1, parsedRows.cend(),
                    [&expectedHeader](const QStringList &row)
                    { return row.size() != expectedHeader.size(); })) {
        setError(errorMessage,
                 QStringLiteral("File verification failure for %1: saved CSV is incomplete.")
                     .arg(absolutePath));
        return false;
    }

    if (summary) {
        summary->absolutePath = absolutePath;
        summary->rowCount = rowCount;
        summary->fileSizeBytes = finalInfo.size();
    }
    setError(errorMessage, {});
    return true;
}
