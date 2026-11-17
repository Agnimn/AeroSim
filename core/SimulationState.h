#pragma once

#include "Entity.h"

#include <QGeoCoordinate>
#include <QVector>

enum class AircraftLifecycle
{
    Scheduled,
    ReadyForDeparture,
    Airborne,
    Landed,
    Cancelled
};

inline QString aircraftLifecycleName(AircraftLifecycle lifecycle)
{
    switch (lifecycle)
    {
    case AircraftLifecycle::Scheduled:
        return QStringLiteral("Scheduled");
    case AircraftLifecycle::ReadyForDeparture:
        return QStringLiteral("ReadyForDeparture");
    case AircraftLifecycle::Airborne:
        return QStringLiteral("Airborne");
    case AircraftLifecycle::Landed:
        return QStringLiteral("Landed");
    case AircraftLifecycle::Cancelled:
        return QStringLiteral("Cancelled");
    }
    return QStringLiteral("Scheduled");
}

struct ScheduledTakeoff
{
    QString aircraftId;
    QString originAirbaseId;
    QString originAirbaseName;
    QString destinationName;
    double scheduledSeconds = 0.0;
    bool executed = false;
    bool cancelled = false;
};

struct AircraftSimulationState
{
    QString id;
    QString callsign;
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    double speed = 0.0;
    double heading = 0.0;
    double verticalSpeed = 0.0;
    int currentWaypoint = 1;
    QString status = QStringLiteral("ACTIVE");
    QString conflictStatus = QStringLiteral("NORMAL");
    QString aircraftType;
    AircraftLifecycle lifecycle = AircraftLifecycle::Airborne;
    QString movementStatus = QStringLiteral("STATIONARY");
    bool individuallyControlled = false;
    double conflictLatitude = 0.0;
    double conflictLongitude = 0.0;
    double avoidanceOffset = 0.0;
    double avoidanceUntil = 0.0;
    double nextTrailTime = 0.0;
    QVector<QGeoCoordinate> trail;
};
