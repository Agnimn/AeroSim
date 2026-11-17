#include "EntityModel.h"

#include <QGeoCoordinate>
#include <QVariant>
#include <QDebug>

EntityModel::EntityModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int EntityModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entities.size());
}

QVariant EntityModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};

    const Entity &entity = m_entities.at(static_cast<std::size_t>(index.row()));
    const auto stateRow = m_aircraftStateRows.constFind(entity.id());
    const AircraftSimulationState *state =
        stateRow == m_aircraftStateRows.cend() ? nullptr : &m_aircraftStates.at(stateRow.value());
    switch (role)
    {
    case IdRole:
        return entity.id();
    case NameRole:
        return entity.name();
    case EntityTypeRole:
        return entity.type();
    case LatitudeRole:
        return state ? state->latitude : entity.latitude();
    case LongitudeRole:
        return state ? state->longitude : entity.longitude();
    case AltitudeRole:
        return state ? state->altitude : entity.altitude();
    case SpeedRole:
        return state ? state->speed : entity.speed();
    case HeadingRole:
        return state ? state->heading : entity.heading();
    case StatusRole:
        return state ? state->status : entity.status();
    case CallsignRole:
        return state ? state->callsign : entity.callsign();
    case VerticalSpeedRole:
        return state ? state->verticalSpeed : entity.verticalSpeed();
    case AircraftTypeRole:
        return state ? state->aircraftType : entity.aircraftType();
    case CurrentWaypointRole:
        if (!state || entity.route().isEmpty())
            return QStringLiteral("HOLD");
        return entity.route()
            .at(qBound(0, state->currentWaypoint - 1,
                       static_cast<int>(entity.route().size() - 1)))
            .name;
    case NextWaypointRole:
        return state && state->currentWaypoint >= 0 &&
                       state->currentWaypoint < entity.route().size()
                   ? entity.route().at(state->currentWaypoint).name
                   : QStringLiteral("HOLD");
    case ConflictStatusRole:
        return state ? state->conflictStatus : QStringLiteral("NORMAL");
    case ConflictLatitudeRole:
        return state ? state->conflictLatitude : 0.0;
    case ConflictLongitudeRole:
        return state ? state->conflictLongitude : 0.0;
    case AvoidanceActiveRole:
        return state && state->avoidanceUntil > 0.0;
    case TrailRole:
    {
        QVariantList trail;
        if (state)
        {
            trail.reserve(state->trail.size());
            for (const QGeoCoordinate &coordinate : state->trail)
                trail.push_back(QVariant::fromValue(coordinate));
        }
        return trail;
    }
    case RouteRole:
    {
        QVariantList route;
        route.reserve(entity.route().size() + (entity.routeLoop() ? 1 : 0));
        for (const Waypoint &waypoint : entity.route())
            route.push_back(QVariant::fromValue(QGeoCoordinate(waypoint.latitude,
                                                               waypoint.longitude)));
        if (entity.routeLoop() && !entity.route().isEmpty())
        {
            const Waypoint &first = entity.route().first();
            route.push_back(QVariant::fromValue(QGeoCoordinate(first.latitude,
                                                               first.longitude)));
        }
        return route;
    }
    case LifecycleRole:
        return state ? aircraftLifecycleName(state->lifecycle) : QStringLiteral("Airborne");
    case MovementStatusRole:
        return state ? state->movementStatus : QStringLiteral("STATIONARY");
    case FormationIdRole:
        return entity.formationId();
    case FormationTypeRole:
        return entity.formationType();
    case FormationLeaderIdRole:
        return entity.formationLeaderId();
    case FormationSlotRole:
        return entity.formationSlot();
    case FormationOffsetRole:
        return QVariantMap{{QStringLiteral("xNm"), entity.formationOffset().xNm},
                           {QStringLiteral("yNm"), entity.formationOffset().yNm}};
    case FormationSpacingRole:
        return entity.formationSpacing();
    case FormationHeadingRole:
        return entity.formationHeading();
    case IsFormationMemberRole:
        return entity.isFormationMember();
    case FormationStateRole:
        return entity.formationState();
    default:
        return {};
    }
}

