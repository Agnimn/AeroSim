#include "ScenarioEngine.h"

#include "../ai/AICommand.h"
#include "../data/CsvExporter.h"
#include "../data/GeoJsonExporter.h"
#include "FormationGenerator.h"

#include <QDebug>
#include <QDateTime>
#include <QPair>
#include <QSet>
#include <QRegularExpression>
#include <QtMath>
#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
    constexpr double EarthRadiusNm = 3440.065;

    QString operationName(ScenarioOperation operation)
    {
        switch (operation)
        {
        case ScenarioOperation::Add: return QStringLiteral("ADD_ENTITIES");
        case ScenarioOperation::Replace: return QStringLiteral("REPLACE_ENTITIES");
        case ScenarioOperation::Query: return QStringLiteral("QUERY_SCENARIO");
        case ScenarioOperation::Reset: return QStringLiteral("RESET_SCENARIO");
        case ScenarioOperation::Save: return QStringLiteral("SAVE_SCENARIO");
        case ScenarioOperation::Load: return QStringLiteral("LOAD_SCENARIO");
        case ScenarioOperation::Move: return QStringLiteral("MOVE_ENTITY");
        case ScenarioOperation::ChangeRoute: return QStringLiteral("SET_ROUTE");
        case ScenarioOperation::StopAircraft: return QStringLiteral("STOP_AIRCRAFT");
        case ScenarioOperation::StopAllAircraft: return QStringLiteral("STOP_ALL_AIRCRAFT");
        case ScenarioOperation::PauseAircraft: return QStringLiteral("PAUSE_AIRCRAFT");
        case ScenarioOperation::ResumeAircraft: return QStringLiteral("RESUME_AIRCRAFT");
        case ScenarioOperation::StartAircraft: return QStringLiteral("START_AIRCRAFT");
        case ScenarioOperation::RemoveAircraft: return QStringLiteral("REMOVE_AIRCRAFT");
        case ScenarioOperation::RemoveLastAircraft:
            return QStringLiteral("REMOVE_LAST_AIRCRAFT");
        case ScenarioOperation::RemoveAllAircraft:
            return QStringLiteral("REMOVE_ALL_AIRCRAFT");
        case ScenarioOperation::StopFormation: return QStringLiteral("STOP_FORMATION");
        case ScenarioOperation::ResumeFormation: return QStringLiteral("RESUME_FORMATION");
        case ScenarioOperation::DisbandFormation: return QStringLiteral("DISBAND_FORMATION");
        case ScenarioOperation::ScheduleTakeoff: return QStringLiteral("SCHEDULE_TAKEOFF");
        case ScenarioOperation::ListScheduledTakeoffs:
            return QStringLiteral("LIST_SCHEDULED_TAKEOFFS");
        case ScenarioOperation::CancelScheduledTakeoff:
            return QStringLiteral("CANCEL_EVENT");
        case ScenarioOperation::CreateFormation: return QStringLiteral("CREATE_FORMATION");
        case ScenarioOperation::MoveFormation: return QStringLiteral("MOVE_FORMATION");
        case ScenarioOperation::CreateRoute: return QStringLiteral("CREATE_ROUTE");
        case ScenarioOperation::DisplayRoute: return QStringLiteral("DISPLAY_ROUTE");
        case ScenarioOperation::AssignRoute: return QStringLiteral("ASSIGN_ROUTE");
        case ScenarioOperation::MoveAlongRoute: return QStringLiteral("MOVE_ALONG_ROUTE");
        }
        return {};
    }

    const std::vector<QPair<double, double>> northIndiaPoints = {
        {28.6139, 77.2090}, {30.7333, 76.7794}, {26.9124, 75.7873}, {27.1767, 78.0081}, {31.6340, 74.8723}, {30.3165, 78.0322}, {29.9457, 78.1642}, {25.4358, 81.8463}};
    const std::vector<QPair<double, double>> southIndiaPoints = {
        {12.9716, 77.5946}, {13.0827, 80.2707}, {17.3850, 78.4867}, {11.0168, 76.9558}, {15.3173, 75.7139}, {9.9312, 76.2673}};
    const std::vector<QPair<double, double>> eastIndiaPoints = {
        {22.5726, 88.3639}, {20.2961, 85.8245}, {26.1445, 91.7362}, {23.3441, 85.3096}, {25.5941, 85.1376}};
    const std::vector<QPair<double, double>> westIndiaPoints = {
        {19.0760, 72.8777}, {23.0225, 72.5714}, {21.1702, 72.8311}, {15.4909, 73.8278}, {18.5204, 73.8567}};
    const std::vector<QPair<double, double>> delhiPoints = {
        {28.6139, 77.2090}, {28.6520, 77.2315}, {28.5355, 77.3910}, {28.7041, 77.1025}, {28.8795, 77.1060}, {28.5677, 77.2704}};
    const std::vector<QPair<double, double>> bengaluruPoints = {
        {12.9716, 77.5946}, {13.0186, 77.5671}, {12.9165, 77.6165}, {13.0827, 77.6633}, {12.9239, 77.5515}, {12.9950, 77.7150}};
    const std::vector<QPair<double, double>> defaultPoints = {{22.5, 79.0}};
    // Conservative scenario approximation, not an authoritative international
    // boundary dataset. The vertices stay inside the intended northern Indian
    // states/UTs, including J&K and Ladakh, to avoid placing entities in
    // neighboring countries until authoritative state geometry is integrated.
    const std::vector<QPair<double, double>> northIndiaBoundary = {
        {35.0, 77.0}, {34.5, 78.5}, {34.8, 79.5}, {33.0, 80.0}, {31.0, 79.5}, {29.2, 79.5}, {28.5, 80.0}, {27.0, 81.0}, {25.0, 82.0}, {24.8, 80.0}, {25.0, 76.5}, {27.0, 75.2}, {30.0, 75.0}, {32.5, 76.0}};
    const std::vector<QPair<double, double>> southIndiaBoundary = {
        {18.8, 72.8}, {18.8, 82.8}, {15.8, 83.8}, {8.0, 78.8}, {8.0, 74.0}, {12.0, 72.0}};
    const std::vector<QPair<double, double>> eastIndiaBoundary = {
        {28.2, 84.0}, {28.2, 95.0}, {25.0, 96.5}, {20.0, 92.5}, {18.0, 87.0}, {21.0, 83.0}};
    const std::vector<QPair<double, double>> westIndiaBoundary = {
        {24.8, 68.0}, {24.8, 76.8}, {22.0, 78.8}, {15.0, 74.0}, {15.0, 72.0}, {19.0, 68.0}};

    double normalizeHeading(double heading)
    {
        const double normalized = std::fmod(heading, 360.0);
        return normalized < 0.0 ? normalized + 360.0 : normalized;
    }

    QGeoCoordinate destinationPoint(double latitude, double longitude,
                                    double bearing, double distanceNm)
    {
        const double angularDistance = distanceNm / EarthRadiusNm;
        const double bearingRadians = qDegreesToRadians(bearing);
        const double latitudeRadians = qDegreesToRadians(latitude);
        const double longitudeRadians = qDegreesToRadians(longitude);
        const double destinationLatitude = qAsin(
            qSin(latitudeRadians) * qCos(angularDistance) +
            qCos(latitudeRadians) * qSin(angularDistance) * qCos(bearingRadians));
        const double destinationLongitude = longitudeRadians + qAtan2(
                                                                   qSin(bearingRadians) * qSin(angularDistance) * qCos(latitudeRadians),
                                                                   qCos(angularDistance) - qSin(latitudeRadians) * qSin(destinationLatitude));
        return QGeoCoordinate(qRadiansToDegrees(destinationLatitude),
                              qRadiansToDegrees(destinationLongitude));
    }

    double bearingTo(double latitude1, double longitude1,
                     double latitude2, double longitude2)
    {
        const double lat1 = qDegreesToRadians(latitude1);
        const double lat2 = qDegreesToRadians(latitude2);
        const double deltaLongitude = qDegreesToRadians(longitude2 - longitude1);
        const double y = qSin(deltaLongitude) * qCos(lat2);
        const double x = qCos(lat1) * qSin(lat2) -
                         qSin(lat1) * qCos(lat2) * qCos(deltaLongitude);
        return normalizeHeading(qRadiansToDegrees(qAtan2(y, x)));
    }

    double distanceNm(double latitude1, double longitude1,
                      double latitude2, double longitude2)
    {
        const double latitude1Radians = qDegreesToRadians(latitude1);
        const double latitude2Radians = qDegreesToRadians(latitude2);
        const double deltaLatitude = latitude2Radians - latitude1Radians;
        const double deltaLongitude =
            qDegreesToRadians(longitude2 - longitude1);
        const double haversine =
            qSin(deltaLatitude * 0.5) * qSin(deltaLatitude * 0.5) +
            qCos(latitude1Radians) * qCos(latitude2Radians) *
                qSin(deltaLongitude * 0.5) * qSin(deltaLongitude * 0.5);
        return EarthRadiusNm * 2.0 *
               qAtan2(qSqrt(haversine), qSqrt(qMax(0.0, 1.0 - haversine)));
    }

    QGeoCoordinate aircraftAnchor(int aircraftIndex, int aircraftCount,
                                  double centerLatitude, double centerLongitude,
                                  double radiusNm)
    {
        const double fraction = (aircraftIndex + 0.5) / qMax(1, aircraftCount);
        const double angle = normalizeHeading(aircraftIndex * 137.507764);
        return destinationPoint(centerLatitude, centerLongitude, angle,
                                radiusNm * qSqrt(fraction));
    }

    bool coordinateInPolygon(const QGeoCoordinate &coordinate,
                             const std::vector<QPair<double, double>> &polygon)
    {
        if (polygon.size() < 3)
            return false;

        bool inside = false;
        for (std::size_t index = 0, previous = polygon.size() - 1;
             index < polygon.size(); previous = index++)
        {
            const double currentLatitude = polygon.at(index).first;
            const double currentLongitude = polygon.at(index).second;
            const double previousLatitude = polygon.at(previous).first;
            const double previousLongitude = polygon.at(previous).second;
            const bool crossesLatitude =
                (currentLatitude > coordinate.latitude()) !=
                (previousLatitude > coordinate.latitude());
            if (crossesLatitude &&
                coordinate.longitude() <
                    (previousLongitude - currentLongitude) *
                            (coordinate.latitude() - currentLatitude) /
                            (previousLatitude - currentLatitude) +
                        currentLongitude)
                inside = !inside;
        }
        return inside;
    }

    QVector<QGeoCoordinate> generatePositions(
        int count, const std::vector<QPair<double, double>> *boundary,
        double centerLatitude, double centerLongitude, double radiusNm)
    {
        QVector<QGeoCoordinate> positions;
        if (count <= 0 || radiusNm <= 0.0)
            return positions;

        double minimumLatitude = std::numeric_limits<double>::max();
        double maximumLatitude = std::numeric_limits<double>::lowest();
        double minimumLongitude = std::numeric_limits<double>::max();
        double maximumLongitude = std::numeric_limits<double>::lowest();
        if (boundary && boundary->size() >= 3)
        {
            for (const auto &point : *boundary)
            {
                minimumLatitude = qMin(minimumLatitude, point.first);
                maximumLatitude = qMax(maximumLatitude, point.first);
                minimumLongitude = qMin(minimumLongitude, point.second);
                maximumLongitude = qMax(maximumLongitude, point.second);
            }
        }
        else
        {
            const QGeoCoordinate north =
                destinationPoint(centerLatitude, centerLongitude, 0.0, radiusNm);
            const QGeoCoordinate south =
                destinationPoint(centerLatitude, centerLongitude, 180.0, radiusNm);
            const QGeoCoordinate east =
                destinationPoint(centerLatitude, centerLongitude, 90.0, radiusNm);
            const QGeoCoordinate west =
                destinationPoint(centerLatitude, centerLongitude, 270.0, radiusNm);
            minimumLatitude = south.latitude();
            maximumLatitude = north.latitude();
            minimumLongitude = west.longitude();
            maximumLongitude = east.longitude();
        }

        const double latitudeRange = maximumLatitude - minimumLatitude;
        const double longitudeRange = maximumLongitude - minimumLongitude;
        if (latitudeRange <= 0.0 || longitudeRange <= 0.0)
            return positions;
        const double longitudeScale = qCos(qDegreesToRadians(
            (minimumLatitude + maximumLatitude) * 0.5));
        const double aspect = longitudeRange * longitudeScale / latitudeRange;
        const int columns = qMax(1, qCeil(qSqrt(count * aspect)));
        const int rows = qMax(1, qCeil(static_cast<double>(count) / columns));
        const double cellLatitude = latitudeRange / rows;
        const double cellLongitude = longitudeRange / columns;

        for (int row = 0; row < rows && positions.size() < count; ++row)
        {
            for (int column = 0; column < columns && positions.size() < count; ++column)
            {
                const double jitterLatitude =
                    0.23 + std::fmod((row * 0.61803398875 + column * 0.38196601125), 0.46);
                const double jitterLongitude =
                    0.19 + std::fmod((column * 0.75487766625 + row * 0.24512233375), 0.62);
                const QGeoCoordinate candidate(
                    minimumLatitude + (row + jitterLatitude) * cellLatitude,
                    minimumLongitude + (column + jitterLongitude) * cellLongitude);
                const bool insideBoundary =
                    !boundary || coordinateInPolygon(candidate, *boundary);
                const bool insideRadius =
                    distanceNm(centerLatitude, centerLongitude,
                               candidate.latitude(), candidate.longitude()) <= radiusNm;
                if (insideBoundary && insideRadius)
                    positions.push_back(candidate);
            }
        }

        constexpr int MaximumFallbackCandidates = 100000;
        for (int candidateIndex = 0;
             positions.size() < count && candidateIndex < MaximumFallbackCandidates;
             ++candidateIndex)
        {
            const double latitudeFraction = std::fmod(
                (candidateIndex + 1) * 0.61803398875, 1.0);
            const double longitudeFraction = std::fmod(
                (candidateIndex + 1) * 0.75487766625, 1.0);
            const QGeoCoordinate candidate(
                minimumLatitude + latitudeFraction * latitudeRange,
                minimumLongitude + longitudeFraction * longitudeRange);
            const bool insideBoundary =
                !boundary || coordinateInPolygon(candidate, *boundary);
            const bool insideRadius =
                distanceNm(centerLatitude, centerLongitude,
                           candidate.latitude(), candidate.longitude()) <= radiusNm;
            if (insideBoundary && insideRadius)
                positions.push_back(candidate);
        }

        return positions;
    }

    const std::vector<QPair<double, double>> *boundaryForRegion(const QString &region)
    {
        if (region == QStringLiteral("North India"))
            return &northIndiaBoundary;
        if (region == QStringLiteral("South India"))
            return &southIndiaBoundary;
        if (region == QStringLiteral("East India"))
            return &eastIndiaBoundary;
        if (region == QStringLiteral("West India"))
            return &westIndiaBoundary;
        return nullptr;
    }

    void logAircraftDistribution(const QString &region,
                                 const QVector<QGeoCoordinate> &positions)
    {
        if (positions.isEmpty())
            return;

        double minimumLatitude = positions.first().latitude();
        double maximumLatitude = minimumLatitude;
        double minimumLongitude = positions.first().longitude();
        double maximumLongitude = minimumLongitude;
        double minimumSeparationNm = std::numeric_limits<double>::max();
        QSet<QString> occupiedCells;
        for (int index = 0; index < positions.size(); ++index)
        {
            const QGeoCoordinate &position = positions.at(index);
            minimumLatitude = qMin(minimumLatitude, position.latitude());
            maximumLatitude = qMax(maximumLatitude, position.latitude());
            minimumLongitude = qMin(minimumLongitude, position.longitude());
            maximumLongitude = qMax(maximumLongitude, position.longitude());
            occupiedCells.insert(QStringLiteral("%1:%2")
                                     .arg(qFloor(position.latitude()))
                                     .arg(qFloor(position.longitude())));
            for (int previous = 0; previous < index; ++previous)
            {
                const QGeoCoordinate &other = positions.at(previous);
                const double northNm = (position.latitude() - other.latitude()) * 60.0;
                const double eastNm = (position.longitude() - other.longitude()) *
                                      60.0 * qCos(qDegreesToRadians((position.latitude() + other.latitude()) * 0.5));
                minimumSeparationNm = qMin(minimumSeparationNm,
                                           qSqrt(northNm * northNm + eastNm * eastNm));
            }
        }
        qInfo().noquote() << "[DISTRIBUTION] region=" << region
                          << "aircraft=" << positions.size()
                          << "lat=" << minimumLatitude << ".." << maximumLatitude
                          << "lon=" << minimumLongitude << ".." << maximumLongitude
                          << "cells=" << occupiedCells.size()
                          << "minSeparationNm=" << minimumSeparationNm;
    }

    Waypoint waypoint(const QString &name, const QGeoCoordinate &position, double altitude)
    {
        return {name, position.latitude(), position.longitude(), altitude};
    }

    bool resolveVerifiedRouteDestination(const AirbaseRegistry &registry,
                                         const QString &name,
                                         QGeoCoordinate &coordinate)
    {
        QVector<const AirbaseRecord *> matches = registry.matchingText(name);
        if (matches.isEmpty())
            for (const AirbaseRecord &record : registry.records())
                if (record.stateOrUnionTerritory.compare(name, Qt::CaseInsensitive) == 0)
                    matches.push_back(&record);

        QVector<const AirbaseRecord *> verifiedMatches;
        for (const AirbaseRecord *record : matches)
            if (record->coordinateAvailable &&
                record->verificationStatus == QStringLiteral("verified_public_reference"))
                verifiedMatches.push_back(record);
        if (verifiedMatches.size() == 1)
        {
            coordinate = verifiedMatches.first()->coordinate;
            return coordinate.isValid();
        }

        if (!verifiedMatches.isEmpty())
            return false;

        const QString normalizedName = name.trimmed().toLower();
        if (normalizedName == QStringLiteral("chennai"))
            coordinate = QGeoCoordinate(13.0827, 80.2707);
        else if (normalizedName == QStringLiteral("delhi"))
            coordinate = QGeoCoordinate(28.6139, 77.2090);
        else if (normalizedName == QStringLiteral("mumbai") ||
                 normalizedName == QStringLiteral("bombay"))
            coordinate = QGeoCoordinate(19.0760, 72.8777);
        else if (normalizedName == QStringLiteral("hyderabad"))
            coordinate = QGeoCoordinate(17.3850, 78.4867);
        else if (normalizedName == QStringLiteral("bengaluru") ||
                 normalizedName == QStringLiteral("bangalore"))
            coordinate = QGeoCoordinate(12.9716, 77.5946);
        else if (normalizedName == QStringLiteral("mysuru") ||
                 normalizedName == QStringLiteral("mysore"))
            coordinate = QGeoCoordinate(12.2958, 76.6394);
        else if (normalizedName == QStringLiteral("kolkata"))
            coordinate = QGeoCoordinate(22.5726, 88.3639);
        else
            return false;
        return coordinate.isValid();
    }

    bool resolveNaturalRouteLocation(const AirbaseRegistry &registry,
                                     const QString &name,
                                     QGeoCoordinate &coordinate)
    {
        const QString normalizedName = name.trimmed().toLower();
        if (normalizedName == QStringLiteral("bengaluru") ||
            normalizedName == QStringLiteral("bangalore"))
            coordinate = QGeoCoordinate(12.9716, 77.5946);
        else if (normalizedName == QStringLiteral("chennai"))
            coordinate = QGeoCoordinate(13.0827, 80.2707);
        else if (normalizedName == QStringLiteral("hyderabad"))
            coordinate = QGeoCoordinate(17.3850, 78.4867);
        else if (normalizedName == QStringLiteral("mumbai") ||
                 normalizedName == QStringLiteral("bombay"))
            coordinate = QGeoCoordinate(19.0760, 72.8777);
        else if (normalizedName == QStringLiteral("delhi"))
            coordinate = QGeoCoordinate(28.6139, 77.2090);
        else if (normalizedName == QStringLiteral("jaipur"))
            coordinate = QGeoCoordinate(26.9124, 75.7873);
        else if (normalizedName == QStringLiteral("leh"))
            coordinate = QGeoCoordinate(34.1526, 77.5771);
        else if (normalizedName == QStringLiteral("srinagar"))
            coordinate = QGeoCoordinate(34.0837, 74.7973);
        else
            return resolveVerifiedRouteDestination(registry, name, coordinate);
        return coordinate.isValid();
    }

    QString displayDestinationName(QString name)
    {
        bool capitalize = true;
        for (int index = 0; index < name.size(); ++index)
        {
            if (capitalize && name.at(index).isLetter())
                name[index] = name.at(index).toUpper();
            capitalize = name.at(index).isSpace() || name.at(index) == QLatin1Char('/') ||
                         name.at(index) == QLatin1Char('-');
        }
        return name;
    }

    bool coordinateWithinScenario(const QGeoCoordinate &coordinate,
                                  const std::vector<QPair<double, double>> *boundary,
                                  double centerLatitude, double centerLongitude,
                                  double radiusNm)
    {
        if (!coordinate.isValid() || !qIsFinite(coordinate.latitude()) ||
            !qIsFinite(coordinate.longitude()) ||
            coordinate.latitude() < -90.0 || coordinate.latitude() > 90.0 ||
            coordinate.longitude() < -180.0 || coordinate.longitude() > 180.0)
            return false;
        const bool insideBoundary =
            !boundary || coordinateInPolygon(coordinate, *boundary);
        return insideBoundary &&
               distanceNm(centerLatitude, centerLongitude,
                          coordinate.latitude(), coordinate.longitude()) <= radiusNm;
    }

    bool clampWaypointToScenario(Waypoint &candidate, const QGeoCoordinate &origin,
                                 const std::vector<QPair<double, double>> *boundary,
                                 double centerLatitude, double centerLongitude,
                                 double radiusNm)
    {
        const QGeoCoordinate target(candidate.latitude, candidate.longitude);
        if (coordinateWithinScenario(target, boundary, centerLatitude,
                                     centerLongitude, radiusNm))
            return true;
        if (!coordinateWithinScenario(origin, boundary, centerLatitude,
                                      centerLongitude, radiusNm))
            return false;

        const double distance = distanceNm(origin.latitude(), origin.longitude(),
                                           target.latitude(), target.longitude());
        const double bearing = bearingTo(origin.latitude(), origin.longitude(),
                                         target.latitude(), target.longitude());
        double low = 0.0;
        double high = distance;
        for (int iteration = 0; iteration < 32; ++iteration)
        {
            const double middle = (low + high) * 0.5;
            const QGeoCoordinate point = destinationPoint(
                origin.latitude(), origin.longitude(), bearing, middle);
            if (coordinateWithinScenario(point, boundary, centerLatitude,
                                         centerLongitude, radiusNm))
                low = middle;
            else
                high = middle;
        }
        const QGeoCoordinate clamped = destinationPoint(
            origin.latitude(), origin.longitude(), bearing, low);
        candidate.latitude = clamped.latitude();
        candidate.longitude = clamped.longitude();
        qInfo().noquote() << "[ROUTE] clamped out-of-bounds waypoint"
                          << candidate.name;
        return coordinateWithinScenario(clamped, boundary, centerLatitude,
                                        centerLongitude, radiusNm);
    }

    bool validateRoute(QVector<Waypoint> &route,
                       const std::vector<QPair<double, double>> *boundary,
                       double centerLatitude, double centerLongitude,
                       double radiusNm)
    {
        if (route.isEmpty())
            return false;
        QGeoCoordinate previous(route.first().latitude, route.first().longitude);
        if (!clampWaypointToScenario(route.first(), previous, boundary,
                                     centerLatitude, centerLongitude, radiusNm))
            return false;
        previous = QGeoCoordinate(route.first().latitude, route.first().longitude);
        for (int index = 1; index < route.size(); ++index)
        {
            if (!clampWaypointToScenario(route[index], previous, boundary,
                                         centerLatitude, centerLongitude, radiusNm))
                return false;
            previous = QGeoCoordinate(route[index].latitude, route[index].longitude);
        }
        return true;
    }

    QVector<Waypoint> makeRoute(const QString &pattern, int aircraftIndex, int aircraftCount,
                                double centerLatitude, double centerLongitude,
                                double altitude, double initialHeading,
                                const QGeoCoordinate &initialPosition, bool *loopRoute)
    {
        QVector<Waypoint> route;
        *loopRoute = pattern == QStringLiteral("holding");
        if (pattern == QStringLiteral("crossing"))
        {
            const double angle = normalizeHeading(aircraftIndex * 137.507764);
            const QGeoCoordinate center(centerLatitude, centerLongitude);
            const QGeoCoordinate end =
                destinationPoint(centerLatitude, centerLongitude, angle + 180.0, 18.0);
            route << waypoint(QStringLiteral("ENTRY_%1").arg(aircraftIndex + 1), initialPosition, altitude)
                  << waypoint(QStringLiteral("CROSS_%1").arg(aircraftIndex + 1), center, altitude)
                  << waypoint(QStringLiteral("EXIT_%1").arg(aircraftIndex + 1), end, altitude);
        }
        else if (pattern == QStringLiteral("parallel"))
        {
            const QGeoCoordinate start = initialPosition;
            const QGeoCoordinate middle =
                destinationPoint(start.latitude(), start.longitude(), 90.0, 30.0);
            const QGeoCoordinate end =
                destinationPoint(start.latitude(), start.longitude(), 90.0, 60.0);
            route << waypoint(QStringLiteral("PARALLEL_IN_%1").arg(aircraftIndex + 1), start, altitude)
                  << waypoint(QStringLiteral("PARALLEL_%1").arg(aircraftIndex + 1), middle, altitude)
                  << waypoint(QStringLiteral("PARALLEL_OUT_%1").arg(aircraftIndex + 1), end, altitude);
        }
        else if (pattern == QStringLiteral("holding"))
        {
            route << waypoint(QStringLiteral("HOLD_%1_1").arg(aircraftIndex + 1),
                              initialPosition, altitude);
            for (int point = 1; point < 4; ++point)
                route << waypoint(QStringLiteral("HOLD_%1_%2").arg(aircraftIndex + 1).arg(point + 1),
                                  destinationPoint(initialPosition.latitude(), initialPosition.longitude(),
                                                   45.0 + point * 90.0, 8.0),
                                  altitude);
        }
        else if (pattern == QStringLiteral("arrival") ||
                 pattern == QStringLiteral("departure"))
        {
            const double direction = normalizeHeading(aircraftIndex * 53.0 + initialHeading);
            const QGeoCoordinate middle =
                destinationPoint(initialPosition.latitude(), initialPosition.longitude(),
                                 direction + 180.0, 16.0);
            const QGeoCoordinate airport =
                destinationPoint(initialPosition.latitude(), initialPosition.longitude(),
                                 direction + 180.0, 30.0);
            if (pattern == QStringLiteral("arrival"))
            {
                route << waypoint(QStringLiteral("ARRIVAL_%1").arg(aircraftIndex + 1), initialPosition, altitude)
                      << waypoint(QStringLiteral("FINAL_%1").arg(aircraftIndex + 1), middle, altitude * 0.5)
                      << waypoint(QStringLiteral("LAND_%1").arg(aircraftIndex + 1), airport, 0.0);
            }
            else
            {
                route << waypoint(QStringLiteral("TAKEOFF_%1").arg(aircraftIndex + 1), airport, 0.0)
                      << waypoint(QStringLiteral("CLIMB_%1").arg(aircraftIndex + 1), middle, altitude * 0.5)
                      << waypoint(QStringLiteral("DEPART_%1").arg(aircraftIndex + 1),
                                  destinationPoint(initialPosition.latitude(), initialPosition.longitude(),
                                                   direction, 30.0),
                                  altitude);
            }
        }
        else
        {
            const double direction = pattern == QStringLiteral("straight")
                                         ? normalizeHeading(initialHeading)
                                         : normalizeHeading(aircraftIndex * 137.507764 +
                                                            initialHeading);
            const QGeoCoordinate middle =
                destinationPoint(initialPosition.latitude(), initialPosition.longitude(),
                                 direction, 25.0);
            const QGeoCoordinate end =
                destinationPoint(initialPosition.latitude(), initialPosition.longitude(),
                                 direction, 50.0);
            route << waypoint(QStringLiteral("START_%1").arg(aircraftIndex + 1), initialPosition, altitude)
                  << waypoint(QStringLiteral("WPT_%1A").arg(aircraftIndex + 1), middle, altitude)
                  << waypoint(QStringLiteral("WPT_%1B").arg(aircraftIndex + 1), end, altitude);
        }
        return route;
    }
}

