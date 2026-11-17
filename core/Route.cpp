#include "Route.h"

#include <QJsonArray>

QJsonObject ScenarioRoute::toJson() const
{
    QJsonArray waypointArray;
    for (const Waypoint &waypoint : waypoints)
        waypointArray.append(QJsonObject{
            {QStringLiteral("name"), waypoint.name},
            {QStringLiteral("latitude"), waypoint.latitude},
            {QStringLiteral("longitude"), waypoint.longitude},
            {QStringLiteral("altitude"), waypoint.altitude}});
    return {{QStringLiteral("id"), id},
            {QStringLiteral("name"), name},
            {QStringLiteral("aircraftId"), aircraftId},
            {QStringLiteral("status"), status},
            {QStringLiteral("waypoints"), waypointArray}};
}

ScenarioRoute ScenarioRoute::fromJson(const QJsonObject &json)
{
    ScenarioRoute route;
    route.id = json.value(QStringLiteral("id")).toString();
    route.name = json.value(QStringLiteral("name")).toString();
    route.aircraftId = json.value(QStringLiteral("aircraftId")).toString();
    route.status = json.value(QStringLiteral("status")).toString(QStringLiteral("DISPLAYED"));
    for (const QJsonValue &value : json.value(QStringLiteral("waypoints")).toArray()) {
        const QJsonObject waypoint = value.toObject();
        route.waypoints.push_back({waypoint.value(QStringLiteral("name")).toString(),
                                   waypoint.value(QStringLiteral("latitude")).toDouble(),
                                   waypoint.value(QStringLiteral("longitude")).toDouble(),
                                   waypoint.value(QStringLiteral("altitude")).toDouble()});
    }
    return route;
}
