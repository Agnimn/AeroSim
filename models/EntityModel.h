#pragma once

#include "../core/Entity.h"

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
        StatusRole
    };
    Q_ENUM(Role)

    explicit EntityModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntities(const std::vector<Entity> &entities);
    const std::vector<Entity> &entities() const;

private:
    std::vector<Entity> m_entities;
};