ScenarioEngine::ScenarioEngine(QObject *parent)
    : QObject(parent), m_airbaseRegistry(this), m_parser(&m_airbaseRegistry),
      m_ollamaClient(this)
{
    connect(&m_simulationEngine, &SimulationEngine::simulationStateChanged,
            this, &ScenarioEngine::simulationStateChanged);
    connect(&m_simulationEngine, &SimulationEngine::simulationStateChanged,
            this, &ScenarioEngine::scheduledTakeoffsChanged);
    connect(&m_ollamaClient, &OllamaClient::responseReady, this,
            [this](const QString &response)
            {
                handleOllamaResponse(m_pendingAiPrompt, response);
            });
    connect(&m_ollamaClient, &OllamaClient::requestFailed, this,
            [this](const QString &message, bool unavailable)
            {
                handleOllamaFailure(m_pendingAiPrompt, message, unavailable);
            });
}

EntityModel *ScenarioEngine::entityModel() { return m_simulationEngine.entityModel(); }
AirbaseRegistry *ScenarioEngine::airbaseModel() { return &m_airbaseRegistry; }
QString ScenarioEngine::scenarioName() const { return m_scenario.name(); }
QString ScenarioEngine::region() const { return m_scenario.region(); }
QString ScenarioEngine::entityType() const { return m_entityType; }
int ScenarioEngine::entityCount() const { return static_cast<int>(m_scenario.entityCount()); }
QString ScenarioEngine::statusMessage() const { return m_statusMessage; }
bool ScenarioEngine::scenarioGenerated() const { return m_scenarioGenerated; }
double ScenarioEngine::centerLatitude() const { return m_centerLatitude; }
double ScenarioEngine::centerLongitude() const { return m_centerLongitude; }
const Scenario &ScenarioEngine::scenario() const { return m_scenario; }
QString ScenarioEngine::selectedEntityId() const { return m_selectedEntityId; }
QString ScenarioEngine::selectedEntityName() const { return m_selectedEntityName; }
QString ScenarioEngine::selectedAirbaseId() const { return m_selectedAirbaseId; }
QString ScenarioEngine::selectedAirbaseName() const
{
    const AirbaseRecord *record = m_airbaseRegistry.byId(m_selectedAirbaseId);
    return record ? record->name : QString();
}
QString ScenarioEngine::selectedAirbaseState() const
{
    const AirbaseRecord *record = m_airbaseRegistry.byId(m_selectedAirbaseId);
    return record ? record->stateOrUnionTerritory : QString();
}
QString ScenarioEngine::selectedAirbaseCategory() const
{
    const AirbaseRecord *record = m_airbaseRegistry.byId(m_selectedAirbaseId);
    return record ? record->category : QString();
}
QString ScenarioEngine::selectedAirbaseVerificationStatus() const
{
    const AirbaseRecord *record = m_airbaseRegistry.byId(m_selectedAirbaseId);
    return record ? record->verificationStatus : QString();
}
QString ScenarioEngine::selectedAirbaseRegion() const
{
    const AirbaseRecord *record = m_airbaseRegistry.byId(m_selectedAirbaseId);
    return record ? record->region : QString();
}
QString ScenarioEngine::airbaseDisplayRegion() const { return m_airbaseDisplayRegion; }
double ScenarioEngine::selectedAircraftLatitude() const
{
    return aircraftInformation(m_selectedEntityId).value(QStringLiteral("latitude")).toDouble();
}
double ScenarioEngine::selectedAircraftLongitude() const
{
    return aircraftInformation(m_selectedEntityId).value(QStringLiteral("longitude")).toDouble();
}
QString ScenarioEngine::selectedAircraftCallsign() const
{
    return aircraftInformation(m_selectedEntityId).value(QStringLiteral("callsign")).toString();
}
double ScenarioEngine::selectedAircraftAltitude() const
{
    return aircraftInformation(m_selectedEntityId).value(QStringLiteral("altitude")).toDouble();
}
double ScenarioEngine::selectedAircraftSpeed() const
{
    return aircraftInformation(m_selectedEntityId).value(QStringLiteral("speed")).toDouble();
}
double ScenarioEngine::selectedAircraftHeading() const
{
    return aircraftInformation(m_selectedEntityId).value(QStringLiteral("heading")).toDouble();
}
double ScenarioEngine::selectedAircraftVerticalSpeed() const
{
    return aircraftInformation(m_selectedEntityId).value(QStringLiteral("verticalSpeed")).toDouble();
}
QString ScenarioEngine::selectedAircraftCurrentWaypoint() const
{
    return aircraftInformation(m_selectedEntityId).value(QStringLiteral("currentWaypoint")).toString();
}
QString ScenarioEngine::selectedAircraftNextWaypoint() const
{
    return aircraftInformation(m_selectedEntityId).value(QStringLiteral("nextWaypoint")).toString();
}
QString ScenarioEngine::selectedAircraftStatus() const
{
    return aircraftInformation(m_selectedEntityId).value(QStringLiteral("status")).toString();
}
QString ScenarioEngine::selectedAircraftConflictStatus() const
{
    return aircraftInformation(m_selectedEntityId).value(QStringLiteral("conflictStatus")).toString();
}
QString ScenarioEngine::selectedAircraftMovementStatus() const
{
    return aircraftInformation(m_selectedEntityId)
        .value(QStringLiteral("movementStatus")).toString();
}
QString ScenarioEngine::selectedAircraftDestination() const
{
    return aircraftInformation(m_selectedEntityId)
        .value(QStringLiteral("destination")).toString();
}
bool ScenarioEngine::simulationRunning() const { return m_simulationEngine.running(); }
double ScenarioEngine::simulationSpeed() const { return m_simulationEngine.speedMultiplier(); }
double ScenarioEngine::simulationTime() const { return m_simulationEngine.simulatedSeconds(); }
double ScenarioEngine::arrivalToleranceMeters() const
{
    return m_simulationEngine.arrivalToleranceMeters();
}
double ScenarioEngine::conflictZoneRadiusMeters() const
{
    return m_simulationEngine.conflictZoneRadiusMeters();
}
bool ScenarioEngine::aiRequestPending() const { return m_aiRequestPending; }
int ScenarioEngine::commandHistoryCount() const { return m_commandHistory.size(); }
QVector<CommandHistoryEntry> ScenarioEngine::commandHistory() const
{
    return m_commandHistory;
}
QVariantList ScenarioEngine::routes() const
{
    QVariantList result;
    for (const ScenarioRoute &route : m_scenario.routes()) {
        QVariantList path;
        for (const Waypoint &waypoint : route.waypoints)
            path.push_back(QVariant::fromValue(
                QGeoCoordinate(waypoint.latitude, waypoint.longitude)));
        result.push_back(QVariantMap{{QStringLiteral("id"), route.id},
                                     {QStringLiteral("name"), route.name},
                                     {QStringLiteral("aircraftId"), route.aircraftId},
                                     {QStringLiteral("status"), route.status},
                                     {QStringLiteral("path"), path}});
    }
    return result;
}
QVariantList ScenarioEngine::routeWaypoints() const
{
    QVariantList result;
    for (const ScenarioRoute &route : m_scenario.routes()) {
        for (int index = 0; index < route.waypoints.size(); ++index) {
            const Waypoint &waypoint = route.waypoints.at(index);
            result.push_back(QVariantMap{
                {QStringLiteral("routeId"), route.id},
                {QStringLiteral("name"), waypoint.name},
                {QStringLiteral("label"), QString(QChar('A' + (index % 26)))},
                {QStringLiteral("latitude"), waypoint.latitude},
                {QStringLiteral("longitude"), waypoint.longitude},
                {QStringLiteral("isStart"), index == 0},
                {QStringLiteral("isEnd"), index == route.waypoints.size() - 1}});
        }
    }
    return result;
}

