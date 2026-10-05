#pragma once

#include "Entity.h"

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
    const std::vector<Entity> &entities() const;

    QJsonObject toJson() const;
    static Scenario fromJson(const QJsonObject &json);

private:
    QString m_name;
    QString m_description;
    QString m_region;
    std::vector<Entity> m_entities;
};
