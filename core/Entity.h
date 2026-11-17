#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

struct Waypoint
{
    QString name;
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
};

struct FormationOffset
{
    double xNm = 0.0;
    double yNm = 0.0;
};

class Entity
{
public:
    Entity() = default;
    Entity(QString id, QString name, QString type, double latitude, double longitude,
           double altitude, double speed, double heading, QString status,
           QString callsign = {}, double verticalSpeed = 0.0, QString aircraftType = {},
           QVector<Waypoint> route = {}, bool routeLoop = false);

    const QString &id() const;
    const QString &name() const;
    const QString &type() const;
    double latitude() const;
    double longitude() const;
    double altitude() const;
    double speed() const;
    double heading() const;
    const QString &status() const;
    const QString &callsign() const;
    double verticalSpeed() const;
    const QString &aircraftType() const;
    const QVector<Waypoint> &route() const;
    bool routeLoop() const;
    const QString &formationId() const;
    const QString &formationType() const;
    const QString &formationLeaderId() const;
    int formationSlot() const;
    FormationOffset formationOffset() const;
    double formationSpacing() const;
    double formationHeading() const;
    bool isFormationMember() const;
    const QString &formationState() const;

    void setPosition(double latitude, double longitude);
    void setPosition(double latitude, double longitude, double altitude);
    void setRoute(QVector<Waypoint> route, bool routeLoop);
    void setFormation(QString formationId, QString formationType,
                      QString formationLeaderId, int formationSlot,
                      FormationOffset formationOffset, double formationSpacing,
                      double formationHeading, QString formationState);
    void clearFormation();

    QJsonObject toJson() const;
    static Entity fromJson(const QJsonObject &json);

private:
    QString m_id;
    QString m_name;
    QString m_type;
    double m_latitude = 0.0;
    double m_longitude = 0.0;
    double m_altitude = 0.0;
    double m_speed = 0.0;
    double m_heading = 0.0;
    QString m_status;
    QString m_callsign;
    double m_verticalSpeed = 0.0;
    QString m_aircraftType;
    QVector<Waypoint> m_route;
    bool m_routeLoop = false;
    QString m_formationId;
    QString m_formationType;
    QString m_formationLeaderId;
    int m_formationSlot = -1;
    FormationOffset m_formationOffset;
    double m_formationSpacing = 0.0;
    double m_formationHeading = 0.0;
    QString m_formationState;
};