bool ScenarioEngine::playSimulation() { return m_simulationEngine.play(); }
void ScenarioEngine::pauseSimulation() { m_simulationEngine.pause(); }
void ScenarioEngine::resetSimulation() { m_simulationEngine.reset(); }
void ScenarioEngine::setSimulationSpeed(double multiplier)
{
    m_simulationEngine.setSpeedMultiplier(multiplier);
}

bool ScenarioEngine::advanceSimulationForTest(double simulatedSeconds)
{
    return m_simulationEngine.advanceSimulationForTest(simulatedSeconds);
}

QVariantMap ScenarioEngine::aircraftInformation(const QString &entityId) const
{
    AircraftSimulationState state;
    if (entityId.isEmpty() || !m_simulationEngine.aircraftState(entityId, &state))
        return {};

    const int entityIndex = m_simulationEngine.entityModel()->rowForId(entityId);
    if (entityIndex < 0 || entityIndex >= static_cast<int>(m_scenario.entities().size()))
        return {};
    const Entity &entity = m_scenario.entities().at(static_cast<std::size_t>(entityIndex));

    QString currentWaypoint = QStringLiteral("HOLD");
    QString nextWaypoint = QStringLiteral("HOLD");
    if (state.currentWaypoint > 0 && state.currentWaypoint < entity.route().size())
        currentWaypoint = entity.route().at(state.currentWaypoint - 1).name;
    else if (!entity.route().isEmpty())
        currentWaypoint = entity.route().last().name;
    if (state.currentWaypoint >= 0 && state.currentWaypoint < entity.route().size())
        nextWaypoint = entity.route().at(state.currentWaypoint).name;

    return {
        {QStringLiteral("id"), state.id},
        {QStringLiteral("callsign"), state.callsign},
        {QStringLiteral("latitude"), state.latitude},
        {QStringLiteral("longitude"), state.longitude},
        {QStringLiteral("altitude"), state.altitude},
        {QStringLiteral("speed"), state.speed},
        {QStringLiteral("heading"), state.heading},
        {QStringLiteral("verticalSpeed"), state.verticalSpeed},
        {QStringLiteral("currentWaypoint"), currentWaypoint},
        {QStringLiteral("nextWaypoint"), nextWaypoint},
        {QStringLiteral("status"), state.status},
        {QStringLiteral("conflictStatus"), state.conflictStatus},
        {QStringLiteral("aircraftType"), state.aircraftType},
        {QStringLiteral("movementStatus"), state.movementStatus},
        {QStringLiteral("destination"), entity.route().isEmpty()
                                           ? QString()
                                           : entity.route().last().name}};
}

bool ScenarioEngine::selectAircraft(const QString &entityId)
{
    for (const Entity &entity : m_scenario.entities())
    {
        if (entity.id() != entityId || entity.type() != QStringLiteral("aircraft"))
            continue;

        if (m_selectedEntityId != entity.id())
        {
            m_selectedEntityId = entity.id();
            AircraftSimulationState state;
            m_selectedEntityName = m_simulationEngine.aircraftState(entity.id(), &state)
                                       ? state.callsign
                                       : entity.name();
            emit selectedEntityChanged();
            emit simulationStateChanged();
        }
        return true;
    }
    return false;
}

bool ScenarioEngine::moveSelectedAircraftToDestination(const QString &destination)
{
    if (m_selectedEntityId.isEmpty())
    {
        setStatusMessage(QStringLiteral("SELECT AN AIRCRAFT BEFORE ASSIGNING A DESTINATION"));
        return false;
    }
    return generateScenario(QStringLiteral("Move %1 to %2")
                                .arg(m_selectedEntityId, destination));
}

bool ScenarioEngine::startSelectedAircraft()
{
    if (m_selectedEntityId.isEmpty())
    {
        setStatusMessage(QStringLiteral("SELECT AN AIRCRAFT BEFORE STARTING MOVEMENT"));
        return false;
    }
    return generateScenario(QStringLiteral("Start %1").arg(m_selectedEntityId));
}

bool ScenarioEngine::pauseSelectedAircraft()
{
    if (m_selectedEntityId.isEmpty())
    {
        setStatusMessage(QStringLiteral("SELECT AN AIRCRAFT BEFORE PAUSING MOVEMENT"));
        return false;
    }
    return generateScenario(QStringLiteral("Pause %1").arg(m_selectedEntityId));
}

bool ScenarioEngine::resumeSelectedAircraft()
{
    if (m_selectedEntityId.isEmpty())
    {
        setStatusMessage(QStringLiteral("SELECT AN AIRCRAFT BEFORE RESUMING MOVEMENT"));
        return false;
    }
    return generateScenario(QStringLiteral("Resume %1").arg(m_selectedEntityId));
}

bool ScenarioEngine::stopSelectedAircraft()
{
    if (m_selectedEntityId.isEmpty())
    {
        setStatusMessage(QStringLiteral("SELECT AN AIRCRAFT BEFORE STOPPING MOVEMENT"));
        return false;
    }
    return generateScenario(QStringLiteral("Stop %1").arg(m_selectedEntityId));
}

void ScenarioEngine::setArrivalToleranceMeters(double tolerance)
{
    m_simulationEngine.setArrivalToleranceMeters(tolerance);
}

bool ScenarioEngine::selectAirbase(const QString &airbaseId)
{
    const AirbaseRecord *record = m_airbaseRegistry.byId(airbaseId);
    if (!record || !record->coordinateAvailable ||
        record->verificationStatus != QStringLiteral("verified_public_reference"))
        return false;
    if (m_selectedAirbaseId == airbaseId)
        return true;
    m_selectedAirbaseId = airbaseId;
    emit selectedAirbaseChanged();
    return true;
}

void ScenarioEngine::clearSelectedAirbase()
{
    if (m_selectedAirbaseId.isEmpty())
        return;
    m_selectedAirbaseId.clear();
    emit selectedAirbaseChanged();
}

void ScenarioEngine::clearSelectedAircraft()
{
    if (m_selectedEntityId.isEmpty())
        return;
    m_selectedEntityId.clear();
    m_selectedEntityName.clear();
    emit selectedEntityChanged();
    emit simulationStateChanged();
}

bool ScenarioEngine::generateScenario(const QString &prompt)
{
    const int commandSequence =
        recordCommandHistory(prompt, QStringLiteral("processing"));
    return executeScenarioPrompt(prompt, commandSequence);
}

int ScenarioEngine::recordCommandHistory(const QString &prompt,
                                        const QString &processingStatus)
{
    if (prompt.trimmed().isEmpty())
        return 0;

    CommandHistoryEntry command;
    command.sequenceNumber = m_commandHistory.size() + 1;
    command.prompt = prompt;
    command.timestamp =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    command.processingStatus = processingStatus;
    m_commandHistory.append(command);
    emit commandHistoryChanged();
    qInfo().noquote() << "[COMMAND HISTORY] Captured sequence="
                      << command.sequenceNumber;
    return command.sequenceNumber;
}

void ScenarioEngine::updateCommandHistory(int sequenceNumber,
                                         const QString &processingStatus,
                                         const QString &parsedOperation)
{
    if (sequenceNumber <= 0)
        return;
    for (CommandHistoryEntry &command : m_commandHistory) {
        if (command.sequenceNumber != sequenceNumber)
            continue;
        command.processingStatus = processingStatus;
        if (!parsedOperation.isEmpty())
            command.parsedOperation = parsedOperation;
        return;
    }
}

