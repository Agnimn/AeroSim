#pragma once

#include <QAbstractListModel>
#include <QGeoCoordinate>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

struct AirbaseRecord
{
    QString id;
    QString name;
    QStringList aliases;
    QGeoCoordinate coordinate;
    bool coordinateAvailable = false;
    QString stateOrUnionTerritory;
    QString region;
    QString command;
    QString commandStatus;
    QString category;
    QString coordinateSource;
    QString verificationStatus;
};

class AirbaseRegistry final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        IdRole = Qt::UserRole + 1,
        NameRole,
        AliasesRole,
        LatitudeRole,
        LongitudeRole,
        CoordinateAvailableRole,
        StateRole,
        RegionRole,
        CommandRole,
        CommandStatusRole,
        CategoryRole,
        CoordinateSourceRole,
        VerificationStatusRole
    };
    Q_ENUM(Role)

    explicit AirbaseRegistry(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    const QVector<AirbaseRecord> &records() const;
    const AirbaseRecord *byId(const QString &id) const;
    QVector<const AirbaseRecord *> matchingRegion(const QString &region) const;
    QVector<const AirbaseRecord *> matchingText(const QString &text) const;

private:
    QVector<AirbaseRecord> m_records;
};
