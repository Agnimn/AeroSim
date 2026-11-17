#pragma once

#include "../core/Entity.h"
#include "../core/SimulationState.h"

#include <QAbstractListModel>
#include <vector>

class EntityModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        IdRole = Qt::UserRole + 1,
        NameRole,
        EntityTypeRole,
        LatitudeRole,
        LongitudeRole,
        AltitudeRole,
        SpeedRole,
        HeadingRole,
        StatusRole,
        CallsignRole,
        VerticalSpeedRole,
        AircraftTypeRole,
        CurrentWaypointRole,
        NextWaypointRole,
        ConflictStatusRole,
        ConflictLatitudeRole,
        ConflictLongitudeRole,
        AvoidanceActiveRole,
        TrailRole,
        RouteRole,
        LifecycleRole,
        MovementStatusRole,
        FormationIdRole,
        FormationTypeRole,
        FormationLeaderIdRole,
        FormationSlotRole,
        FormationOffsetRole,
        FormationSpacingRole,
        FormationHeadingRole,
        IsFormationMemberRole,
        FormationStateRole
    };
    Q_ENUM(Role)

    explicit EntityModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntities(const std::vector<Entity> &entities);
    void setEntitiesIncremental(const std::vector<Entity> &entities);
    const std::vector<Entity> &entities() const;
    void setAircraftStates(const QVector<AircraftSimulationState> &states, bool trailsChanged);
    const QVector<AircraftSimulationState> &aircraftStates() const;
    int rowForId(const QString &id) const;

private:
    std::vector<Entity> m_entities;
    QVector<AircraftSimulationState> m_aircraftStates;
    QHash<QString, int> m_aircraftStateRows;
};