bool ScenarioEngine::executeScenarioPrompt(const QString &prompt,
                                           int commandSequence)
{
    const auto execute = [this, &prompt, commandSequence]() -> bool
    {
    ScenarioRequest request;
    QString errorMessage;
    if (!m_parser.parsePrompt(prompt, request, &errorMessage, m_selectedAirbaseId))
    {
        setStatusMessage(errorMessage);
        return false;
    }
    updateCommandHistory(commandSequence, QStringLiteral("processing"),
                         operationName(request.operation));

    if (request.operation == ScenarioOperation::CreateRoute ||
        request.operation == ScenarioOperation::DisplayRoute ||
        request.operation == ScenarioOperation::AssignRoute ||
        request.operation == ScenarioOperation::MoveAlongRoute)
    {
        QVector<Waypoint> waypoints;
        waypoints.reserve(request.routeWaypoints.size());
        for (const QString &location : request.routeWaypoints) {
            QGeoCoordinate coordinate;
            if (!resolveNaturalRouteLocation(m_airbaseRegistry, location,
                                              coordinate)) {
                setStatusMessage(QStringLiteral("ROUTE LOCATION CANNOT BE RESOLVED: %1")
                                     .arg(location));
                return false;
            }
            if (!waypoints.isEmpty() &&
                qFuzzyCompare(waypoints.last().latitude, coordinate.latitude()) &&
                qFuzzyCompare(waypoints.last().longitude, coordinate.longitude())) {
                setStatusMessage(QStringLiteral("ROUTE CONTAINS DUPLICATE CONSECUTIVE WAYPOINTS"));
                return false;
            }
            waypoints.push_back({displayDestinationName(location),
                                 coordinate.latitude(), coordinate.longitude(), 0.0});
        }
        if (waypoints.size() < 2) {
            setStatusMessage(QStringLiteral("ROUTE REQUIRES AT LEAST TWO WAYPOINTS"));
            return false;
        }

        QString assignedAircraftId;
        if (request.operation == ScenarioOperation::AssignRoute ||
            request.operation == ScenarioOperation::MoveAlongRoute) {
            for (const Entity &entity : m_scenario.entities())
                if (entity.type() == QStringLiteral("aircraft") &&
                    (entity.id().compare(request.routeAircraftId, Qt::CaseInsensitive) == 0 ||
                     entity.callsign().compare(request.routeAircraftId, Qt::CaseInsensitive) == 0)) {
                    assignedAircraftId = entity.id();
                    break;
                }
            if (assignedAircraftId.isEmpty()) {
                setStatusMessage(QStringLiteral("AIRCRAFT %1 WAS NOT FOUND")
                                     .arg(request.routeAircraftId));
                return false;
            }
            AircraftSimulationState state;
            if (!m_simulationEngine.aircraftState(assignedAircraftId, &state)) {
                setStatusMessage(QStringLiteral("AIRCRAFT %1 HAS NO RUNTIME STATE")
                                     .arg(assignedAircraftId));
                return false;
            }
            QVector<Waypoint> assigned = waypoints;
            assigned[0] = {QStringLiteral("CURRENT POSITION"), state.latitude,
                           state.longitude, state.altitude};
            QString movementError;
            if (!m_simulationEngine.assignIndividualRoute(assignedAircraftId, assigned,
                                                           &movementError)) {
                setStatusMessage(movementError);
                return false;
            }
            for (Entity &entity : m_scenario.entities())
                if (entity.id() == assignedAircraftId) {
                    entity.setRoute(assigned, false);
                    break;
                }
            waypoints = assigned;
        }

        ScenarioRoute route;
        route.id = QStringLiteral("ROUTE%1")
                       .arg(m_scenario.routes().size() + 1, 2, 10, QLatin1Char('0'));
        route.name = request.routeName.isEmpty()
                         ? request.routeWaypoints.join(QStringLiteral(" to "))
                         : request.routeName;
        route.waypoints = waypoints;
        route.aircraftId = assignedAircraftId;
        route.status = request.operation == ScenarioOperation::AssignRoute
                           ? QStringLiteral("ASSIGNED")
                           : request.operation == ScenarioOperation::MoveAlongRoute
                                 ? QStringLiteral("MOVING")
                                 : QStringLiteral("DISPLAYED");
        m_scenario.addRoute(std::move(route));
        emit routesChanged();
        emit scenarioChanged();
        setStatusMessage(QStringLiteral("%1 ROUTE %2 (%3 WAYPOINTS)")
                             .arg(operationName(request.operation),
                                  request.routeName,
                                  QString::number(request.routeWaypoints.size())));
        return true;
    }

    if (request.operation == ScenarioOperation::RemoveAircraft ||
        request.operation == ScenarioOperation::RemoveLastAircraft ||
        request.operation == ScenarioOperation::RemoveAllAircraft)
    {
        QStringList removedIds;
        const auto resolveAircraftId = [this](const QString &reference)
        {
            const QRegularExpression numberPattern(QStringLiteral("(\\d+)$"));
            const QRegularExpressionMatch referenceNumber = numberPattern.match(reference);
            for (const Entity &entity : m_scenario.entities())
                if (entity.type() == QStringLiteral("aircraft") &&
                    (entity.id().compare(reference, Qt::CaseInsensitive) == 0 ||
                     entity.callsign().compare(reference, Qt::CaseInsensitive) == 0))
                    return entity.id();
            if (referenceNumber.hasMatch())
                for (const Entity &entity : m_scenario.entities())
                {
                    if (entity.type() != QStringLiteral("aircraft"))
                        continue;
                    const QRegularExpressionMatch idNumber = numberPattern.match(entity.id());
                    const QRegularExpressionMatch callsignNumber =
                        numberPattern.match(entity.callsign());
                    if ((idNumber.hasMatch() &&
                         referenceNumber.captured(1).toInt() ==
                             idNumber.captured(1).toInt()) ||
                        (callsignNumber.hasMatch() &&
                         referenceNumber.captured(1).toInt() ==
                             callsignNumber.captured(1).toInt()))
                        return entity.id();
                }
            return QString();
        };

        if (request.operation == ScenarioOperation::RemoveAircraft)
        {
            const QString resolvedId = resolveAircraftId(request.operationTargetId);
            if (resolvedId.isEmpty())
            {
                setStatusMessage(QStringLiteral("AIRCRAFT %1 WAS NOT FOUND")
                                     .arg(request.operationTargetId));
                return false;
            }
            removedIds.push_back(resolvedId);
        }
        else
        {
            for (auto entity = m_scenario.entities().rbegin();
                 entity != m_scenario.entities().rend(); ++entity)
                if (entity->type() == QStringLiteral("aircraft") &&
                    (request.operation != ScenarioOperation::RemoveLastAircraft ||
                     removedIds.size() < request.entityCount))
                    removedIds.push_back(entity->id());
        }

        if (removedIds.isEmpty())
        {
            setScenarioGenerated(!m_scenario.entities().empty());
            setStatusMessage(QStringLiteral("NO AIRCRAFT TO REMOVE"));
            return true;
        }

        QSet<QString> affectedFormations;
        for (const Entity &entity : m_scenario.entities())
            if (removedIds.contains(entity.id()) && entity.isFormationMember())
                affectedFormations.insert(entity.formationId());
        for (const QString &formationId : affectedFormations)
            m_simulationEngine.disbandFormation(formationId);

        for (const QString &id : removedIds)
            m_scenario.removeEntity(id);
        for (Entity &entity : m_scenario.entities())
            if (entity.isFormationMember() &&
                affectedFormations.contains(entity.formationId()))
                entity.clearFormation();

        auto &scheduledTakeoffs = m_scenario.scheduledTakeoffs();
        scheduledTakeoffs.erase(
            std::remove_if(scheduledTakeoffs.begin(), scheduledTakeoffs.end(),
                           [&removedIds](const ScheduledTakeoff &event)
                           { return removedIds.contains(event.aircraftId); }),
            scheduledTakeoffs.end());
        const bool hasAircraft = std::any_of(
            m_scenario.entities().cbegin(), m_scenario.entities().cend(),
            [](const Entity &entity)
            { return entity.type() == QStringLiteral("aircraft"); });
        m_simulationEngine.setScenario(m_scenario.entities(), hasAircraft);
        m_simulationEngine.setScheduledTakeoffs(scheduledTakeoffs);

        QStringList remainingTypes;
        for (const Entity &entity : m_scenario.entities())
        {
            const QString type = displayType(entity.type());
            if (!remainingTypes.contains(type))
                remainingTypes.push_back(type);
        }
        m_entityType = formatEntitySummary(remainingTypes);
        setScenarioGenerated(!m_scenario.entities().empty());
        if (removedIds.contains(m_selectedEntityId))
            clearSelectedAircraft();
        emit scenarioChanged();
        setStatusMessage(QStringLiteral("REMOVED %1 AIRCRAFT")
                             .arg(removedIds.size()));
        return true;
    }

    if (request.operation == ScenarioOperation::MoveFormation ||
        request.operation == ScenarioOperation::StopFormation ||
        request.operation == ScenarioOperation::ResumeFormation ||
        request.operation == ScenarioOperation::DisbandFormation)
    {
        struct FormationSelection
        {
            QString id;
            QString leaderId;
        };
        QVector<FormationSelection> formations;
        for (const Entity &entity : m_scenario.entities())
        {
            if (!entity.isFormationMember() ||
                entity.id() != entity.formationLeaderId() ||
                (request.formationTypeSpecified &&
                 entity.formationType().compare(request.formationType,
                                                Qt::CaseInsensitive) != 0))
                continue;
            formations.push_back({entity.formationId(), entity.id()});
        }
        if (formations.isEmpty())
        {
            setStatusMessage(QStringLiteral("NO MATCHING FORMATION WAS FOUND"));
            return false;
        }
        if (!request.allFormations && formations.size() != 1)
        {
            setStatusMessage(QStringLiteral(
                                 "FORMATION COMMAND IS AMBIGUOUS; SPECIFY A FORMATION TYPE OR ALL FORMATIONS"));
            return false;
        }

        if (request.operation == ScenarioOperation::MoveFormation)
        {
            QStringList movedLeaderIds;
            for (const FormationSelection &formation : formations)
            {
                if (!generateScenario(QStringLiteral("Move %1 to %2")
                                          .arg(formation.leaderId,
                                               request.operationLocation)))
                    return false;
                movedLeaderIds.push_back(formation.leaderId);
                if (!request.allFormations)
                    break;
            }
            setStatusMessage(QStringLiteral("MOVED %1 FORMATION%2 TOWARDS %3")
                                 .arg(movedLeaderIds.size())
                                 .arg(movedLeaderIds.size() == 1 ? QString() : QStringLiteral("S"))
                                 .arg(request.operationLocation));
            return true;
        }

        QStringList controlledFormationIds;
        QString errorMessage;
        for (const FormationSelection &formation : formations)
        {
            bool succeeded = false;
            if (request.operation == ScenarioOperation::StopFormation)
                succeeded = m_simulationEngine.stopFormation(formation.id, &errorMessage);
            else if (request.operation == ScenarioOperation::ResumeFormation)
                succeeded = m_simulationEngine.resumeFormation(formation.id, &errorMessage);
            else
                succeeded = m_simulationEngine.disbandFormation(formation.id);
            if (!succeeded)
            {
                setStatusMessage(errorMessage.isEmpty()
                                     ? QStringLiteral("FORMATION OPERATION FAILED")
                                     : errorMessage);
                return false;
            }
            controlledFormationIds.push_back(formation.id);
        }
        if (request.operation == ScenarioOperation::DisbandFormation)
        {
            for (Entity &entity : m_scenario.entities())
                if (entity.isFormationMember() &&
                    controlledFormationIds.contains(entity.formationId()))
                    entity.clearFormation();
            m_simulationEngine.setScenario(m_scenario.entities(), true);
            emit scenarioChanged();
        }
        const QString action =
            request.operation == ScenarioOperation::StopFormation
                ? QStringLiteral("STOPPED")
                : request.operation == ScenarioOperation::ResumeFormation
                      ? QStringLiteral("RESUMED")
                      : QStringLiteral("DISBANDED");
        setStatusMessage(QStringLiteral("%1 %2 FORMATION%3")
                             .arg(action)
                             .arg(controlledFormationIds.size())
                             .arg(controlledFormationIds.size() == 1
                                      ? QString()
                                      : QStringLiteral("S")));
        return true;
    }

    if (request.operation == ScenarioOperation::StopAllAircraft)
    {
        const bool hasAircraft = std::any_of(
            m_scenario.entities().cbegin(), m_scenario.entities().cend(),
            [](const Entity &entity)
            { return entity.type() == QStringLiteral("aircraft"); });
        if (!hasAircraft)
        {
            setStatusMessage(QStringLiteral("NO AIRCRAFT TO STOP"));
            return true;
        }
        m_simulationEngine.stopAllAircraft();
        setStatusMessage(QStringLiteral("STOPPED ALL AIRCRAFT"));
        return true;
    }

    if (request.operation == ScenarioOperation::Query)
    {
        setStatusMessage(QStringLiteral("SCENARIO HAS %1 ENTITIES").arg(entityCount()));
        return true;
    }
    if (request.operation == ScenarioOperation::ListScheduledTakeoffs)
    {
        setStatusMessage(QStringLiteral("%1 SCHEDULED TAKE-OFFS")
                             .arg(m_simulationEngine.scheduledTakeoffs().size()));
        return true;
    }
    if (request.operation == ScenarioOperation::ScheduleTakeoff)
    {
        const auto sameAircraftReference = [](const QString &reference, const Entity &entity)
        {
            if (entity.id().compare(reference, Qt::CaseInsensitive) == 0 ||
                entity.callsign().compare(reference, Qt::CaseInsensitive) == 0)
                return true;
            const QRegularExpression numberPattern(QStringLiteral("(\\d+)$"));
            const QRegularExpressionMatch referenceMatch = numberPattern.match(reference);
            const QRegularExpressionMatch idMatch = numberPattern.match(entity.id());
            const QRegularExpressionMatch callsignMatch = numberPattern.match(entity.callsign());
            return referenceMatch.hasMatch() &&
                   ((idMatch.hasMatch() && referenceMatch.captured(1).toInt() == idMatch.captured(1).toInt()) ||
                    (callsignMatch.hasMatch() && referenceMatch.captured(1).toInt() == callsignMatch.captured(1).toInt()));
        };
        QString resolvedAircraftId;
        for (const Entity &entity : m_scenario.entities())
        {
            if (sameAircraftReference(request.operationTargetId, entity))
            {
                resolvedAircraftId = entity.id();
                break;
            }
        }
        if (resolvedAircraftId.isEmpty())
        {
            setStatusMessage(QStringLiteral("AIRCRAFT %1 WAS NOT FOUND")
                                 .arg(request.operationTargetId));
            return false;
        }

        const Entity *targetEntity = nullptr;
        for (const Entity &entity : m_scenario.entities())
            if (entity.id() == resolvedAircraftId)
                targetEntity = &entity;
        if (!targetEntity || targetEntity->type() != QStringLiteral("aircraft"))
        {
            setStatusMessage(QStringLiteral("TARGET IS NOT AN AIRCRAFT"));
            return false;
        }

        ScheduledTakeoff event;
        event.aircraftId = resolvedAircraftId;
        event.scheduledSeconds = request.scheduledSeconds;
        event.originAirbaseName = request.originAirbaseName;
        if (!request.originAirbaseName.isEmpty())
        {
            const QVector<const AirbaseRecord *> matches =
                m_airbaseRegistry.matchingText(request.originAirbaseName);
            if (matches.size() != 1 || !matches.first()->coordinateAvailable ||
                matches.first()->verificationStatus != QStringLiteral("verified_public_reference"))
            {
                setStatusMessage(QStringLiteral("ORIGIN AIRBASE IS UNKNOWN OR AMBIGUOUS"));
                return false;
            }
            event.originAirbaseId = matches.first()->id;
            const QGeoCoordinate origin = matches.first()->coordinate;
            const QGeoCoordinate current(targetEntity->latitude(), targetEntity->longitude());
            if (origin.distanceTo(current) > 5.0 * 1852.0)
            {
                setStatusMessage(QStringLiteral("AIRCRAFT IS NOT ASSIGNED TO THE REQUESTED ORIGIN"));
                return false;
            }
        }

        Scenario previousScenario = m_scenario;
        if (!request.destinationName.isEmpty())
        {
            QGeoCoordinate destination;
            const QVector<const AirbaseRecord *> airbaseMatches =
                m_airbaseRegistry.matchingText(request.destinationName);
            if (airbaseMatches.size() == 1 && airbaseMatches.first()->coordinateAvailable)
                destination = airbaseMatches.first()->coordinate;
            else if (request.region != QStringLiteral("Unknown Region") && !request.region.isEmpty())
            {
                double latitude = 0.0;
                double longitude = 0.0;
                regionCenter(request.region, latitude, longitude);
                destination = QGeoCoordinate(latitude, longitude);
            }
            if (!destination.isValid())
            {
                setStatusMessage(QStringLiteral("DESTINATION IS UNKNOWN OR AMBIGUOUS"));
                return false;
            }
            event.destinationName = request.destinationName;
            QVector<Waypoint> route;
            route.push_back({QStringLiteral("DEPARTURE"), targetEntity->latitude(),
                             targetEntity->longitude(), targetEntity->altitude()});
            route.push_back({request.destinationName, destination.latitude(),
                             destination.longitude(), targetEntity->altitude()});
            double routeCenterLatitude = 0.0;
            double routeCenterLongitude = 0.0;
            regionCenter(m_scenario.region(), routeCenterLatitude, routeCenterLongitude);
            if (!validateRoute(route, boundaryForRegion(m_scenario.region()),
                               routeCenterLatitude, routeCenterLongitude, 100000.0))
            {
                setStatusMessage(QStringLiteral("DESTINATION ROUTE IS OUTSIDE SCENARIO BOUNDS"));
                return false;
            }
            for (Entity &entity : m_scenario.entities())
                if (entity.id() == resolvedAircraftId)
                    entity.setRoute(std::move(route), false);
        }

        m_simulationEngine.setScenario(m_scenario.entities());
        QString scheduleError;
        if (!m_simulationEngine.scheduleTakeoff(event, &scheduleError))
        {
            m_scenario = std::move(previousScenario);
            m_simulationEngine.setScenario(m_scenario.entities());
            setStatusMessage(scheduleError);
            return false;
        }
        m_scenario.addScheduledTakeoff(event);
        emit scheduledTakeoffsChanged();
        emit scenarioChanged();
        setStatusMessage(QStringLiteral("TAKE-OFF SCHEDULED: %1 AT %2 FROM %3 TO %4")
                             .arg(resolvedAircraftId,
                                  QStringLiteral("%1:%2")
                                      .arg(request.scheduledSeconds / 3600, 2, 10, QLatin1Char('0'))
                                      .arg((request.scheduledSeconds / 60) % 60, 2, 10, QLatin1Char('0')),
                                  event.originAirbaseName.isEmpty() ? QStringLiteral("CURRENT POSITION")
                                                                    : event.originAirbaseName,
                                  event.destinationName.isEmpty() ? QStringLiteral("EXISTING ROUTE")
                                                                  : event.destinationName));
        return true;
    }
    if (request.operation == ScenarioOperation::CancelScheduledTakeoff)
    {
        QString resolvedAircraftId = request.operationTargetId;
        const QRegularExpression numberPattern(QStringLiteral("(\\d+)$"));
        const QRegularExpressionMatch referenceMatch = numberPattern.match(request.operationTargetId);
        for (const Entity &entity : m_scenario.entities())
        {
            const QRegularExpressionMatch callsignMatch = numberPattern.match(entity.callsign());
            if (entity.callsign().compare(request.operationTargetId, Qt::CaseInsensitive) == 0 ||
                (referenceMatch.hasMatch() && callsignMatch.hasMatch() &&
                 referenceMatch.captured(1).toInt() == callsignMatch.captured(1).toInt()))
                resolvedAircraftId = entity.id();
        }
        QString cancelError;
        if (!m_simulationEngine.cancelScheduledTakeoff(resolvedAircraftId, &cancelError))
        {
            setStatusMessage(cancelError);
            return false;
        }
        for (ScheduledTakeoff &event : m_scenario.scheduledTakeoffs())
            if (event.aircraftId == resolvedAircraftId)
                event.cancelled = true;
        emit scheduledTakeoffsChanged();
        emit scenarioChanged();
        setStatusMessage(QStringLiteral("TAKE-OFF CANCELLED FOR %1")
                             .arg(request.operationTargetId));
        return true;
    }
    if (request.operation == ScenarioOperation::Save)
        return saveScenario();
    if (request.operation == ScenarioOperation::Load)
    {
        setStatusMessage(QStringLiteral("SPECIFY A JSON FILE TO LOAD"));
        return false;
    }
    if (request.operation == ScenarioOperation::Reset)
    {
        if (!request.operationTargetId.isEmpty())
        {
            if (!m_scenario.removeEntity(request.operationTargetId))
            {
                setStatusMessage(QStringLiteral("ENTITY %1 WAS NOT FOUND")
                                     .arg(request.operationTargetId));
                return false;
            }
            auto &scheduledTakeoffs = m_scenario.scheduledTakeoffs();
            scheduledTakeoffs.erase(
                std::remove_if(scheduledTakeoffs.begin(), scheduledTakeoffs.end(),
                               [&request](const ScheduledTakeoff &event)
                               { return event.aircraftId == request.operationTargetId; }),
                scheduledTakeoffs.end());
            m_simulationEngine.setScenario(m_scenario.entities());
            m_simulationEngine.setScheduledTakeoffs(scheduledTakeoffs);
            emit scenarioChanged();
            setStatusMessage(QStringLiteral("REMOVED %1").arg(request.operationTargetId));
            return true;
        }
        m_scenario.clearEntities();
        m_simulationEngine.setScenario(m_scenario.entities());
        m_simulationEngine.setScheduledTakeoffs({});
        setScenarioGenerated(false);
        clearSelectedAircraft();
        emit scenarioChanged();
        setStatusMessage(QStringLiteral("SCENARIO RESET"));
        return true;
    }
    if (request.operation == ScenarioOperation::Move ||
        request.operation == ScenarioOperation::ChangeRoute ||
        request.operation == ScenarioOperation::StopAircraft ||
        request.operation == ScenarioOperation::PauseAircraft ||
        request.operation == ScenarioOperation::ResumeAircraft ||
        request.operation == ScenarioOperation::StartAircraft)
    {
        int entityIndex = -1;
        const QRegularExpression numberPattern(QStringLiteral("(\\d+)$"));
        const QRegularExpressionMatch referenceNumber =
            numberPattern.match(request.operationTargetId);
        for (int index = 0; index < static_cast<int>(m_scenario.entities().size()); ++index)
        {
            const Entity &entity = m_scenario.entities().at(static_cast<std::size_t>(index));
            if (entity.type() != QStringLiteral("aircraft"))
                continue;
            if (entity.id().compare(request.operationTargetId, Qt::CaseInsensitive) == 0 ||
                entity.callsign().compare(request.operationTargetId, Qt::CaseInsensitive) == 0)
            {
                entityIndex = index;
                break;
            }
        }
        if (entityIndex < 0 && referenceNumber.hasMatch())
        {
            for (int index = 0; index < static_cast<int>(m_scenario.entities().size()); ++index)
            {
                const Entity &entity = m_scenario.entities().at(static_cast<std::size_t>(index));
                const QRegularExpressionMatch idNumber = numberPattern.match(entity.id());
                const QRegularExpressionMatch callsignNumber = numberPattern.match(entity.callsign());
                if (entity.type() == QStringLiteral("aircraft") &&
                    ((idNumber.hasMatch() &&
                      referenceNumber.captured(1).toInt() == idNumber.captured(1).toInt()) ||
                     (callsignNumber.hasMatch() &&
                      referenceNumber.captured(1).toInt() == callsignNumber.captured(1).toInt())))
                {
                    entityIndex = index;
                    break;
                }
            }
        }
        if (entityIndex < 0)
        {
            setStatusMessage(QStringLiteral("AIRCRAFT %1 WAS NOT FOUND")
                                 .arg(request.operationTargetId));
            return false;
        }

        Entity &entity = m_scenario.entities().at(static_cast<std::size_t>(entityIndex));
        AircraftSimulationState currentState;
        if (!m_simulationEngine.aircraftState(entity.id(), &currentState))
        {
            setStatusMessage(QStringLiteral("AIRCRAFT %1 HAS NO RUNTIME STATE").arg(entity.id()));
            return false;
        }
        QString movementError;
        bool succeeded = false;
        switch (request.operation)
        {
        case ScenarioOperation::StopAircraft:
            succeeded = m_simulationEngine.stopAircraft(entity.id(), &movementError);
            break;
        case ScenarioOperation::PauseAircraft:
            succeeded = m_simulationEngine.pauseAircraft(entity.id(), &movementError);
            break;
        case ScenarioOperation::ResumeAircraft:
            succeeded = m_simulationEngine.resumeAircraft(entity.id(), &movementError);
            break;
        case ScenarioOperation::StartAircraft:
            succeeded = m_simulationEngine.startAircraft(entity.id(), &movementError);
            break;
        case ScenarioOperation::Move:
        case ScenarioOperation::ChangeRoute:
        {
            QString destinationQuery = request.operationLocation.trimmed();
            destinationQuery.remove(QRegularExpression(QStringLiteral("[.!?]+$")));
            const QString destinationName = displayDestinationName(destinationQuery);
            QVector<const AirbaseRecord *> matches =
                m_airbaseRegistry.matchingText(destinationQuery);
            const AirbaseRecord *destinationAirbase = nullptr;
            if (matches.size() > 1)
            {
                setStatusMessage(QStringLiteral("DESTINATION IS AMBIGUOUS: %1")
                                     .arg(request.operationLocation));
                return false;
            }
            if (matches.size() == 1)
            {
                destinationAirbase = matches.first();
                if (!destinationAirbase->coordinateAvailable ||
                    destinationAirbase->verificationStatus !=
                        QStringLiteral("verified_public_reference"))
                {
                    setStatusMessage(QStringLiteral("DESTINATION AIRBASE IS NOT VERIFIED: %1")
                                         .arg(request.operationLocation));
                    return false;
                }
            }
            if (!destinationAirbase)
            {
                QString normalizedQuery = destinationQuery.toLower();
                normalizedQuery.remove(QRegularExpression(
                    QStringLiteral("\\s+(?:airbase|air force base|airport)$")));
                normalizedQuery = normalizedQuery.trimmed();
                QVector<const AirbaseRecord *> stateMatches;
                for (const AirbaseRecord &record : m_airbaseRegistry.records())
                {
                    if (record.coordinateAvailable &&
                        record.verificationStatus ==
                            QStringLiteral("verified_public_reference") &&
                        record.stateOrUnionTerritory.compare(normalizedQuery,
                                                             Qt::CaseInsensitive) == 0)
                        stateMatches.push_back(&record);
                }
                if (stateMatches.size() == 1)
                    destinationAirbase = stateMatches.first();
                else if (stateMatches.size() > 1)
                {
                    setStatusMessage(QStringLiteral("DESTINATION IS AMBIGUOUS: %1")
                                         .arg(request.operationLocation));
                    return false;
                }
            }

            QGeoCoordinate destination;
            if (destinationAirbase)
                destination = destinationAirbase->coordinate;
            else
            {
                const QStringList broadRegions = {
                    QStringLiteral("India"), QStringLiteral("North India"),
                    QStringLiteral("South India"), QStringLiteral("East India"),
                    QStringLiteral("West India")};
                if (broadRegions.contains(request.region, Qt::CaseInsensitive))
                {
                    setStatusMessage(QStringLiteral("DESTINATION MUST BE A SPECIFIC VERIFIED AIRBASE OR CITY"));
                    return false;
                }

                QString normalizedQuery = destinationQuery.toLower();
                normalizedQuery.remove(QRegularExpression(
                    QStringLiteral("\\s+(?:airbase|air force base|airport)$")));
                normalizedQuery = normalizedQuery.trimmed();
                QString canonicalCity = request.region;
                if (normalizedQuery == QStringLiteral("bombay"))
                    canonicalCity = QStringLiteral("Mumbai");
                else if (normalizedQuery == QStringLiteral("bangalore"))
                    canonicalCity = QStringLiteral("Bengaluru");
                if (canonicalCity.compare(normalizedQuery, Qt::CaseInsensitive) != 0 &&
                    normalizedQuery != QStringLiteral("bombay") &&
                    normalizedQuery != QStringLiteral("bangalore"))
                {
                    setStatusMessage(QStringLiteral("DESTINATION CANNOT BE RESOLVED: %1")
                                         .arg(request.operationLocation));
                    return false;
                }
                double latitude = 0.0;
                double longitude = 0.0;
                regionCenter(canonicalCity, latitude, longitude);
                destination = QGeoCoordinate(latitude, longitude);
            }

            if (!destination.isValid() || !std::isfinite(destination.latitude()) ||
                !std::isfinite(destination.longitude()))
            {
                setStatusMessage(QStringLiteral("DESTINATION COORDINATES ARE INVALID"));
                return false;
            }

            const QGeoCoordinate origin(currentState.latitude, currentState.longitude);
            const double distanceMeters = origin.distanceTo(destination);
            const double initialBearing = origin.azimuthTo(destination);
            if (!std::isfinite(distanceMeters) || distanceMeters < 0.0 ||
                !std::isfinite(initialBearing))
            {
                setStatusMessage(QStringLiteral("COULD NOT PLAN A VALID ROUTE TO %1")
                                     .arg(request.operationLocation));
                return false;
            }
            constexpr double WaypointSpacingMeters = 100000.0;
            const int segmentCount = qMax(
                1, static_cast<int>(qCeil(distanceMeters / WaypointSpacingMeters)));
            if (segmentCount > 1000)
            {
                setStatusMessage(QStringLiteral("DESTINATION IS TOO FAR TO PLAN A SAFE ROUTE"));
                return false;
            }
            QVector<Waypoint> proposedRoute;
            proposedRoute.reserve(segmentCount + 1);
            proposedRoute.push_back({QStringLiteral("CURRENT POSITION"),
                                     currentState.latitude, currentState.longitude,
                                     currentState.altitude});
            for (int segment = 1; segment < segmentCount; ++segment)
            {
                const QGeoCoordinate waypointCoordinate =
                    origin.atDistanceAndAzimuth(
                        distanceMeters * segment / segmentCount, initialBearing);
                if (!waypointCoordinate.isValid())
                {
                    setStatusMessage(QStringLiteral("PLANNED ROUTE CONTAINS AN INVALID WAYPOINT"));
                    return false;
                }
                proposedRoute.push_back({
                    QStringLiteral("ENROUTE %1").arg(segment),
                    waypointCoordinate.latitude(), waypointCoordinate.longitude(),
                    currentState.altitude});
            }
            proposedRoute.push_back({destinationName,
                                     destination.latitude(), destination.longitude(),
                                     currentState.altitude});

            const QVector<Waypoint> oldRoute = entity.route();
            entity.setRoute(proposedRoute, false);
            if (!m_simulationEngine.assignIndividualRoute(entity.id(), proposedRoute,
                                                          &movementError))
            {
                entity.setRoute(oldRoute, false);
                setStatusMessage(movementError);
                return false;
            }
            emit scenarioChanged();
            setStatusMessage(QStringLiteral("AIRCRAFT %1 MOVING TO %2")
                                 .arg(entity.id(), destinationName));
            return true;
        }
        default:
            break;
        }

        if (!succeeded)
        {
            setStatusMessage(movementError);
            return false;
        }
        const QString actionStatus =
            request.operation == ScenarioOperation::StopAircraft
                ? QStringLiteral("STOPPED")
                : request.operation == ScenarioOperation::PauseAircraft
                      ? QStringLiteral("PAUSED")
                      : request.operation == ScenarioOperation::StartAircraft
                            ? QStringLiteral("STARTED")
                            : QStringLiteral("RESUMED");
        setStatusMessage(QStringLiteral("AIRCRAFT %1 %2").arg(entity.id(), actionStatus));
        return true;
    }

    if (request.airbaseDisplayOnly)
    {
        m_airbaseDisplayRegion = request.airbaseRegion;
        setStatusMessage(QStringLiteral("AIRBASES SHOWN IN %1").arg(request.airbaseRegion));
        emit scenarioChanged();
        return true;
    }

    m_airbaseDisplayRegion.clear();

    const QString scenarioRegion = request.airbaseRegion.isEmpty() ? request.region : request.airbaseRegion;
    if (!request.explicitOperation && m_scenarioGenerated && !m_scenario.entities().empty())
    {
        setStatusMessage(QStringLiteral("DO YOU MEAN ADDITIONAL ENTITIES OR %1 ENTITIES IN TOTAL?")
                             .arg(request.entityCount));
        return false;
    }
    const bool additive =
        (request.operation == ScenarioOperation::Add ||
         request.operation == ScenarioOperation::CreateFormation) &&
        m_scenarioGenerated;
    Scenario generated = additive
                             ? m_scenario
                             : Scenario(scenarioNameForRegion(scenarioRegion), prompt.trimmed(), scenarioRegion);
    double scenarioCenterLatitude = 0.0;
    double scenarioCenterLongitude = 0.0;
    regionCenter(scenarioRegion, scenarioCenterLatitude, scenarioCenterLongitude);
    if (!request.airbaseId.isEmpty())
    {
        const AirbaseRecord *record = m_airbaseRegistry.byId(request.airbaseId);
        if (!record)
        {
            setStatusMessage(QStringLiteral("THE REQUESTED AIRBASE IS NOT CONFIGURED"));
            return false;
        }
        scenarioCenterLatitude = record->coordinate.latitude();
        scenarioCenterLongitude = record->coordinate.longitude();
    }

    QVector<const AirbaseRecord *> airbaseTargets;
    if (request.airbaseRegionTarget)
        airbaseTargets = m_airbaseRegistry.matchingRegion(request.airbaseRegion);
    else if (!request.airbaseId.isEmpty())
        airbaseTargets.push_back(m_airbaseRegistry.byId(request.airbaseId));
    if ((request.airbaseRegionTarget || !request.airbaseId.isEmpty()) && airbaseTargets.isEmpty())
    {
        setStatusMessage(QStringLiteral("NO CONFIGURED AIRBASES MATCH THE REQUEST"));
        return false;
    }
    QVector<EntityRequest> generationRequests;
    if (airbaseTargets.isEmpty())
        generationRequests = request.entities;
    else
    {
        for (const AirbaseRecord *unused : airbaseTargets)
        {
            Q_UNUSED(unused)
            generationRequests += request.entities;
        }
    }
    int totalAircraft = 0;
    int totalEntities = 0;
    for (const EntityRequest &entityRequest : generationRequests)
    {
        if (entityRequest.count <= 0 || entityRequest.count > MaximumScenarioEntityCount)
        {
            setStatusMessage(QStringLiteral("SCENARIO ENTITY COUNT MUST BE BETWEEN 1 AND %1")
                                 .arg(MaximumScenarioEntityCount));
            return false;
        }
        if (totalEntities > MaximumScenarioEntityCount - entityRequest.count)
        {
            setStatusMessage(QStringLiteral("SCENARIO ENTITY COUNT MUST NOT EXCEED %1")
                                 .arg(MaximumScenarioEntityCount));
            return false;
        }
        totalEntities += entityRequest.count;
        if (entityRequest.type == QStringLiteral("aircraft"))
            totalAircraft += entityRequest.count;
    }
    if (totalEntities <= 0 || generated.entityCount() > MaximumScenarioEntityCount - totalEntities)
    {
        setStatusMessage(QStringLiteral("SCENARIO MUST CONTAIN AT LEAST ONE ENTITY"));
        return false;
    }
    if (request.routeDestinations.size() > totalAircraft)
    {
        setStatusMessage(QStringLiteral("ROUTE COUNT EXCEEDS THE SCENARIO AIRCRAFT COUNT"));
        return false;
    }
    QVector<QGeoCoordinate> routeDestinations;
    for (const QString &destinationName : request.routeDestinations)
    {
        QGeoCoordinate destination;
        if (!resolveVerifiedRouteDestination(m_airbaseRegistry, destinationName,
                                             destination))
        {
            setStatusMessage(QStringLiteral("ROUTE DESTINATION IS NOT A UNIQUE VERIFIED LOCATION: %1")
                                 .arg(destinationName));
            return false;
        }
        routeDestinations.push_back(destination);
    }
    int aircraftIndex = 0;
    for (const Entity &existing : generated.entities())
        if (existing.type() == QStringLiteral("aircraft"))
            ++aircraftIndex;
    const auto nextUniqueEntityId = [&generated](const QString &prefix)
    {
        int number = 1;
        while (true)
        {
            const QString candidate =
                prefix + QStringLiteral("%1").arg(number, 2, 10, QLatin1Char('0'));
            const bool exists = std::any_of(
                generated.entities().cbegin(), generated.entities().cend(),
                [&candidate](const Entity &entity)
                { return entity.id().compare(candidate, Qt::CaseInsensitive) == 0; });
            if (!exists)
                return candidate;
            ++number;
        }
    };
    const auto nextUniqueCallsign = [&generated]()
    {
        int number = 1;
        while (true)
        {
            const QString candidate =
                QStringLiteral("AERO%1").arg(number, 3, 10, QLatin1Char('0'));
            const bool exists = std::any_of(
                generated.entities().cbegin(), generated.entities().cend(),
                [&candidate](const Entity &entity)
                { return entity.callsign().compare(candidate, Qt::CaseInsensitive) == 0; });
            if (!exists)
                return candidate;
            ++number;
        }
    };
    QStringList summaryTypes;
    const QStringList aircraftTypes = {
        QStringLiteral("A320"), QStringLiteral("B737"), QStringLiteral("A220"),
        QStringLiteral("E190"), QStringLiteral("C172")};
    int placementOffset = 0;
    const auto *regionalBoundary = boundaryForRegion(scenarioRegion);
    const double configuredRadiusNm = request.hasRadius
                                          ? request.radiusKm * 1000.0 / 1.852
                                          : 0.0;
    const double placementRadiusNm = request.hasRadius
                                         ? configuredRadiusNm
                                         : (regionalBoundary ? 100000.0 : 18.0);
    const double validationRadiusNm = request.hasRadius
                                          ? configuredRadiusNm
                                          : 100000.0;
    int routedAircraftCount = 0;
    QStringList routedAircraftIds;
    const bool createFormation =
        request.operation == ScenarioOperation::CreateFormation;
    QString formationId;
    QString formationLeaderId;
    QVector<QGeoCoordinate> entityPositions;
    QVector<QGeoCoordinate> entityOrigins;
    QVector<FormationSlot> formationSlots;
    double formationHeading = request.formationHeading;
    if (createFormation)
    {
        formationLeaderId = nextUniqueEntityId(entityIdPrefix(QStringLiteral("aircraft")));
        const QGeoCoordinate reference(scenarioCenterLatitude,
                                       scenarioCenterLongitude);
        QString formationError;
        if (!FormationGenerator::generate(
                request.entityCount, request.formationType, reference,
                formationHeading, request.formationSpacing,
                &formationSlots, &formationError))
        {
            setStatusMessage(formationError);
            return false;
        }
        formationId = formationSlots.first().formationId;
        for (const FormationSlot &slot : formationSlots)
        {
            entityPositions.push_back(slot.coordinate);
            entityOrigins.push_back(reference);
        }
    }
    else if (!airbaseTargets.isEmpty())
    {
        constexpr double AirbaseSpawnRadiusNm = 0.5;
        for (const AirbaseRecord *record : airbaseTargets)
        {
            for (const EntityRequest &entityRequest : request.entities)
            {
                for (int index = 0; index < entityRequest.count; ++index)
                {
                    entityPositions.push_back(aircraftAnchor(index, entityRequest.count,
                                                             record->coordinate.latitude(),
                                                             record->coordinate.longitude(),
                                                             AirbaseSpawnRadiusNm));
                    entityOrigins.push_back(record->coordinate);
                }
            }
        }
    }
    else
    {
        entityPositions = generatePositions(totalEntities, regionalBoundary,
                                            scenarioCenterLatitude, scenarioCenterLongitude,
                                            placementRadiusNm);
        entityOrigins.fill(QGeoCoordinate(scenarioCenterLatitude, scenarioCenterLongitude),
                           entityPositions.size());
    }
    if (entityPositions.size() != totalEntities)
    {
        setStatusMessage(QStringLiteral("COULD NOT PLACE ALL ENTITIES WITHIN SCENARIO BOUNDS"));
        return false;
    }
    logAircraftDistribution(scenarioRegion, entityPositions);

    for (const EntityRequest &entityRequest : generationRequests)
    {
        const QString type = entityRequest.type;
        const QString entityLabel = displayType(type);
        for (int index = 0; index < entityRequest.count; ++index)
        {
            const int placementIndex = placementOffset + index;
            if (placementIndex < 0 || placementIndex >= entityPositions.size())
            {
                setStatusMessage(QStringLiteral("INSUFFICIENT VALID ENTITY POSITIONS"));
                return false;
            }
            const QGeoCoordinate &placement = entityPositions.at(placementIndex);
            const QGeoCoordinate &placementOrigin = entityOrigins.at(placementIndex);
            if (!coordinateWithinScenario(placement, regionalBoundary,
                                          scenarioCenterLatitude,
                                          scenarioCenterLongitude,
                                          validationRadiusNm))
            {
                setStatusMessage(QStringLiteral("INVALID ENTITY POSITION FOR SCENARIO REGION"));
                return false;
            }
            const QString prefix = entityIdPrefix(type);
            const QString entityId = nextUniqueEntityId(prefix);
            const QString suffix = entityId.mid(prefix.size());

            if (type == QStringLiteral("aircraft"))
            {
                bool loopRoute = false;
                QVector<Waypoint> route;
                if (routedAircraftCount < request.routeDestinations.size())
                {
                    const QString &destinationName =
                        request.routeDestinations.at(routedAircraftCount);
                    if (createFormation)
                    {
                        const QGeoCoordinate origin(placement.latitude(),
                                                    placement.longitude());
                        const QGeoCoordinate destination =
                            routeDestinations.at(routedAircraftCount);
                        const double distanceMeters = origin.distanceTo(destination);
                        const double bearing = origin.azimuthTo(destination);
                        const int segments = qMax(
                            1, static_cast<int>(qCeil(distanceMeters / 100000.0)));
                        route.push_back(waypoint(QStringLiteral("FORMATION LEADER"),
                                                 placement,
                                                 entityRequest.altitude));
                        for (int segment = 1; segment < segments; ++segment)
                        {
                            const QGeoCoordinate point = origin.atDistanceAndAzimuth(
                                distanceMeters * segment / segments, bearing);
                            route.push_back(waypoint(
                                QStringLiteral("ENROUTE %1").arg(segment), point,
                                entityRequest.altitude));
                        }
                        route.push_back(waypoint(
                            destinationName, destination, entityRequest.altitude));
                    }
                    else
                    {
                        route << waypoint(request.locationName, placement,
                                         entityRequest.altitude)
                              << waypoint(destinationName,
                                          routeDestinations.at(routedAircraftCount),
                                          entityRequest.altitude);
                    }
                    routedAircraftIds.push_back(entityId);
                    ++routedAircraftCount;
                }
                else if (createFormation)
                {
                    route << waypoint(QStringLiteral("FORMATION SLOT"), placement,
                                      entityRequest.altitude);
                }
                else
                {
                    route = makeRoute(
                        request.routePattern, aircraftIndex, totalAircraft,
                        placementOrigin.latitude(), placementOrigin.longitude(),
                        entityRequest.altitude, entityRequest.heading,
                        placement, &loopRoute);
                }
                if (!createFormation &&
                    !validateRoute(route, regionalBoundary,
                                   placementOrigin.latitude(), placementOrigin.longitude(),
                                   validationRadiusNm))
                {
                    setStatusMessage(QStringLiteral("COULD NOT KEEP AIRCRAFT ROUTE WITHIN SCENARIO BOUNDS"));
                    return false;
                }
                const Waypoint &start = route.first();
                const Waypoint &next = route.size() > 1 ? route.at(1) : route.first();
                const double initialHeading = createFormation
                                                   ? formationHeading
                                                   : bearingTo(start.latitude,
                                                               start.longitude,
                                                               next.latitude,
                                                               next.longitude);
                const QString callsign = nextUniqueCallsign();
                const QString aircraftType =
                    aircraftTypes.at(aircraftIndex % aircraftTypes.size());
                Entity aircraft(entityId,
                                callsign,
                                type,
                                placement.latitude(),
                                placement.longitude(),
                                entityRequest.altitude,
                                entityRequest.speed,
                                initialHeading,
                                QStringLiteral("ACTIVE"),
                                callsign,
                                0.0,
                                aircraftType,
                                std::move(route),
                                loopRoute);
                if (createFormation)
                {
                    const FormationSlot &slot = formationSlots.at(index);
                    aircraft.setFormation(
                        formationId, request.formationType, formationLeaderId, slot.slot,
                        slot.offset, request.formationSpacing,
                        formationHeading,
                        QStringLiteral("ACTIVE"));
                }
                generated.addEntity(std::move(aircraft));
                ++aircraftIndex;
                continue;
            }

            generated.addEntity(Entity(entityId,
                                       entityLabel + QLatin1Char(' ') + suffix,
                                       type,
                                       placement.latitude(),
                                       placement.longitude(),
                                       entityRequest.altitude,
                                       entityRequest.speed,
                                       entityRequest.heading,
                                       QStringLiteral("ACTIVE")));
        }
        placementOffset += entityRequest.count;
        summaryTypes << entityLabel;
    }

    QSet<QString> committedIds;
    QSet<QString> committedCalls;
    for (const Entity &entity : generated.entities())
    {
        const QString id = entity.id().trimmed();
        const QString callsign = entity.callsign().trimmed();
        if (id.isEmpty() || committedIds.contains(id))
        {
            setStatusMessage(QStringLiteral("GENERATION FAILED: DUPLICATE OR EMPTY ENTITY ID"));
            return false;
        }
        if (!callsign.isEmpty() && committedCalls.contains(callsign))
        {
            setStatusMessage(QStringLiteral("GENERATION FAILED: DUPLICATE AIRCRAFT CALLSIGN"));
            return false;
        }
        committedIds.insert(id);
        if (!callsign.isEmpty())
            committedCalls.insert(callsign);
        for (const Waypoint &waypoint : entity.route())
        {
            if (!QGeoCoordinate(waypoint.latitude, waypoint.longitude).isValid())
            {
                setStatusMessage(QStringLiteral("GENERATION FAILED: INVALID ROUTE COORDINATE"));
                return false;
            }
        }
    }

    m_scenario = std::move(generated);
    if (additive && !scenarioRegion.isEmpty() && scenarioRegion != QStringLiteral("Unknown Region"))
        m_scenario.setRegion(scenarioRegion);
    if (!request.airbaseId.isEmpty())
        selectAirbase(request.airbaseId);
    clearSelectedAircraft();
    m_entityType = formatEntitySummary(summaryTypes);
    regionCenter(request.region, m_centerLatitude, m_centerLongitude);
    m_simulationEngine.setScenario(m_scenario.entities(), additive);
    if (!additive)
        m_simulationEngine.setScheduledTakeoffs(m_scenario.scheduledTakeoffs());
    setScenarioGenerated(true);
    emit scenarioChanged();
    if (createFormation)
        setStatusMessage(QStringLiteral("GENERATED %1-AIRCRAFT %2 FORMATION; LEADER %3")
                             .arg(request.entityCount)
                             .arg(request.formationType, formationLeaderId));
    else if (routedAircraftIds.isEmpty())
        setStatusMessage(QStringLiteral("GENERATED"));
    else if (routedAircraftIds.size() == 1)
        setStatusMessage(QStringLiteral("GENERATED: %1 AIRCRAFT; %2 ROUTED TO %3")
                             .arg(request.entityCount)
                             .arg(routedAircraftIds.first(),
                                  request.routeDestinations.first().toUpper()));
    else
    {
        QStringList routeSummaries;
        for (int index = 0; index < routedAircraftIds.size(); ++index)
            routeSummaries << QStringLiteral("%1 TO %2")
                                  .arg(routedAircraftIds.at(index),
                                       request.routeDestinations.at(index).toUpper());
        setStatusMessage(QStringLiteral("GENERATED: %1 AIRCRAFT; ROUTES: %2")
                             .arg(request.entityCount)
                             .arg(routeSummaries.join(QStringLiteral("; "))));
    }
    return true;
    };

    const bool succeeded = execute();
    updateCommandHistory(commandSequence,
                         succeeded ? QStringLiteral("succeeded")
                                   : QStringLiteral("failed"));
    return succeeded;
}

