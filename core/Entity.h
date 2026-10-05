#pragma once

#include <QJsonObject>
#include <QString>

class Entity
{
public:
    Entity() = default;
    Entity(QString id, QString name, QString type, double latitude, double longitude,
           double altitude, double speed, double heading, QString status);

    const QString &id() const;
    const QString &name() const;
    const QString &type() const;
    double latitude() const;
    double longitude() const;
    double altitude() const;
    double speed() const;
    double heading() const;
    const QString &status() const;

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
};
