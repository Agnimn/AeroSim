#include "Entity.h"

#include <QJsonArray>
#include <QJsonValue>
#include <utility>

Entity::Entity(QString id, QString name, QString type, double latitude, double longitude,
               double altitude, double speed, double heading, QString status,
               QString callsign, double verticalSpeed, QString aircraftType,
               QVector<Waypoint> route, bool routeLoop)
    : m_id(std::move(id)),
      m_name(std::move(name)),
      m_type(std::move(type)),
      m_latitude(latitude),
      m_longitude(longitude),
      m_altitude(altitude),
      m_speed(speed),
      m_heading(heading),
      m_status(std::move(status)),
      m_callsign(std::move(callsign)),
      m_verticalSpeed(verticalSpeed),
      m_aircraftType(std::move(aircraftType)),
      m_route(std::move(route)),
      m_routeLoop(routeLoop)
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
const QString &Entity::callsign() const { return m_callsign; }
double Entity::verticalSpeed() const { return m_verticalSpeed; }
const QString &Entity::aircraftType() const { return m_aircraftType; }
const QVector<Waypoint> &Entity::route() const { return m_route; }
bool Entity::routeLoop() const { return m_routeLoop; }
const QString &Entity::formationId() const { return m_formationId; }
const QString &Entity::formationType() const { return m_formationType; }
const QString &Entity::formationLeaderId() const { return m_formationLeaderId; }
int Entity::formationSlot() const { return m_formationSlot; }
FormationOffset Entity::formationOffset() const { return m_formationOffset; }
double Entity::formationSpacing() const { return m_formationSpacing; }
double Entity::formationHeading() const { return m_formationHeading; }
bool Entity::isFormationMember() const { return !m_formationId.isEmpty(); }
const QString &Entity::formationState() const { return m_formationState; }

void Entity::setPosition(double latitude, double longitude)
{
    m_latitude = latitude;
    m_longitude = longitude;
}

void Entity::setPosition(double latitude, double longitude, double altitude)
{
    setPosition(latitude, longitude);
    m_altitude = altitude;
}

void Entity::setRoute(QVector<Waypoint> route, bool routeLoop)
{
    m_route = std::move(route);
    m_routeLoop = routeLoop;
}

void Entity::setFormation(QString formationId, QString formationType,
                          QString formationLeaderId, int formationSlot,
                          FormationOffset formationOffset, double formationSpacing,
                          double formationHeading, QString formationState)
{
    m_formationId = std::move(formationId);
    m_formationType = std::move(formationType);
    m_formationLeaderId = std::move(formationLeaderId);
    m_formationSlot = formationSlot;
    m_formationOffset = formationOffset;
    m_formationSpacing = formationSpacing;
    m_formationHeading = formationHeading;
    m_formationState = std::move(formationState);
}

void Entity::clearFormation()
{
    m_formationId.clear();
    m_formationType.clear();
    m_formationLeaderId.clear();
    m_formationSlot = -1;
    m_formationOffset = {};
    m_formationSpacing = 0.0;
    m_formationHeading = 0.0;
    m_formationState.clear();
}

QJsonObject Entity::toJson() const
{
    QJsonArray routeArray;
    for (const Waypoint &waypoint : m_route)
    {
        routeArray.append(QJsonObject{
            {QStringLiteral("name"), waypoint.name},
            {QStringLiteral("latitude"), waypoint.latitude},
            {QStringLiteral("longitude"), waypoint.longitude},
            {QStringLiteral("altitude"), waypoint.altitude}});
    }

    return {
        {QStringLiteral("id"), m_id},
        {QStringLiteral("name"), m_name},
        {QStringLiteral("type"), m_type},
        {QStringLiteral("latitude"), m_latitude},
        {QStringLiteral("longitude"), m_longitude},
        {QStringLiteral("altitude"), m_altitude},
        {QStringLiteral("speed"), m_speed},
        {QStringLiteral("heading"), m_heading},
        {QStringLiteral("status"), m_status},
        {QStringLiteral("callsign"), m_callsign},
        {QStringLiteral("verticalSpeed"), m_verticalSpeed},
        {QStringLiteral("aircraftType"), m_aircraftType},
        {QStringLiteral("route"), routeArray},
        {QStringLiteral("routeLoop"), m_routeLoop},
        {QStringLiteral("formationId"), m_formationId},
        {QStringLiteral("formationType"), m_formationType},
        {QStringLiteral("formationLeaderId"), m_formationLeaderId},
        {QStringLiteral("formationSlot"), m_formationSlot},
        {QStringLiteral("formationOffset"),
         QJsonObject{{QStringLiteral("xNm"), m_formationOffset.xNm},
                     {QStringLiteral("yNm"), m_formationOffset.yNm}}},
        {QStringLiteral("formationSpacing"), m_formationSpacing},
        {QStringLiteral("formationHeading"), m_formationHeading},
        {QStringLiteral("isFormationMember"), isFormationMember()},
        {QStringLiteral("formationState"), m_formationState}};
}

Entity Entity::fromJson(const QJsonObject &json)
{
    QVector<Waypoint> route;
    const QJsonArray routeArray = json.value(QStringLiteral("route")).toArray();
    route.reserve(routeArray.size());
    for (const QJsonValue &value : routeArray)
    {
        if (!value.isObject())
            continue;
        const QJsonObject waypoint = value.toObject();
        route.push_back({waypoint.value(QStringLiteral("name")).toString(),
                         waypoint.value(QStringLiteral("latitude")).toDouble(),
                         waypoint.value(QStringLiteral("longitude")).toDouble(),
                         waypoint.value(QStringLiteral("altitude")).toDouble()});
    }

    Entity entity(json.value(QStringLiteral("id")).toString(),
                  json.value(QStringLiteral("name")).toString(),
                  json.value(QStringLiteral("type")).toString(),
                  json.value(QStringLiteral("latitude")).toDouble(),
                  json.value(QStringLiteral("longitude")).toDouble(),
                  json.value(QStringLiteral("altitude")).toDouble(),
                  json.value(QStringLiteral("speed")).toDouble(),
                  json.value(QStringLiteral("heading")).toDouble(),
                  json.value(QStringLiteral("status")).toString(),
                  json.value(QStringLiteral("callsign")).toString(),
                  json.value(QStringLiteral("verticalSpeed")).toDouble(),
                  json.value(QStringLiteral("aircraftType")).toString(),
                  std::move(route),
                  json.value(QStringLiteral("routeLoop")).toBool());
    const QJsonObject offset = json.value(QStringLiteral("formationOffset")).toObject();
    if (json.value(QStringLiteral("isFormationMember")).toBool() ||
        !json.value(QStringLiteral("formationId")).toString().isEmpty())
    {
        entity.setFormation(
            json.value(QStringLiteral("formationId")).toString(),
            json.value(QStringLiteral("formationType")).toString(),
            json.value(QStringLiteral("formationLeaderId")).toString(),
            json.value(QStringLiteral("formationSlot")).toInt(-1),
            {offset.value(QStringLiteral("xNm")).toDouble(),
             offset.value(QStringLiteral("yNm")).toDouble()},
            json.value(QStringLiteral("formationSpacing")).toDouble(),
            json.value(QStringLiteral("formationHeading")).toDouble(),
            json.value(QStringLiteral("formationState")).toString());
    }
    return entity;
}
