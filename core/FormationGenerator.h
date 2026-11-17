#pragma once

#include "Entity.h"

#include <QGeoCoordinate>
#include <QString>
#include <QVector>

struct FormationSlot
{
    QString formationId;
    QString leaderSlotId;
    QString slotId;
    int slot = -1;
    bool isLeader = false;
    FormationOffset offset;
    QGeoCoordinate coordinate;
};

class FormationGenerator final
{
public:
    static QStringList supportedTypes();
    static QString canonicalType(const QString &type);
    static bool generate(int count, const QString &type,
                         const QGeoCoordinate &reference, double headingDegrees,
                         double spacingNm, QVector<FormationSlot> *outputSlots,
                         QString *errorMessage = nullptr);
};
