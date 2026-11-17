#pragma once

#include <QVector>
#include <QString>
#include <QStringList>

inline constexpr int MaximumScenarioEntityCount = 200;

enum class ScenarioOperation
{
    Add,
    Replace,
    Query,
    Reset,
    Save,
    Load,
    Move,
    ChangeRoute,
    StopAircraft,
    StopAllAircraft,
    PauseAircraft,
    ResumeAircraft,
    StartAircraft,
    RemoveAircraft,
    RemoveLastAircraft,
    RemoveAllAircraft,
    StopFormation,
    ResumeFormation,
    DisbandFormation,
    ScheduleTakeoff,
    ListScheduledTakeoffs,
    CancelScheduledTakeoff,
    CreateFormation,
    MoveFormation,
    CreateRoute,
    DisplayRoute,
    AssignRoute,
    MoveAlongRoute
};

struct EntityRequest
{
    QString type;
    int count = 0;
    double altitude = 10000.0;
    double speed = 450.0;
    double heading = 90.0;
};

// Structured output from a parser; kept separate from generation and the UI.
struct ScenarioRequest
{
    ScenarioOperation operation = ScenarioOperation::Add;
    QString operationTargetId;
    QString operationLocation;
    QString originAirbaseId;
    QString originAirbaseName;
    QString destinationName;
    // Scheduled clock times are relative to scenario day 00:00:00.
    int scheduledSeconds = -1;
    bool explicitOperation = false;
    QString entityType;
    QStringList entityTypes;
    QVector<EntityRequest> entities;
    int entityCount = 1;
    QString region;
    QString locationName;
    QStringList routeDestinations;
    QString routeName;
    QString routeAircraftId;
    QStringList routeWaypoints;
    double radiusKm = 0.0;
    double altitude = 10000.0;
    double speed = 450.0;
    double heading = 90.0;
    QString routePattern = QStringLiteral("waypoint");
    bool hasRadius = false;
    bool hasCustomLocation = false;
    bool airbaseDisplayOnly = false;
    bool airbaseRegionTarget = false;
    bool removeLastN = false;
    bool allFormations = false;
    bool formationTypeSpecified = false;
    QString airbaseId;
    QString airbaseName;
    QString airbaseRegion;
    QString formationType = QStringLiteral("V");
    double formationSpacing = 3.0;
    double formationHeading = 90.0;
};