bool ScenarioEngine::generateScenarioWithAI(const QString &prompt)
{
    qDebug().noquote() << "[SCENARIO AI] submit; entityCountBefore=" << entityCount();
    m_aiRetryCount = 0;
    const int commandSequence =
        recordCommandHistory(prompt, QStringLiteral("submitted"));
    if (prompt.trimmed().isEmpty())
    {
        setStatusMessage(QStringLiteral("PLEASE ENTER A SCENARIO"));
        return false;
    }
    ScenarioRequest parsedRequest;
    QString parseError;
    if (m_parser.parsePrompt(prompt, parsedRequest, &parseError,
                             m_selectedAirbaseId))
        updateCommandHistory(commandSequence, QStringLiteral("submitted"),
                             operationName(parsedRequest.operation));
    if (m_aiRequestPending)
    {
        updateCommandHistory(commandSequence, QStringLiteral("failed"));
        setStatusMessage(QStringLiteral("AN AI COMMAND IS ALREADY IN PROGRESS"));
        return false;
    }
    m_pendingCommandSequence = commandSequence;
    updateCommandHistory(commandSequence, QStringLiteral("pending"));
    m_pendingAiPrompt = prompt;
    setAiRequestPending(true);
    setStatusMessage(QStringLiteral("ASKING LOCAL OLLAMA..."));
    m_ollamaClient.generate(
        QStringLiteral(
            "Output one flat JSON object, not nested. Its top-level property must be "
            "\"operation\"; never use an operation name as a property. No markdown, "
            "explanation, or extra fields. Extract values only from the user command. "
            "Ordinary create/add aircraft: operation ADD_ENTITIES, integer count, "
            "entity_type aircraft, region equal to the stated location. Include "
            "route_destinations only if explicitly requested. Only choose "
            "CREATE_FORMATION when the command explicitly says formation; include "
            "integer count, entity_type aircraft, and formation_type (V if unspecified). "
            "For a single named aircraft movement, choose MOVE_ENTITY and copy its "
            "entity_id exactly. For formation movement, choose MOVE_FORMATION. Both "
            "movement operations require destination. Never change the operation, "
            "count, formation type, location, or aircraft ID. Do not add a route or "
            "destination unless requested. Aircraft removal uses REMOVE_AIRCRAFT "
            "with entity_id, REMOVE_LAST_AIRCRAFT with integer count, or "
            "REMOVE_ALL_AIRCRAFT with no entity_id. Stop all aircraft uses "
            "STOP_ALL_AIRCRAFT with no entity_id. Formation controls are "
            "STOP_FORMATION, RESUME_FORMATION, and DISBAND_FORMATION; preserve an "
            "explicit formation_type or all_formations selection. MOVE_FORMATION "
            "must include formation_type when the user names a type, and must set "
            "all_formations to true when the user says all formations. Copy the "
            "named type and destination only from the user command; never reuse "
            "a destination from another instruction. Do not invent selectors for "
            "an unqualified singular formation command. Other operations are "
            "SET_ROUTE, STOP_AIRCRAFT, PAUSE_AIRCRAFT, RESUME_AIRCRAFT, "
            "START_AIRCRAFT, SCHEDULE_TAKEOFF, CANCEL_EVENT, SHOW_AIRBASES, "
            "QUERY_SCENARIO. For a route request use CREATE_ROUTE or DISPLAY_ROUTE "
            "with route_name and an ordered waypoints array of location names. "
            "Use ASSIGN_ROUTE or MOVE_ALONG_ROUTE with aircraft_id and the same "
            "ordered waypoints when the user explicitly names an aircraft. "
            "Do not invent coordinates; locations are resolved offline. "
            "User command: [%1]")
            .arg(prompt));
    return true;
}

