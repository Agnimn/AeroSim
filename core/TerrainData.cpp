#include "TerrainData.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

TerrainData::TerrainData(QObject *parent)
    : QObject(parent)
{
    QFile metadataFile(QStringLiteral(":/AeroSim/assets/terrain/north_india_dem.json"));
    QFile heightmapFile(QStringLiteral(":/AeroSim/assets/terrain/north_india_dem.png"));
    if (!metadataFile.open(QIODevice::ReadOnly) || !heightmapFile.exists())
        return;

    const QJsonDocument metadata = QJsonDocument::fromJson(metadataFile.readAll());
    if (!metadata.isObject())
        return;

    const QJsonObject object = metadata.object();
    const double minimumElevation = object.value(QStringLiteral("minimum_elevation_meters")).toDouble();
    const double maximumElevation = object.value(QStringLiteral("maximum_elevation_meters")).toDouble();
    const int rasterWidth = object.value(QStringLiteral("width")).toInt();
    const int rasterHeight = object.value(QStringLiteral("height")).toInt();
    const QJsonArray bounds = object.value(QStringLiteral("bbox_wgs84")).toArray();
    const QString crs = object.value(QStringLiteral("target_crs")).toString();
    const QString verticalUnits = object.value(QStringLiteral("vertical_units")).toString();
    if (!qFuzzyCompare(minimumElevation + 1.0, minimumElevation + 1.0) ||
        !qFuzzyCompare(maximumElevation + 1.0, maximumElevation + 1.0) ||
        maximumElevation <= minimumElevation || rasterWidth < 2 || rasterHeight < 2 ||
        bounds.size() != 4 ||
        !(bounds.at(0).toDouble() < bounds.at(2).toDouble()) ||
        !(bounds.at(1).toDouble() < bounds.at(3).toDouble()) ||
        crs != QStringLiteral("EPSG:4326") || verticalUnits != QStringLiteral("meters"))
        return;

    m_minimumElevationMeters = minimumElevation;
    m_maximumElevationMeters = maximumElevation;
    m_width = rasterWidth;
    m_height = rasterHeight;
    m_minimumLongitude = bounds.at(0).toDouble();
    m_minimumLatitude = bounds.at(1).toDouble();
    m_maximumLongitude = bounds.at(2).toDouble();
    m_maximumLatitude = bounds.at(3).toDouble();
    m_available = true;
}

bool TerrainData::available() const { return m_available; }
double TerrainData::minimumElevationMeters() const { return m_minimumElevationMeters; }
double TerrainData::maximumElevationMeters() const { return m_maximumElevationMeters; }
int TerrainData::width() const { return m_width; }
int TerrainData::height() const { return m_height; }
double TerrainData::minimumLatitude() const { return m_minimumLatitude; }
double TerrainData::maximumLatitude() const { return m_maximumLatitude; }
double TerrainData::minimumLongitude() const { return m_minimumLongitude; }
double TerrainData::maximumLongitude() const { return m_maximumLongitude; }
