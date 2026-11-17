#include "GeoJsonExporter.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace
{
bool validCoordinate(double latitude, double longitude, double altitude)
{
    return std::isfinite(latitude) &&
           std::isfinite(longitude) &&
           std::isfinite(altitude) &&
           latitude >= -90.0 && latitude <= 90.0 &&
           longitude >= -180.0 && longitude <= 180.0;
}

void setError(QString *errorMessage, const QString &message)
{
    if (!message.isEmpty())
        qWarning().noquote() << "[GEOJSON EXPORT] Error:" << message;
    if (errorMessage)
        *errorMessage = message;
}

QJsonObject pointFeature(const QJsonArray &coordinates, const QJsonObject &properties)
{
    return {
        {QStringLiteral("type"), QStringLiteral("Feature")},
        {QStringLiteral("geometry"), QJsonObject{
             {QStringLiteral("type"), QStringLiteral("Point")},
             {QStringLiteral("coordinates"), coordinates}
         }},
        {QStringLiteral("properties"), properties}
    };
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

    if (selectedInfo.suffix().compare(QStringLiteral("geojson"),
                                      Qt::CaseInsensitive) != 0) {
        path = selectedInfo.dir().filePath(
            selectedInfo.completeBaseName() + QStringLiteral(".geojson"));
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
    qInfo().noquote() << "[GEOJSON EXPORT] Normalized absolute path:" << resolvedPath;
    return true;
}
}

QByteArray GeoJsonExporter::toJson(const Scenario &scenario,
                                   const QVector<AirbaseRecord> &airbases,
                                   QString *errorMessage,
                                   int *featureCount)
{
    return toJson(scenario, airbases, {}, errorMessage, featureCount);
}

QByteArray GeoJsonExporter::toJson(
    const Scenario &scenario,
    const QVector<AirbaseRecord> &airbases,
    const QVector<CommandHistoryEntry> &commandHistory,
    QString *errorMessage,
    int *featureCount)
{
    QJsonArray features;

    for (const Entity &entity : scenario.entities()) {
        if (entity.type() != QStringLiteral("aircraft"))
            continue;

        if (!validCoordinate(entity.latitude(), entity.longitude(), entity.altitude())) {
            setError(errorMessage,
                     QStringLiteral("Serialization failure: entity %1 has an invalid position.")
                         .arg(entity.id()));
            return {};
        }

        QJsonObject entityProperties{
            {QStringLiteral("entityId"), entity.id()},
            {QStringLiteral("name"), entity.name()},
            {QStringLiteral("type"), entity.type()},
            {QStringLiteral("callsign"), entity.callsign()},
            {QStringLiteral("altitude"), entity.altitude()},
            {QStringLiteral("speed"), entity.speed()},
            {QStringLiteral("heading"), entity.heading()},
            {QStringLiteral("status"), entity.status()},
            {QStringLiteral("aircraftType"), entity.aircraftType()},
            {QStringLiteral("formationId"), entity.formationId()},
            {QStringLiteral("formationType"), entity.formationType()},
            {QStringLiteral("formationLeaderId"), entity.formationLeaderId()},
            {QStringLiteral("formationSlot"), entity.formationSlot()}
        };
        features.append(pointFeature(
            QJsonArray{entity.longitude(), entity.latitude(), entity.altitude()},
            entityProperties));

        const QVector<Waypoint> &route = entity.route();
        if (route.size() < 2)
            continue;

        QJsonArray coordinates;
        QJsonArray waypoints;
        for (const Waypoint &waypoint : route) {
            if (!validCoordinate(waypoint.latitude, waypoint.longitude,
                                 waypoint.altitude)) {
                setError(errorMessage,
                         QStringLiteral("Serialization failure: entity %1 has an invalid waypoint: %2.")
                             .arg(entity.id(), waypoint.name));
                return {};
            }

            coordinates.append(QJsonArray{
                waypoint.longitude,
                waypoint.latitude,
                waypoint.altitude
            });
            waypoints.append(QJsonObject{
                {QStringLiteral("name"), waypoint.name},
                {QStringLiteral("latitude"), waypoint.latitude},
                {QStringLiteral("longitude"), waypoint.longitude},
                {QStringLiteral("altitude"), waypoint.altitude}
            });
        }

        QJsonObject routeProperties{
            {QStringLiteral("entityId"), entity.id()},
            {QStringLiteral("name"), entity.name()},
            {QStringLiteral("type"), entity.type()},
            {QStringLiteral("callsign"), entity.callsign()},
            {QStringLiteral("routeLoop"), entity.routeLoop()},
            {QStringLiteral("waypoints"), waypoints},
            {QStringLiteral("featureKind"), QStringLiteral("route")}
        };
        features.append(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("Feature")},
            {QStringLiteral("geometry"), QJsonObject{
                 {QStringLiteral("type"), QStringLiteral("LineString")},
                 {QStringLiteral("coordinates"), coordinates}
             }},
            {QStringLiteral("properties"), routeProperties}
        });
    }

    for (const ScenarioRoute &route : scenario.routes()) {
        if (route.waypoints.size() < 2)
            continue;
        QJsonArray coordinates;
        QJsonArray waypoints;
        for (const Waypoint &waypoint : route.waypoints) {
            if (!validCoordinate(waypoint.latitude, waypoint.longitude,
                                 waypoint.altitude)) {
                setError(errorMessage,
                         QStringLiteral("Serialization failure: route %1 has an invalid waypoint.")
                             .arg(route.id));
                return {};
            }
            coordinates.append(QJsonArray{waypoint.longitude, waypoint.latitude,
                                          waypoint.altitude});
            waypoints.append(QJsonObject{
                {QStringLiteral("name"), waypoint.name},
                {QStringLiteral("latitude"), waypoint.latitude},
                {QStringLiteral("longitude"), waypoint.longitude},
                {QStringLiteral("altitude"), waypoint.altitude}});
        }
        features.append(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("Feature")},
            {QStringLiteral("geometry"), QJsonObject{
                 {QStringLiteral("type"), QStringLiteral("LineString")},
                 {QStringLiteral("coordinates"), coordinates}}},
            {QStringLiteral("properties"), QJsonObject{
                 {QStringLiteral("routeId"), route.id},
                 {QStringLiteral("routeName"), route.name},
                 {QStringLiteral("aircraftId"), route.aircraftId},
                 {QStringLiteral("status"), route.status},
                 {QStringLiteral("waypoints"), waypoints},
                 {QStringLiteral("featureKind"), QStringLiteral("scenarioRoute")}}}});
    }

    for (const AirbaseRecord &airbase : airbases) {
        if (!airbase.coordinateAvailable)
            continue;
        const double latitude = airbase.coordinate.latitude();
        const double longitude = airbase.coordinate.longitude();
        if (!validCoordinate(latitude, longitude, 0.0)) {
            setError(errorMessage,
                     QStringLiteral("Serialization failure: airbase %1 has an invalid position.")
                         .arg(airbase.id));
            return {};
        }

        features.append(pointFeature(
            QJsonArray{longitude, latitude},
            QJsonObject{
                {QStringLiteral("airbaseId"), airbase.id},
                {QStringLiteral("name"), airbase.name},
                {QStringLiteral("type"), QStringLiteral("airbase")},
                {QStringLiteral("aliases"), QJsonArray::fromStringList(airbase.aliases)},
                {QStringLiteral("stateOrUnionTerritory"), airbase.stateOrUnionTerritory},
                {QStringLiteral("region"), airbase.region},
                {QStringLiteral("command"), airbase.command},
                {QStringLiteral("commandStatus"), airbase.commandStatus},
                {QStringLiteral("category"), airbase.category},
                {QStringLiteral("coordinateSource"), airbase.coordinateSource},
                {QStringLiteral("verificationStatus"), airbase.verificationStatus}
            }));
    }

    QJsonArray commandRecords;
    for (const CommandHistoryEntry &command : commandHistory) {
        QJsonObject record{
            {QStringLiteral("sequenceNumber"), command.sequenceNumber},
            {QStringLiteral("prompt"), command.prompt},
            {QStringLiteral("timestamp"), command.timestamp}
        };
        if (!command.processingStatus.isEmpty())
            record.insert(QStringLiteral("processingStatus"), command.processingStatus);
        if (!command.parsedOperation.isEmpty())
            record.insert(QStringLiteral("parsedOperation"), command.parsedOperation);
        commandRecords.append(record);
    }

    const QJsonObject collection{
        {QStringLiteral("type"), QStringLiteral("FeatureCollection")},
        {QStringLiteral("name"), scenario.name()},
        {QStringLiteral("description"), scenario.description()},
        {QStringLiteral("region"), scenario.region()},
        {QStringLiteral("properties"), QJsonObject{
             {QStringLiteral("exportTimestamp"),
              QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
             {QStringLiteral("commandCount"), commandHistory.size()},
             {QStringLiteral("commandHistory"), commandRecords}
         }},
        {QStringLiteral("features"), features}
    };
    const QByteArray json = QJsonDocument(collection).toJson(QJsonDocument::Indented);
    if (json.isEmpty()) {
        setError(errorMessage, QStringLiteral("Serialization failure: GeoJSON is empty."));
        return {};
    }

    if (featureCount)
        *featureCount = features.size();
    setError(errorMessage, {});
    return json;
}