void ScenarioEngine::handleOllamaResponse(const QString &prompt, const QString &response)
{
    const int commandSequence = m_pendingCommandSequence;
    qWarning().noquote() << "[SCENARIO AI] raw Ollama response:"
                         << response;
    qDebug().noquote() << "[SCENARIO AI] response received; entityCount="
                       << entityCount();
    const auto rejectOrRetry = [this, prompt, commandSequence](const QString &rejection)
    {
        if (m_aiRetryCount < 1)
        {
            ++m_aiRetryCount;
            setAiRequestPending(true);
            setStatusMessage(QStringLiteral("OLLAMA RESPONSE INVALID - REQUESTING ONE CORRECTED RESPONSE"));
            updateCommandHistory(commandSequence, QStringLiteral("pending"));
            m_ollamaClient.generate(
                QStringLiteral(
                    "Correct the previous JSON response. Return exactly one flat JSON object "
                    "and no markdown or explanation. Preserve the original user intent exactly: "
                    "operation, entity count, location, and route destinations must match. "
                    "The original command is [%1]. The exact validation failure is [%2]. "
                    "For an ordinary create/add aircraft command use ADD_ENTITIES.")
                    .arg(prompt, rejection));
            return;
        }
        setAiRequestPending(false);
        updateCommandHistory(commandSequence, QStringLiteral("failed"));
        setStatusMessage(QStringLiteral(
                             "MODEL RESPONSE REJECTED: %1; DETERMINISTIC FALLBACK NOT USED")
                             .arg(rejection));
        m_pendingAiPrompt.clear();
        m_pendingCommandSequence = 0;
    };
    const auto rejectModelResponse = [this, commandSequence](const QString &rejection)
    {
        setAiRequestPending(false);
        updateCommandHistory(commandSequence, QStringLiteral("failed"));
        setStatusMessage(QStringLiteral(
                             "MODEL RESPONSE REJECTED: %1; DETERMINISTIC FALLBACK NOT USED")
                             .arg(rejection));
        m_pendingAiPrompt.clear();
        m_pendingCommandSequence = 0;
    };

    ScenarioRequest request;
    QString errorMessage;
    if (!AICommand::fromResponse(response, request, &errorMessage))
    {
        qWarning().noquote() << "[SCENARIO AI] command validation failed:" << errorMessage;
        rejectOrRetry(errorMessage.isEmpty()
                          ? QStringLiteral("OLLAMA RETURNED A MALFORMED COMMAND")
                          : errorMessage);
        return;
    }
    QString canonicalPrompt;
    if (!AICommand::toPrompt(request, &canonicalPrompt, &errorMessage))
    {
        rejectModelResponse(errorMessage);
        return;
    }
    updateCommandHistory(commandSequence, QStringLiteral("processing"),
                         operationName(request.operation));

    ScenarioRequest originalRequest;
    QString originalError;
    const bool originalParsed =
        m_parser.parsePrompt(prompt, originalRequest, &originalError, m_selectedAirbaseId);
    if (originalParsed &&
        (originalRequest.operation == ScenarioOperation::MoveFormation ||
         originalRequest.operation == ScenarioOperation::StopFormation ||
         originalRequest.operation == ScenarioOperation::ResumeFormation ||
         originalRequest.operation == ScenarioOperation::DisbandFormation))
    {
        const bool requestIsSameFormationOperation =
            (originalRequest.operation == ScenarioOperation::MoveFormation &&
             request.operation == ScenarioOperation::MoveFormation) ||
            (originalRequest.operation == ScenarioOperation::StopFormation &&
             request.operation == ScenarioOperation::StopFormation) ||
            (originalRequest.operation == ScenarioOperation::ResumeFormation &&
             request.operation == ScenarioOperation::ResumeFormation) ||
            (originalRequest.operation == ScenarioOperation::DisbandFormation &&
             request.operation == ScenarioOperation::DisbandFormation);
        if (requestIsSameFormationOperation)
        {
            if (originalRequest.formationTypeSpecified &&
                !request.formationTypeSpecified)
            {
                request.formationTypeSpecified = true;
                request.formationType = originalRequest.formationType;
            }
            if (originalRequest.allFormations)
                request.allFormations = true;
        }
    }
    if (originalParsed &&
        (originalRequest.operation == ScenarioOperation::Add ||
         originalRequest.operation == ScenarioOperation::Replace))
    {
        ScenarioRequest modelIntent;
        QString modelError;
        const bool modelIntentParsed =
            m_parser.parsePrompt(canonicalPrompt, modelIntent, &modelError, m_selectedAirbaseId);
        const bool operationMatches =
            modelIntentParsed &&
            (modelIntent.operation == originalRequest.operation ||
             (modelIntent.operation == ScenarioOperation::Add &&
              originalRequest.operation == ScenarioOperation::Replace));
        const bool matchesOriginal =
            operationMatches &&
            modelIntent.entityCount == originalRequest.entityCount &&
            modelIntent.entityType == originalRequest.entityType &&
            modelIntent.region == originalRequest.region &&
            modelIntent.locationName == originalRequest.locationName &&
            modelIntent.airbaseId == originalRequest.airbaseId &&
            modelIntent.routeDestinations == originalRequest.routeDestinations;
        if (!matchesOriginal)
        {
            rejectOrRetry(QStringLiteral(
                "MODEL COMMAND DOES NOT MATCH THE REQUESTED OPERATION, COUNT, LOCATION, OR ROUTE"));
            return;
        }
    }
    if (originalParsed &&
        originalRequest.operation == ScenarioOperation::CreateFormation)
    {
        if (request.operation != ScenarioOperation::CreateFormation ||
            request.entityCount != originalRequest.entityCount ||
            request.formationType != originalRequest.formationType)
        {
            rejectOrRetry(
                QStringLiteral("MODEL FORMATION DOES NOT MATCH THE REQUESTED COUNT OR TYPE"));
            return;
        }
    }
    if (originalParsed &&
        (originalRequest.operation == ScenarioOperation::CreateRoute ||
         originalRequest.operation == ScenarioOperation::DisplayRoute ||
         originalRequest.operation == ScenarioOperation::AssignRoute ||
         originalRequest.operation == ScenarioOperation::MoveAlongRoute))
    {
        const bool sameRoute =
            request.operation == originalRequest.operation &&
            request.routeWaypoints == originalRequest.routeWaypoints &&
            request.routeAircraftId.compare(originalRequest.routeAircraftId,
                                             Qt::CaseInsensitive) == 0;
        if (!sameRoute) {
            rejectOrRetry(
                QStringLiteral("MODEL ROUTE DOES NOT MATCH THE REQUESTED WAYPOINTS OR AIRCRAFT"));
            return;
        }
    }
    if (originalParsed &&
        (originalRequest.operation == ScenarioOperation::RemoveAircraft ||
         originalRequest.operation == ScenarioOperation::RemoveLastAircraft ||
         originalRequest.operation == ScenarioOperation::RemoveAllAircraft ||
         originalRequest.operation == ScenarioOperation::StopAllAircraft ||
         originalRequest.operation == ScenarioOperation::StopFormation ||
         originalRequest.operation == ScenarioOperation::ResumeFormation ||
         originalRequest.operation == ScenarioOperation::DisbandFormation))
    {
        bool matchesOriginal = request.operation == originalRequest.operation;
        if (matchesOriginal &&
            originalRequest.operation == ScenarioOperation::RemoveAircraft)
            matchesOriginal = request.operationTargetId.compare(
                                  originalRequest.operationTargetId,
                                  Qt::CaseInsensitive) == 0;
        if (matchesOriginal &&
            originalRequest.operation == ScenarioOperation::RemoveLastAircraft)
            matchesOriginal = request.entityCount == originalRequest.entityCount;
        if (matchesOriginal &&
            (originalRequest.operation == ScenarioOperation::StopFormation ||
             originalRequest.operation == ScenarioOperation::ResumeFormation ||
             originalRequest.operation == ScenarioOperation::DisbandFormation))
            matchesOriginal =
                request.allFormations == originalRequest.allFormations &&
                request.formationTypeSpecified ==
                    originalRequest.formationTypeSpecified &&
                (!originalRequest.formationTypeSpecified ||
                 request.formationType == originalRequest.formationType);
        if (!matchesOriginal)
        {
            rejectOrRetry(
                QStringLiteral("MODEL COMMAND DOES NOT MATCH THE REQUESTED AIRCRAFT OR FORMATION ACTION"));
            return;
        }
    }
    const auto isIndividualAircraftOperation = [](ScenarioOperation operation)
    {
        return operation == ScenarioOperation::Move ||
               operation == ScenarioOperation::ChangeRoute ||
               operation == ScenarioOperation::AssignRoute ||
               operation == ScenarioOperation::MoveAlongRoute ||
               operation == ScenarioOperation::StopAircraft ||
               operation == ScenarioOperation::PauseAircraft ||
               operation == ScenarioOperation::ResumeAircraft ||
               operation == ScenarioOperation::StartAircraft;
    };
    if (isIndividualAircraftOperation(request.operation) ||
        (originalParsed && isIndividualAircraftOperation(originalRequest.operation)))
    {
        const auto sameDestination = [this](const QString &first, const QString &second)
        {
            if (first.compare(second, Qt::CaseInsensitive) == 0)
                return true;
            const QVector<const AirbaseRecord *> firstMatches =
                m_airbaseRegistry.matchingText(first);
            const QVector<const AirbaseRecord *> secondMatches =
                m_airbaseRegistry.matchingText(second);
            return firstMatches.size() == 1 && secondMatches.size() == 1 &&
                   firstMatches.first()->id == secondMatches.first()->id;
        };
        const bool matchesOriginal =
            originalParsed &&
            request.operation == originalRequest.operation &&
            request.operationTargetId.compare(originalRequest.operationTargetId,
                                               Qt::CaseInsensitive) == 0 &&
            sameDestination(request.operationLocation, originalRequest.operationLocation) &&
            request.allFormations == originalRequest.allFormations &&
            request.formationTypeSpecified ==
                originalRequest.formationTypeSpecified &&
            (!originalRequest.formationTypeSpecified ||
             request.formationType == originalRequest.formationType);
        if (!matchesOriginal)
        {
            rejectOrRetry(
                QStringLiteral("MODEL COMMAND DOES NOT MATCH THE REQUESTED AIRCRAFT ACTION"));
            return;
        }
    }

    setAiRequestPending(false);
    qDebug().noquote() << "[SCENARIO AI] validated Ollama command; operation="
                       << static_cast<int>(request.operation)
                       << "executing through scenario engine";
    QString executionPrompt = canonicalPrompt;
    if (originalParsed &&
        originalRequest.operation == ScenarioOperation::Replace &&
        request.operation == ScenarioOperation::Add)
        executionPrompt.replace(0, QStringLiteral("Add").size(),
                                QStringLiteral("Replace"));

    if (!executeScenarioPrompt(executionPrompt, commandSequence))
    {
        qWarning().noquote() << "[SCENARIO AI] authoritative execution failed:"
                             << statusMessage();
        setStatusMessage(QStringLiteral(
                             "AI COMMAND REJECTED; DETERMINISTIC FALLBACK NOT USED: %1")
                             .arg(statusMessage()));
        m_pendingAiPrompt.clear();
        m_pendingCommandSequence = 0;
        Q_UNUSED(prompt)
        return;
    }
    setStatusMessage(QStringLiteral("OLLAMA GENERATED JSON; COMMAND EXECUTED: %1")
                         .arg(statusMessage()));
    m_pendingAiPrompt.clear();
    m_pendingCommandSequence = 0;
    qDebug().noquote() << "[SCENARIO AI] execution succeeded; entityCountAfter="
                       << entityCount();
    Q_UNUSED(prompt)
}

