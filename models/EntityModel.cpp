#include "EntityModel.h"

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
    switch (role)
    {
    case IdRole:
        return entity.id();
    case NameRole:
        return entity.name();
    case EntityTypeRole:
        return entity.type();
    case LatitudeRole:
        return entity.latitude();
    case LongitudeRole:
        return entity.longitude();
    case AltitudeRole:
        return entity.altitude();
    case SpeedRole:
        return entity.speed();
    case HeadingRole:
        return entity.heading();
    case StatusRole:
        return entity.status();
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
        {StatusRole, "status"}};
}

void EntityModel::setEntities(const std::vector<Entity> &entities)
{
    beginResetModel();
    m_entities = entities;
    endResetModel();
}

const std::vector<Entity> &EntityModel::entities() const
{
    return m_entities;
}
