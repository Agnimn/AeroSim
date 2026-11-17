#include "FormationGenerator.h"

#include <QUuid>
#include <QtMath>
#include <cmath>

namespace
{
    constexpr double EarthRadiusNm = 3440.065;
    constexpr double MinimumSpacingNm = 0.1;
    constexpr double MaximumSpacingNm = 100.0;

    QGeoCoordinate positionAt(const QGeoCoordinate &reference,
                              const FormationOffset &offset, double heading)
    {
        const double east = offset.xNm;
        const double north = offset.yNm;
        const double distance = qSqrt(east * east + north * north);
        if (distance == 0.0)
            return reference;
        const double bearing = heading +
                               qRadiansToDegrees(qAtan2(east, north));
        return reference.atDistanceAndAzimuth(distance * 1852.0, bearing);
    }

    QVector<FormationOffset> makeOffsets(int count, const QString &type,
                                         double spacing)
    {
        QVector<FormationOffset> offsets;
        offsets.reserve(count);
        offsets.push_back({0.0, 0.0});

        if (type == QStringLiteral("LINE"))
        {
            for (int index = 1; index < count; ++index)
            {
                const int rank = (index + 1) / 2;
                const double side = index % 2 == 1 ? 1.0 : -1.0;
                offsets.push_back({side * rank * spacing, 0.0});
            }
        }
        else if (type == QStringLiteral("COLUMN"))
        {
            for (int index = 1; index < count; ++index)
                offsets.push_back({0.0, -index * spacing});
        }
        else if (type == QStringLiteral("V") ||
                 type == QStringLiteral("INVERTED_V") ||
                 type == QStringLiteral("WEDGE") ||
                 type == QStringLiteral("ECHELON_LEFT") ||
                 type == QStringLiteral("ECHELON_RIGHT"))
        {
            const bool inverted = type == QStringLiteral("INVERTED_V");
            const bool echelon = type == QStringLiteral("ECHELON_LEFT") ||
                                 type == QStringLiteral("ECHELON_RIGHT");
            const double leftSign = type == QStringLiteral("ECHELON_LEFT") ? -1.0 : 1.0;
            const double diagonal = type == QStringLiteral("WEDGE") ? 0.75 : 1.0;
            for (int index = 1; index < count; ++index)
            {
                const int rank = (index + 1) / 2;
                const double side = index % 2 == 1 ? 1.0 : -1.0;
                double x = side * rank * spacing * diagonal;
                double y = (inverted ? 1.0 : -1.0) * rank * spacing * diagonal;
                if (echelon)
                {
                    x = leftSign * index * spacing;
                    y = -index * spacing;
                }
                offsets.push_back({x, y});
            }
        }
        else if (type == QStringLiteral("DIAMOND"))
        {
            const QVector<FormationOffset> diamond = {
                {0.0, -spacing}, {-spacing, 0.0}, {spacing, 0.0},
                {0.0, -2.0 * spacing}, {-2.0 * spacing, -spacing},
                {2.0 * spacing, -spacing}, {-spacing, spacing},
                {spacing, spacing}};
            for (int index = 1; index < count; ++index)
            {
                if (index - 1 < diamond.size())
                    offsets.push_back(diamond.at(index - 1));
                else
                {
                    const int layer = (index - 1 - diamond.size()) / 4 + 3;
                    const int arm = (index - 1 - diamond.size()) % 4;
                    const double extent = layer * spacing;
                    const FormationOffset extra[4] = {
                        {0.0, -extent}, {-extent, 0.0},
                        {extent, 0.0}, {0.0, extent}};
                    offsets.push_back(extra[arm]);
                }
            }
        }
        else if (type == QStringLiteral("CIRCULAR"))
        {
            const int followerCount = count - 1;
            if (followerCount > 0)
            {
                const double radius = spacing /
                                      (2.0 * qSin(M_PI / qMax(3, followerCount)));
                for (int index = 0; index < followerCount; ++index)
                {
                    const double angle = 2.0 * M_PI * index / followerCount;
                    offsets.push_back({radius * qSin(angle), radius * qCos(angle)});
                }
            }
        }
        else
        {
            for (int index = 1; index < count; ++index)
            {
                const int rank = (index + 1) / 2;
                const double side = index % 2 == 1 ? 1.0 : -1.0;
                offsets.push_back({side * rank * spacing,
                                   -((index + 1) / 2) * spacing});
            }
        }
        return offsets;
    }
}