void ScenarioEngine::handleOllamaFailure(const QString &prompt, const QString &message,
                                         bool unavailable)
{
    const int commandSequence = m_pendingCommandSequence;
    qWarning().noquote() << "[SCENARIO AI] request failed; unavailable=" << unavailable
                         << "message=" << message;
    setAiRequestPending(false);
    if (!unavailable)
    {
        updateCommandHistory(commandSequence, QStringLiteral("failed"));
        setStatusMessage(QStringLiteral(
                             "OLLAMA FAILED; DETERMINISTIC FALLBACK NOT USED: %1")
                             .arg(message));
        m_pendingAiPrompt.clear();
        m_pendingCommandSequence = 0;
        return;
    }
    if (qEnvironmentVariableIntValue(
            "AEROSIM_DISABLE_DETERMINISTIC_FALLBACK") == 1)
    {
        updateCommandHistory(commandSequence, QStringLiteral("failed"));
        setStatusMessage(QStringLiteral(
                             "LOCAL OLLAMA UNAVAILABLE; DETERMINISTIC FALLBACK DISABLED: %1")
                             .arg(message));
        m_pendingAiPrompt.clear();
        m_pendingCommandSequence = 0;
        return;
    }
    const bool executed = executeScenarioPrompt(prompt, commandSequence);
    qDebug().noquote() << "[SCENARIO AI] fallback result=" << executed
                       << "entityCount=" << entityCount();
    const QString fallbackStatus =
        executed ? QStringLiteral("LOCAL AI UNAVAILABLE - DETERMINISTIC PARSER EXECUTED")
                 : QStringLiteral("LOCAL AI UNAVAILABLE - DETERMINISTIC PARSER REJECTED: %1")
                       .arg(statusMessage());
    setStatusMessage(fallbackStatus);
    m_pendingAiPrompt.clear();
    m_pendingCommandSequence = 0;
}

