#include "Entity.h"

#include <QJsonValue>
#include <utility>

Entity::Entity(QString id, QString name, QString type, double latitude, double longitude,
               double altitude, double speed, double heading, QString status)
    : m_id(std::move(id)),
      m_name(std::move(name)),
      m_type(std::move(type)),
      m_latitude(latitude),
      m_longitude(longitude),
      m_altitude(altitude),
      m_speed(speed),
      m_heading(heading),
      m_status(std::move(status))
{
}

const QString &Entity::id() const { return m_id; }
const QString &Entity::name() const { return m_name; }
const QString &Entity::type() const { return m_type; }
double Entity::latitude() const { return m_latitude; }
double Entity::longitude() const { return m_longitude; }
double Entity::altitude() const { return m_altitude; }
double Entity::speed() const { return m_speed; }
double Entity::heading() const { return m_heading; }
const QString &Entity::status() const { return m_status; }

QJsonObject Entity::toJson() const
{
    return {
        {QStringLiteral("id"), m_id},
        {QStringLiteral("name"), m_name},
        {QStringLiteral("type"), m_type},
        {QStringLiteral("latitude"), m_latitude},
        {QStringLiteral("longitude"), m_longitude},
        {QStringLiteral("altitude"), m_altitude},
        {QStringLiteral("speed"), m_speed},
        {QStringLiteral("heading"), m_heading},
        {QStringLiteral("status"), m_status}};
}

Entity Entity::fromJson(const QJsonObject &json)
{
    return Entity(json.value(QStringLiteral("id")).toString(),
                  json.value(QStringLiteral("name")).toString(),
                  json.value(QStringLiteral("type")).toString(),
                  json.value(QStringLiteral("latitude")).toDouble(),
                  json.value(QStringLiteral("longitude")).toDouble(),
                  json.value(QStringLiteral("altitude")).toDouble(),
                  json.value(QStringLiteral("speed")).toDouble(),
                  json.value(QStringLiteral("heading")).toDouble(),
                  json.value(QStringLiteral("status")).toString());
}
