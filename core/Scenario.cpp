#include "Scenario.h"
#include "Route.h"

#include <QJsonArray>
#include <algorithm>
#include <utility>

Scenario::Scenario(QString name, QString description, QString region)
    : m_name(std::move(name)),
      m_description(std::move(description)),
      m_region(std::move(region))
{
}

const QString &Scenario::name() const { return m_name; }
const QString &Scenario::description() const { return m_description; }
const QString &Scenario::region() const { return m_region; }
void Scenario::setName(QString name) { m_name = std::move(name); }
void Scenario::setDescription(QString description) { m_description = std::move(description); }
void Scenario::setRegion(QString region) { m_region = std::move(region); }

void Scenario::addEntity(Entity entity)
{
    m_entities.push_back(std::move(entity));
}

bool Scenario::removeEntity(const QString &id)
{
    const auto it = std::find_if(m_entities.begin(), m_entities.end(),
                                 [&id](const Entity &entity)
                                 { return entity.id() == id; });
    if (it == m_entities.end())
        return false;
    m_entities.erase(it);
    return true;
}

void Scenario::clearEntities()
{
    m_entities.clear();
    m_scheduledTakeoffs.clear();
    m_routes.clear();
}

std::size_t Scenario::entityCount() const
{
    return m_entities.size();
}

std::vector<Entity> &Scenario::entities()
{
    return m_entities;
}

void Scenario::addScheduledTakeoff(ScheduledTakeoff event)
{
    m_scheduledTakeoffs.push_back(std::move(event));
}

QVector<ScheduledTakeoff> &Scenario::scheduledTakeoffs()
{
    return m_scheduledTakeoffs;
}

const QVector<ScheduledTakeoff> &Scenario::scheduledTakeoffs() const
{
    return m_scheduledTakeoffs;
}

const QVector<ScenarioRoute> &Scenario::routes() const { return m_routes; }
QVector<ScenarioRoute> &Scenario::routes() { return m_routes; }
void Scenario::addRoute(ScenarioRoute route) { m_routes.push_back(std::move(route)); }
void Scenario::clearRoutes() { m_routes.clear(); }

void Scenario::setRuntimeState(QJsonObject runtimeState)
{
    m_runtimeState = std::move(runtimeState);
}

const QJsonObject &Scenario::runtimeState() const
{
    return m_runtimeState;
}

const std::vector<Entity> &Scenario::entities() const
{
    return m_entities;
}

QJsonObject Scenario::toJson() const
{
    QJsonArray entityArray;
    for (const Entity &entity : m_entities)
        entityArray.append(entity.toJson());

    QJsonArray scheduledArray;
    for (const ScheduledTakeoff &event : m_scheduledTakeoffs)
    {
        scheduledArray.append(QJsonObject{
            {QStringLiteral("aircraftId"), event.aircraftId},
            {QStringLiteral("originAirbaseId"), event.originAirbaseId},
            {QStringLiteral("originAirbaseName"), event.originAirbaseName},
            {QStringLiteral("destinationName"), event.destinationName},
            {QStringLiteral("scheduledSeconds"), event.scheduledSeconds},
            {QStringLiteral("executed"), event.executed},
            {QStringLiteral("cancelled"), event.cancelled}});
    }

    QJsonObject result{
        {QStringLiteral("scenario"), QJsonObject{
                                         {QStringLiteral("name"), m_name},
                                         {QStringLiteral("description"), m_description},
                                         {QStringLiteral("region"), m_region}}},
        {QStringLiteral("entities"), entityArray},
        {QStringLiteral("scheduledTakeoffs"), scheduledArray}};
    QJsonArray routeArray;
    for (const ScenarioRoute &route : m_routes)
        routeArray.append(route.toJson());
    result.insert(QStringLiteral("routes"), routeArray);
    if (!m_runtimeState.isEmpty())
        result.insert(QStringLiteral("runtimeState"), m_runtimeState);
    return result;
}

Scenario Scenario::fromJson(const QJsonObject &json)
{
    const QJsonObject scenarioData = json.value(QStringLiteral("scenario")).toObject();
    Scenario scenario(scenarioData.value(QStringLiteral("name")).toString(),
                      scenarioData.value(QStringLiteral("description")).toString(),
                      scenarioData.value(QStringLiteral("region")).toString());

    const QJsonArray entityArray = json.value(QStringLiteral("entities")).toArray();
    for (const QJsonValue &value : entityArray)
    {
        if (value.isObject())
            scenario.addEntity(Entity::fromJson(value.toObject()));
    }
    const QJsonArray scheduledArray = json.value(QStringLiteral("scheduledTakeoffs")).toArray();
    for (const QJsonValue &value : scheduledArray)
    {
        if (!value.isObject())
            continue;
        const QJsonObject eventObject = value.toObject();
        ScheduledTakeoff event;
        event.aircraftId = eventObject.value(QStringLiteral("aircraftId")).toString();
        event.originAirbaseId = eventObject.value(QStringLiteral("originAirbaseId")).toString();
        event.originAirbaseName = eventObject.value(QStringLiteral("originAirbaseName")).toString();
        event.destinationName = eventObject.value(QStringLiteral("destinationName")).toString();
        event.scheduledSeconds = eventObject.value(QStringLiteral("scheduledSeconds")).toDouble();
        event.executed = eventObject.value(QStringLiteral("executed")).toBool();
        event.cancelled = eventObject.value(QStringLiteral("cancelled")).toBool();
        if (!event.aircraftId.isEmpty())
            scenario.addScheduledTakeoff(std::move(event));
    }
    for (const QJsonValue &value : json.value(QStringLiteral("routes")).toArray())
        if (value.isObject())
            scenario.addRoute(ScenarioRoute::fromJson(value.toObject()));
    if (json.value(QStringLiteral("runtimeState")).isObject())
        scenario.setRuntimeState(json.value(QStringLiteral("runtimeState")).toObject());
    return scenario;
}
