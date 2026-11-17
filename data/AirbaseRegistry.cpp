#include "AirbaseRegistry.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSet>

namespace
{
    QString normalized(const QString &value)
    {
        QString result = value.trimmed().toLower();
        result.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
        return result;
    }
}

AirbaseRegistry::AirbaseRegistry(QObject *parent)
    : QAbstractListModel(parent)
{
    QFile file(QStringLiteral(":/AeroSim/assets/airbases/airbases.json"));
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "Could not open airbase registry" << file.errorString();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray())
    {
        qWarning() << "Could not parse airbase registry" << parseError.errorString();
        return;
    }
    QSet<QString> ids;
    for (const QJsonValue &value : document.array())
    {
        const QJsonObject object = value.toObject();
        AirbaseRecord record;
        record.id = object.value(QStringLiteral("id")).toString();
        record.name = object.value(QStringLiteral("name")).toString();
        for (const QJsonValue &alias : object.value(QStringLiteral("aliases")).toArray())
            record.aliases << alias.toString();
        const QJsonValue latitude = object.value(QStringLiteral("latitude"));
        const QJsonValue longitude = object.value(QStringLiteral("longitude"));
        record.coordinateAvailable = latitude.isDouble() && longitude.isDouble();
        if (record.coordinateAvailable)
            record.coordinate = QGeoCoordinate(latitude.toDouble(), longitude.toDouble());
        record.stateOrUnionTerritory = object.value(QStringLiteral("stateOrUnionTerritory")).toString();
        record.region = object.value(QStringLiteral("region")).toString();
        record.command = object.value(QStringLiteral("command")).toString();
        record.commandStatus = object.value(QStringLiteral("commandStatus")).toString();
        record.category = object.value(QStringLiteral("category")).toString();
        record.coordinateSource = object.value(QStringLiteral("coordinateSource")).toString();
        record.verificationStatus = object.value(QStringLiteral("verificationStatus")).toString();
        if (!record.id.isEmpty() && !record.name.isEmpty() && !ids.contains(record.id))
        {
            ids.insert(record.id);
            m_records.push_back(std::move(record));
        }
    }
}

int AirbaseRegistry::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_records.size();
}

QVariant AirbaseRegistry::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_records.size())
        return {};
    const AirbaseRecord &record = m_records.at(index.row());
    switch (role)
    {
    case IdRole:
        return record.id;
    case NameRole:
        return record.name;
    case AliasesRole:
        return record.aliases;
    case LatitudeRole:
        return record.coordinate.latitude();
    case LongitudeRole:
        return record.coordinateAvailable ? record.coordinate.longitude() : QVariant();
    case CoordinateAvailableRole:
        return record.coordinateAvailable;
    case StateRole:
        return record.stateOrUnionTerritory;
    case RegionRole:
        return record.region;
    case CommandRole:
        return record.command;
    case CommandStatusRole:
        return record.commandStatus;
    case CategoryRole:
        return record.category;
    case CoordinateSourceRole:
        return record.coordinateSource;
    case VerificationStatusRole:
        return record.verificationStatus;
    default:
        return {};
    }
}

QHash<int, QByteArray> AirbaseRegistry::roleNames() const
{
    return {{IdRole, "airbaseId"}, {NameRole, "airbaseName"}, {AliasesRole, "aliases"}, {LatitudeRole, "latitude"}, {LongitudeRole, "longitude"}, {CoordinateAvailableRole, "coordinateAvailable"}, {StateRole, "stateOrUnionTerritory"}, {RegionRole, "region"}, {CommandRole, "command"}, {CommandStatusRole, "commandStatus"}, {CategoryRole, "category"}, {CoordinateSourceRole, "coordinateSource"}, {VerificationStatusRole, "verificationStatus"}};
}

const QVector<AirbaseRecord> &AirbaseRegistry::records() const { return m_records; }

const AirbaseRecord *AirbaseRegistry::byId(const QString &id) const
{
    for (const AirbaseRecord &record : m_records)
        if (record.id == id)
            return &record;
    return nullptr;
}

QVector<const AirbaseRecord *> AirbaseRegistry::matchingRegion(const QString &region) const
{
    QVector<const AirbaseRecord *> result;
    for (const AirbaseRecord &record : m_records)
        if (region.compare(QStringLiteral("India"), Qt::CaseInsensitive) == 0 ||
            record.region.compare(region, Qt::CaseInsensitive) == 0)
        {
            if (record.coordinateAvailable &&
                record.verificationStatus == QStringLiteral("verified_public_reference"))
                result.push_back(&record);
        }
    return result;
}

QVector<const AirbaseRecord *> AirbaseRegistry::matchingText(const QString &text) const
{
    const QString query = normalized(text);
    QVector<const AirbaseRecord *> result;
    for (const AirbaseRecord &record : m_records)
    {
        if (normalized(record.name) == query || normalized(record.id) == query)
        {
            result.push_back(&record);
            continue;
        }
        for (const QString &alias : record.aliases)
        {
            if (normalized(alias) == query)
            {
                result.push_back(&record);
                break;
            }
        }
    }
    if (!result.isEmpty())
        return result;
    for (const AirbaseRecord &record : m_records)
    {
        if (normalized(record.name).contains(query))
            result.push_back(&record);
        else
            for (const QString &alias : record.aliases)
                if (normalized(alias).contains(query))
                {
                    result.push_back(&record);
                    break;
                }
    }
    return result;
}
