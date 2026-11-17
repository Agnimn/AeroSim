#pragma once

#include "Entity.h"

#include <QJsonObject>
#include <QString>
#include <QVector>

struct ScenarioRoute
{
    QString id;
    QString name;
    QVector<Waypoint> waypoints;
    QString aircraftId;
    QString status = QStringLiteral("DISPLAYED");

    QJsonObject toJson() const;
    static ScenarioRoute fromJson(const QJsonObject &json);
};
