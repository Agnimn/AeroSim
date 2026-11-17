#pragma once

#include <QObject>

class TerrainData final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(double minimumElevationMeters READ minimumElevationMeters CONSTANT)
    Q_PROPERTY(double maximumElevationMeters READ maximumElevationMeters CONSTANT)
    Q_PROPERTY(int width READ width CONSTANT)
    Q_PROPERTY(int height READ height CONSTANT)
    Q_PROPERTY(double minimumLatitude READ minimumLatitude CONSTANT)
    Q_PROPERTY(double maximumLatitude READ maximumLatitude CONSTANT)
    Q_PROPERTY(double minimumLongitude READ minimumLongitude CONSTANT)
    Q_PROPERTY(double maximumLongitude READ maximumLongitude CONSTANT)

public:
    explicit TerrainData(QObject *parent = nullptr);

    bool available() const;
    double minimumElevationMeters() const;
    double maximumElevationMeters() const;
    int width() const;
    int height() const;
    double minimumLatitude() const;
    double maximumLatitude() const;
    double minimumLongitude() const;
    double maximumLongitude() const;

private:
    bool m_available = false;
    double m_minimumElevationMeters = 0.0;
    double m_maximumElevationMeters = 0.0;
    int m_width = 0;
    int m_height = 0;
    double m_minimumLatitude = 0.0;
    double m_maximumLatitude = 0.0;
    double m_minimumLongitude = 0.0;
    double m_maximumLongitude = 0.0;
};
