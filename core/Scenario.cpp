#include "Scenario.h"

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
}

std::size_t Scenario::entityCount() const
{
    return m_entities.size();
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

    return {
        {QStringLiteral("scenario"), QJsonObject{
                                         {QStringLiteral("name"), m_name},
                                         {QStringLiteral("description"), m_description},
                                         {QStringLiteral("region"), m_region}}},
        {QStringLiteral("entities"), entityArray}};
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
    return scenario;
}