QByteArray GeoJsonExporter::toJson(const Scenario &scenario, QString *errorMessage)
{
    return toJson(scenario, {}, errorMessage);
}

bool GeoJsonExporter::writeFile(const Scenario &scenario,
                                const QVector<AirbaseRecord> &airbases,
                                const QString &filePath,
                                ExportSummary *summary,
                                QString *errorMessage)
{
    return writeFile(scenario, airbases, {}, filePath, summary, errorMessage);
}

bool GeoJsonExporter::writeFile(
    const Scenario &scenario,
    const QVector<AirbaseRecord> &airbases,
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
    for (const ScenarioRoute &route : scenario.routes())
        if (route.waypoints.size() >= 2)
            ++routeCount;
    const int airbaseCount = static_cast<int>(std::count_if(
        airbases.cbegin(), airbases.cend(),
        [](const AirbaseRecord &airbase) { return airbase.coordinateAvailable; }));
    qInfo().noquote() << "[GEOJSON EXPORT] writeFile requestedPath="
                      << filePath << "aircraft=" << aircraftCount
                      << "airbases=" << airbaseCount << "routes=" << routeCount
                      << "commands=" << commandHistory.size();

    QString absolutePath;
    if (!normalizedExportPath(filePath, &absolutePath, errorMessage))
        return false;

    int featureCount = 0;
    const QByteArray json = toJson(scenario, airbases, commandHistory,
                                   errorMessage, &featureCount);
    if (json.isEmpty())
        return false;
    qInfo().noquote() << "[GEOJSON EXPORT] Serialized bytes=" << json.size()
                      << "features=" << featureCount;
    if (featureCount == 0 && commandHistory.isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("No exportable aircraft, airbases, or routes are available."));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument serializedDocument = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError ||
        !serializedDocument.isObject() ||
        serializedDocument.object().value(QStringLiteral("type")).toString() !=
            QStringLiteral("FeatureCollection")) {
        setError(errorMessage,
                 QStringLiteral("Serialization failure: %1")
                     .arg(parseError.errorString()));
        return false;
    }

    QSaveFile file(absolutePath);
    const bool opened = file.open(QIODevice::WriteOnly);
    qInfo().noquote() << "[GEOJSON EXPORT] Open result=" << opened
                      << "path=" << absolutePath;
    if (!opened) {
        setError(errorMessage,
                 QStringLiteral("File write failure for %1: %2")
                     .arg(absolutePath, file.errorString()));
        return false;
    }

    const qint64 bytesWritten = file.write(json);
    qInfo().noquote() << "[GEOJSON EXPORT] Bytes written=" << bytesWritten
                      << "expected=" << json.size();
    if (bytesWritten != json.size()) {
        setError(errorMessage,
                 QStringLiteral("File write failure for %1: wrote %2 of %3 bytes (%4).")
                     .arg(absolutePath)
                     .arg(bytesWritten)
                     .arg(json.size())
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
    qInfo().noquote() << "[GEOJSON EXPORT] Commit result=" << committed
                      << "path=" << absolutePath
                      << "error=" << file.errorString();
    if (!committed) {
        setError(errorMessage,
                 QStringLiteral("File write failure committing %1: %2")
                     .arg(absolutePath, file.errorString()));
        return false;
    }

    const QFileInfo finalInfo(absolutePath);
    qInfo().noquote() << "[GEOJSON EXPORT] Final file size="
                      << (finalInfo.exists() ? finalInfo.size() : -1)
                      << "path=" << absolutePath;
    if (!finalInfo.exists() || !finalInfo.isFile() ||
        finalInfo.size() != json.size() || finalInfo.size() <= 0) {
        setError(errorMessage,
                 QStringLiteral("File verification failure for %1: expected %2 bytes, found %3.")
                     .arg(absolutePath)
                     .arg(json.size())
                     .arg(finalInfo.exists() ? finalInfo.size() : -1));
        return false;
    }

    QFile verifyFile(absolutePath);
    if (!verifyFile.open(QIODevice::ReadOnly)) {
        setError(errorMessage,
                 QStringLiteral("File verification failure for %1: %2")
                     .arg(absolutePath, verifyFile.errorString()));
        return false;
    }
    const QByteArray writtenJson = verifyFile.readAll();
    QJsonParseError writtenParseError;
    const QJsonDocument writtenDocument =
        QJsonDocument::fromJson(writtenJson, &writtenParseError);
    const QJsonArray writtenFeatures =
        writtenDocument.object().value(QStringLiteral("features")).toArray();
    const QJsonObject exportProperties =
        writtenDocument.object().value(QStringLiteral("properties")).toObject();
    const QJsonArray writtenCommands =
        exportProperties.value(QStringLiteral("commandHistory")).toArray();
    int writtenAircraftPoints = 0;
    int writtenAirbasePoints = 0;
    int writtenRoutes = 0;
    bool validFeatures = true;
    bool validCommands = true;
    for (const QJsonValue &featureValue : writtenFeatures) {
        const QJsonObject feature = featureValue.toObject();
        const QJsonObject geometry = feature.value(QStringLiteral("geometry")).toObject();
        const QJsonObject properties = feature.value(QStringLiteral("properties")).toObject();
        if (feature.value(QStringLiteral("type")).toString() != QStringLiteral("Feature") ||
            !geometry.value(QStringLiteral("coordinates")).isArray() ||
            !feature.value(QStringLiteral("properties")).isObject()) {
            validFeatures = false;
            break;
        }
        const QString geometryType = geometry.value(QStringLiteral("type")).toString();
        if (geometryType == QStringLiteral("Point")) {
            if (properties.contains(QStringLiteral("entityId")))
                ++writtenAircraftPoints;
            else if (properties.contains(QStringLiteral("airbaseId")))
                ++writtenAirbasePoints;
        } else if (geometryType == QStringLiteral("LineString")) {
            ++writtenRoutes;
        } else {
            validFeatures = false;
            break;
        }
    }
    if (writtenCommands.size() != commandHistory.size()) {
        validCommands = false;
    } else {
        for (int index = 0; index < commandHistory.size(); ++index) {
            const CommandHistoryEntry &expected = commandHistory.at(index);
            const QJsonObject actual = writtenCommands.at(index).toObject();
            if (actual.value(QStringLiteral("sequenceNumber")).toInt(-1) !=
                    expected.sequenceNumber ||
                actual.value(QStringLiteral("prompt")).toString() != expected.prompt ||
                actual.value(QStringLiteral("timestamp")).toString() !=
                    expected.timestamp ||
                (expected.processingStatus.isEmpty()
                     ? actual.contains(QStringLiteral("processingStatus"))
                     : actual.value(QStringLiteral("processingStatus")).toString() !=
                           expected.processingStatus) ||
                (expected.parsedOperation.isEmpty()
                     ? actual.contains(QStringLiteral("parsedOperation"))
                     : actual.value(QStringLiteral("parsedOperation")).toString() !=
                           expected.parsedOperation)) {
                validCommands = false;
                break;
            }
        }
    }
    const bool expectedFeatureCounts =
        writtenAircraftPoints == aircraftCount &&
        writtenAirbasePoints == airbaseCount && writtenRoutes == routeCount;
    qInfo().noquote() << "[GEOJSON EXPORT] Read-back validation bytes="
                      << writtenJson.size() << "features=" << writtenFeatures.size()
                      << "aircraftPoints=" << writtenAircraftPoints
                      << "airbasePoints=" << writtenAirbasePoints
                      << "routes=" << writtenRoutes
                      << "commands=" << writtenCommands.size();
    if (writtenJson.size() != json.size() ||
        writtenJson != json ||
        writtenParseError.error != QJsonParseError::NoError ||
        !writtenDocument.isObject() ||
        writtenDocument.object().value(QStringLiteral("type")).toString() !=
            QStringLiteral("FeatureCollection") ||
        !writtenDocument.object().value(QStringLiteral("features")).isArray() ||
        writtenFeatures.size() != featureCount || !validFeatures ||
        !expectedFeatureCounts ||
        !exportProperties.value(QStringLiteral("exportTimestamp")).isString() ||
        exportProperties.value(QStringLiteral("commandCount")).toInt(-1) !=
            commandHistory.size() ||
        writtenCommands.size() != commandHistory.size() || !validCommands) {
        setError(errorMessage,
                 QStringLiteral("File verification failure for %1: saved JSON is incomplete or invalid.")
                     .arg(absolutePath));
        return false;
    }

    if (summary) {
        summary->absolutePath = absolutePath;
        summary->featureCount = featureCount;
        summary->fileSizeBytes = finalInfo.size();
    }
    setError(errorMessage, {});
    return true;
}