QStringList FormationGenerator::supportedTypes()
{
    return {QStringLiteral("V"), QStringLiteral("INVERTED_V"),
            QStringLiteral("LINE"), QStringLiteral("COLUMN"),
            QStringLiteral("DIAMOND"), QStringLiteral("WEDGE"),
            QStringLiteral("ECHELON_LEFT"), QStringLiteral("ECHELON_RIGHT"),
            QStringLiteral("CIRCULAR"), QStringLiteral("CUSTOM")};
}

QString FormationGenerator::canonicalType(const QString &type)
{
    QString normalized = type.trimmed().toUpper();
    normalized.replace(QLatin1Char(' '), QLatin1Char('_'));
    if (normalized == QStringLiteral("HORIZONTAL") ||
        normalized == QStringLiteral("HORIZONTAL_LINE"))
        normalized = QStringLiteral("LINE");
    if (normalized == QStringLiteral("INVERTED_VEE") ||
        normalized == QStringLiteral("INVERTED_V_FORMATION"))
        normalized = QStringLiteral("INVERTED_V");
    if (normalized == QStringLiteral("ECHELONLEFT"))
        normalized = QStringLiteral("ECHELON_LEFT");
    if (normalized == QStringLiteral("ECHELONRIGHT"))
        normalized = QStringLiteral("ECHELON_RIGHT");
    return supportedTypes().contains(normalized) ? normalized : QString();
}

bool FormationGenerator::generate(int count, const QString &type,
                                  const QGeoCoordinate &reference,
                                  double headingDegrees, double spacingNm,
                                  QVector<FormationSlot> *outputSlots,
                                  QString *errorMessage)
{
    if (!outputSlots)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("FORMATION OUTPUT IS REQUIRED");
        return false;
    }
    const QString canonical = canonicalType(type);
    if (count < 2 || count > 200)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("FORMATION COUNT MUST BE BETWEEN 2 AND 200");
        return false;
    }
    if (canonical.isEmpty())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("UNSUPPORTED FORMATION TYPE");
        return false;
    }
    if (!reference.isValid() || !std::isfinite(headingDegrees) ||
        !std::isfinite(spacingNm) || spacingNm < MinimumSpacingNm ||
        spacingNm > MaximumSpacingNm)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("FORMATION POSITION, HEADING, OR SPACING IS INVALID");
        return false;
    }

    const QVector<FormationOffset> offsets = makeOffsets(count, canonical, spacingNm);
    const QString formationId =
        QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString leaderSlotId =
        formationId + QStringLiteral("-slot-00");
    QVector<FormationSlot> generated;
    generated.reserve(count);
    for (int index = 0; index < offsets.size(); ++index)
    {
        const QGeoCoordinate coordinate =
            positionAt(reference, offsets.at(index), headingDegrees);
        if (!coordinate.isValid() || !std::isfinite(coordinate.latitude()) ||
            !std::isfinite(coordinate.longitude()))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("FORMATION SLOT IS OUTSIDE VALID COORDINATES");
            return false;
        }
        for (const FormationSlot &existing : generated)
        {
            if (coordinate.distanceTo(existing.coordinate) < spacingNm * 1852.0 * 0.5)
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("FORMATION SLOTS OVERLAP");
                return false;
            }
        }
        const QString slotId =
            formationId + QStringLiteral("-slot-%1")
                              .arg(index, 2, 10, QLatin1Char('0'));
        generated.push_back({formationId, leaderSlotId, slotId, index,
                             index == 0, offsets.at(index), coordinate});
    }
    *outputSlots = std::move(generated);
    if (errorMessage)
        errorMessage->clear();
    return true;
}