QHash<int, QByteArray> EntityModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {EntityTypeRole, "entityType"},
        {LatitudeRole, "latitude"},
        {LongitudeRole, "longitude"},
        {AltitudeRole, "altitude"},
        {SpeedRole, "speed"},
        {HeadingRole, "heading"},
        {StatusRole, "status"},
        {CallsignRole, "callsign"},
        {VerticalSpeedRole, "verticalSpeed"},
        {AircraftTypeRole, "aircraftType"},
        {CurrentWaypointRole, "currentWaypoint"},
        {NextWaypointRole, "nextWaypoint"},
        {ConflictStatusRole, "conflictStatus"},
        {ConflictLatitudeRole, "conflictLatitude"},
        {ConflictLongitudeRole, "conflictLongitude"},
        {AvoidanceActiveRole, "avoidanceActive"},
        {TrailRole, "trail"},
        {RouteRole, "route"},
        {LifecycleRole, "lifecycle"},
        {MovementStatusRole, "movementStatus"},
        {FormationIdRole, "formationId"},
        {FormationTypeRole, "formationType"},
        {FormationLeaderIdRole, "formationLeaderId"},
        {FormationSlotRole, "formationSlot"},
        {FormationOffsetRole, "formationOffset"},
        {FormationSpacingRole, "formationSpacing"},
        {FormationHeadingRole, "formationHeading"},
        {IsFormationMemberRole, "isFormationMember"},
        {FormationStateRole, "formationState"}};
}

void EntityModel::setEntities(const std::vector<Entity> &entities)
{
    beginResetModel();
    m_entities = entities;
    m_aircraftStates.clear();
    m_aircraftStateRows.clear();
    endResetModel();
}

void EntityModel::setEntitiesIncremental(const std::vector<Entity> &entities)
{
    qDebug().noquote() << "[ENTITY MODEL] incremental update:"
                       << m_entities.size() << "->" << entities.size();
    int firstDifference = 0;
    while (firstDifference < static_cast<int>(m_entities.size()) &&
           firstDifference < static_cast<int>(entities.size()) &&
           m_entities.at(static_cast<std::size_t>(firstDifference)).id() ==
               entities.at(static_cast<std::size_t>(firstDifference)).id())
        ++firstDifference;
    if (entities.size() > m_entities.size())
    {
        beginInsertRows({}, static_cast<int>(m_entities.size()),
                        static_cast<int>(entities.size()) - 1);
        m_entities = entities;
        endInsertRows();
    }
    else if (entities.size() < m_entities.size())
    {
        const int removedCount = static_cast<int>(m_entities.size() - entities.size());
        beginRemoveRows({}, firstDifference, firstDifference + removedCount - 1);
        m_entities = entities;
        endRemoveRows();
    }
    else
    {
        m_entities = entities;
    }
    if (firstDifference < rowCount())
        emit dataChanged(index(firstDifference), index(rowCount() - 1));
    else if (rowCount() > 0)
        emit dataChanged(index(0), index(rowCount() - 1));
}

const std::vector<Entity> &EntityModel::entities() const
{
    return m_entities;
}

void EntityModel::setAircraftStates(const QVector<AircraftSimulationState> &states,
                                    bool trailsChanged)
{
    m_aircraftStates = states;
    m_aircraftStateRows.clear();
    for (int stateIndex = 0; stateIndex < m_aircraftStates.size(); ++stateIndex)
        m_aircraftStateRows.insert(m_aircraftStates.at(stateIndex).id, stateIndex);

    if (m_entities.empty())
        return;

    QVector<int> changedRoles = {
        LatitudeRole, LongitudeRole, AltitudeRole, SpeedRole, HeadingRole, StatusRole,
        VerticalSpeedRole, CurrentWaypointRole, NextWaypointRole, ConflictStatusRole,
        ConflictLatitudeRole, ConflictLongitudeRole, AvoidanceActiveRole, LifecycleRole,
        MovementStatusRole, FormationHeadingRole, FormationStateRole};
    if (trailsChanged)
        changedRoles.push_back(TrailRole);
    emit dataChanged(index(0), index(rowCount() - 1), changedRoles);
}

const QVector<AircraftSimulationState> &EntityModel::aircraftStates() const
{
    return m_aircraftStates;
}

int EntityModel::rowForId(const QString &id) const
{
    for (int row = 0; row < static_cast<int>(m_entities.size()); ++row)
    {
        if (m_entities.at(static_cast<std::size_t>(row)).id() == id)
            return row;
    }
    return -1;
}
