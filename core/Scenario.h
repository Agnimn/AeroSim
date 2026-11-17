#pragma once

#include "Entity.h"
#include "Route.h"
#include "SimulationState.h"

#include <QJsonObject>
#include <QString>
#include <vector>


class Scenario
{
public:
    Scenario() = default;
    Scenario(QString name, QString description, QString region);

    const QString &name() const;
    const QString &description() const;
    const QString &region() const;
    void setName(QString name);
    void setDescription(QString description);
    void setRegion(QString region);

    void addEntity(Entity entity);
    bool removeEntity(const QString &id);
    void clearEntities();
    std::size_t entityCount() const;
    std::vector<Entity> &entities();
    const std::vector<Entity> &entities() const;
    void addScheduledTakeoff(ScheduledTakeoff event);
    QVector<ScheduledTakeoff> &scheduledTakeoffs();
    const QVector<ScheduledTakeoff> &scheduledTakeoffs() const;
    const QVector<ScenarioRoute> &routes() const;
    QVector<ScenarioRoute> &routes();
    void addRoute(ScenarioRoute route);
    void clearRoutes();
    void setRuntimeState(QJsonObject runtimeState);
    const QJsonObject &runtimeState() const;

    QJsonObject toJson() const;
    static Scenario fromJson(const QJsonObject &json);

private:
    QString m_name;
    QString m_description;
    QString m_region;
    std::vector<Entity> m_entities;
    QVector<ScheduledTakeoff> m_scheduledTakeoffs;
    QVector<ScenarioRoute> m_routes;
    QJsonObject m_runtimeState;
};