bool ScenarioEngine::saveScenario(const QString &filePath)
{
    QString savedPath;
    QString errorMessage;
    Scenario snapshot = snapshotForPersistence();
    if (!filePath.isEmpty())
    {
        ScenarioRepository repository(QFileInfo(filePath).absolutePath());
        if (!repository.saveScenario(snapshot.description(), snapshot,
                                     &savedPath, &errorMessage))
        {
            setStatusMessage(QStringLiteral("SAVE FAILED: %1").arg(errorMessage));
            return false;
        }
    }
    else if (!m_scenarioRepository.saveScenario(snapshot.description(), snapshot,
                                                &savedPath, &errorMessage))
    {
        setStatusMessage(QStringLiteral("SAVE FAILED: %1").arg(errorMessage));
        return false;
    }
    setStatusMessage(QStringLiteral("SCENARIO SAVED"));
    return true;
}

bool ScenarioEngine::exportScenarioGeoJson(const QString &filePath)
{
    const QVector<CommandHistoryEntry> commandHistorySnapshot = m_commandHistory;
    int aircraftCount = 0;
    int routeCount = 0;
    for (const Entity &entity : m_scenario.entities()) {
        if (entity.type() != QStringLiteral("aircraft"))
            continue;
        ++aircraftCount;
        if (entity.route().size() >= 2)
            ++routeCount;
    }
    const int airbaseCount = static_cast<int>(std::count_if(
        m_airbaseRegistry.records().cbegin(), m_airbaseRegistry.records().cend(),
        [](const AirbaseRecord &airbase) { return airbase.coordinateAvailable; }));
    qInfo().noquote() << "[GEOJSON EXPORT] ScenarioEngine received path="
                      << filePath << "aircraft=" << aircraftCount
                      << "airbases=" << airbaseCount << "routes=" << routeCount
                      << "commands=" << commandHistorySnapshot.size();

    if (filePath.trimmed().isEmpty())
    {
        qWarning().noquote() << "[GEOJSON EXPORT] No file path provided";
        setStatusMessage(QStringLiteral("GEOJSON EXPORT FAILED: No file path provided"));
        return false;
    }

    Scenario exportScenario = m_scenario;
    for (Entity &entity : exportScenario.entities())
    {
        if (entity.type() != QStringLiteral("aircraft"))
            continue;

        AircraftSimulationState currentState;
        if (!m_simulationEngine.aircraftState(entity.id(), &currentState))
        {
            const QString message =
                QStringLiteral("GEOJSON EXPORT FAILED: Aircraft %1 has no current simulation state")
                    .arg(entity.id());
            qWarning().noquote() << "[GEOJSON EXPORT]" << message;
            setStatusMessage(message);
            return false;
        }
        entity.setPosition(currentState.latitude, currentState.longitude,
                           currentState.altitude);
    }

    GeoJsonExporter::ExportSummary summary;
    QString errorMessage;
    if (!GeoJsonExporter::writeFile(
            exportScenario, m_airbaseRegistry.records(), commandHistorySnapshot,
            filePath, &summary, &errorMessage))
    {
        qWarning().noquote() << "GeoJSON export failed:"
                             << errorMessage << "requestedPath=" << filePath;
        setStatusMessage(
            QStringLiteral("GEOJSON EXPORT FAILED: %1").arg(errorMessage));
        return false;
    }

    const QString successMessage =
        QStringLiteral("GEOJSON EXPORT SUCCESS\nPath: %1\nFeatures: %2\nSize: %3 bytes")
            .arg(summary.absolutePath)
            .arg(summary.featureCount)
            .arg(summary.fileSizeBytes);
    qInfo().noquote() << successMessage;
    setStatusMessage(successMessage);
    return true;
}

bool ScenarioEngine::exportScenarioCsv(const QString &filePath)
{
    const QVector<CommandHistoryEntry> commandHistorySnapshot = m_commandHistory;
    int aircraftCount = 0;
    int routeCount = 0;
    for (const Entity &entity : m_scenario.entities()) {
        if (entity.type() != QStringLiteral("aircraft"))
            continue;
        ++aircraftCount;
        if (entity.route().size() >= 2)
            ++routeCount;
    }
    const int airbaseCount = static_cast<int>(std::count_if(
        m_airbaseRegistry.records().cbegin(), m_airbaseRegistry.records().cend(),
        [](const AirbaseRecord &airbase) { return airbase.coordinateAvailable; }));
    qInfo().noquote() << "[CSV EXPORT] ScenarioEngine received path="
                      << filePath << "aircraft=" << aircraftCount
                      << "airbases=" << airbaseCount << "routes=" << routeCount
                      << "commands=" << commandHistorySnapshot.size();

    QHash<QString, AircraftSimulationState> aircraftStates;
    for (const Entity &entity : m_scenario.entities()) {
        if (entity.type() != QStringLiteral("aircraft"))
            continue;

        AircraftSimulationState currentState;
        if (!m_simulationEngine.aircraftState(entity.id(), &currentState)) {
            const QString message =
                QStringLiteral("CSV EXPORT FAILED: Aircraft %1 has no current simulation state")
                    .arg(entity.id());
            qWarning().noquote() << "[CSV EXPORT]" << message;
            setStatusMessage(message);
            return false;
        }
        aircraftStates.insert(entity.id(), currentState);
    }

    CsvExporter::ExportSummary summary;
    QString errorMessage;
    if (!CsvExporter::writeFile(
            m_scenario, m_airbaseRegistry.records(), aircraftStates,
            commandHistorySnapshot, filePath, &summary, &errorMessage)) {
        qWarning().noquote() << "CSV export failed:"
                             << errorMessage << "requestedPath=" << filePath;
        setStatusMessage(QStringLiteral("CSV EXPORT FAILED: %1").arg(errorMessage));
        return false;
    }

    const QString successMessage =
        QStringLiteral("CSV EXPORT SUCCESS\nPath: %1\nRows: %2\nSize: %3 bytes")
            .arg(summary.absolutePath)
            .arg(summary.rowCount)
            .arg(summary.fileSizeBytes);
    qInfo().noquote() << successMessage;
    setStatusMessage(successMessage);
    return true;
}

Scenario ScenarioEngine::snapshotForPersistence() const
{
    Scenario snapshot = m_scenario;
    snapshot.scheduledTakeoffs() = m_simulationEngine.scheduledTakeoffs();
    snapshot.setRuntimeState(m_simulationEngine.runtimeStateToJson());
    return snapshot;
}

bool ScenarioEngine::loadScenario(const QString &filePath)
{
    Scenario loaded;
    QString prompt;
    QString errorMessage;
    if (!m_scenarioRepository.loadScenario(filePath, loaded, &prompt, &errorMessage))
    {
        setStatusMessage(QStringLiteral("LOAD FAILED: %1").arg(errorMessage));
        return false;
    }

    SimulationEngine restoredEngine;
    restoredEngine.setScenario(loaded.entities(), false);
    restoredEngine.setScheduledTakeoffs(loaded.scheduledTakeoffs());
    if (!loaded.runtimeState().isEmpty() &&
        !restoredEngine.restoreRuntimeState(loaded.runtimeState(), &errorMessage))
    {
        setStatusMessage(QStringLiteral("LOAD FAILED: %1").arg(errorMessage));
        return false;
    }

    m_scenario = std::move(loaded);
    m_simulationEngine.setScenario(m_scenario.entities(), false);
    m_simulationEngine.setScheduledTakeoffs(m_scenario.scheduledTakeoffs());
    if (!m_scenario.runtimeState().isEmpty())
    {
        if (!m_simulationEngine.restoreRuntimeState(m_scenario.runtimeState(), &errorMessage))
        {
            setStatusMessage(QStringLiteral("LOAD FAILED: %1").arg(errorMessage));
            return false;
        }
    }
    setScenarioGenerated(!m_scenario.entities().empty());
    clearSelectedAircraft();
    emit scenarioChanged();
    setStatusMessage(QStringLiteral("SCENARIO LOADED"));
    return true;
}

QVariantList ScenarioEngine::scheduledTakeoffs() const
{
    QVariantList result;
    for (const ScheduledTakeoff &event : m_simulationEngine.scheduledTakeoffs())
    {
        const int totalMinutes = static_cast<int>(event.scheduledSeconds) / 60;
        QString lifecycle = QStringLiteral("Airborne");
        AircraftSimulationState aircraftState;
        if (m_simulationEngine.aircraftState(event.aircraftId, &aircraftState))
            lifecycle = aircraftLifecycleName(aircraftState.lifecycle);
        result.push_back(QVariantMap{
            {QStringLiteral("aircraftId"), event.aircraftId},
            {QStringLiteral("origin"), event.originAirbaseName},
            {QStringLiteral("destination"), event.destinationName},
            {QStringLiteral("time"), QStringLiteral("%1:%2")
                                         .arg(totalMinutes / 60, 2, 10, QLatin1Char('0'))
                                         .arg(totalMinutes % 60, 2, 10, QLatin1Char('0'))},
            {QStringLiteral("status"), event.cancelled  ? QStringLiteral("Cancelled")
                                       : event.executed ? QStringLiteral("Executed")
                                                        : QStringLiteral("Pending")},
            {QStringLiteral("lifecycle"), lifecycle}});
    }
    return result;
}

QString ScenarioEngine::displayType(const QString &type)
{
    if (type == QStringLiteral("radar_site"))
        return QStringLiteral("Radar Site");
    if (type == QStringLiteral("ground_station"))
        return QStringLiteral("Ground Station");
    if (type == QStringLiteral("ship"))
        return QStringLiteral("Ship");
    return QStringLiteral("Aircraft");
}

QString ScenarioEngine::entityIdPrefix(const QString &type)
{
    if (type == QStringLiteral("radar_site"))
        return QStringLiteral("R");
    if (type == QStringLiteral("ground_station"))
        return QStringLiteral("G");
    if (type == QStringLiteral("ship"))
        return QStringLiteral("S");
    return QStringLiteral("A");
}

QString ScenarioEngine::formatEntitySummary(const QStringList &types)
{
    if (types.isEmpty())
        return QStringLiteral("Aircraft");
    if (types.size() == 1)
        return types.first();
    return types.join(QStringLiteral(" / "));
}

QString ScenarioEngine::scenarioNameForRegion(const QString &region)
{
    return region == QStringLiteral("Unknown Region")
               ? QStringLiteral("Scenario")
               : region + QStringLiteral(" Scenario");
}

void ScenarioEngine::regionCenter(const QString &region, double &latitude, double &longitude)
{
    if (region == QStringLiteral("North India"))
    {
        latitude = 29.75;
        longitude = 77.0;
    }
    else if (region == QStringLiteral("South India"))
    {
        latitude = 13.5;
        longitude = 78.5;
    }
    else if (region == QStringLiteral("East India"))
    {
        latitude = 23.5;
        longitude = 87.5;
    }
    else if (region == QStringLiteral("West India"))
    {
        latitude = 21.0;
        longitude = 73.5;
    }
    else if (region == QStringLiteral("Delhi"))
    {
        latitude = 28.6139;
        longitude = 77.2090;
    }
    else if (region == QStringLiteral("Bengaluru"))
    {
        latitude = 12.9716;
        longitude = 77.5946;
    }
    else if (region == QStringLiteral("Mumbai"))
    {
        latitude = 19.0760;
        longitude = 72.8777;
    }
    else if (region == QStringLiteral("Chennai"))
    {
        latitude = 13.0827;
        longitude = 80.2707;
    }
    else if (region == QStringLiteral("Pune"))
    {
        latitude = 18.5820;
        longitude = 73.9190;
    }
    else if (region == QStringLiteral("Goa"))
    {
        latitude = 15.3800;
        longitude = 73.8110;
    }
    else if (region == QStringLiteral("Hyderabad"))
    {
        latitude = 17.3850;
        longitude = 78.4867;
    }
    else if (region == QStringLiteral("Kolkata"))
    {
        latitude = 22.5726;
        longitude = 88.3639;
    }
    else
    {
        latitude = 22.5;
        longitude = 79.0;
    }
}

const std::vector<QPair<double, double>> &ScenarioEngine::pointsForRegion(const QString &region)
{
    if (region == QStringLiteral("North India"))
        return northIndiaPoints;
    if (region == QStringLiteral("South India"))
        return southIndiaPoints;
    if (region == QStringLiteral("East India"))
        return eastIndiaPoints;
    if (region == QStringLiteral("West India"))
        return westIndiaPoints;
    if (region == QStringLiteral("Delhi"))
        return delhiPoints;
    if (region == QStringLiteral("Bengaluru"))
        return bengaluruPoints;
    return defaultPoints;
}

void ScenarioEngine::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message)
        return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

void ScenarioEngine::setScenarioGenerated(bool generated)
{
    if (m_scenarioGenerated == generated)
        return;
    m_scenarioGenerated = generated;
    emit scenarioGeneratedChanged();
}

void ScenarioEngine::setAiRequestPending(bool pending)
{
    if (m_aiRequestPending == pending)
        return;
    m_aiRequestPending = pending;
    emit aiRequestPendingChanged();
}
