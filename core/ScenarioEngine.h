#pragma once

#include "Scenario.h"
#include "../ai/AIParser.h"
#include "../models/EntityModel.h"

#include <QObject>
#include <QPair>
#include <QString>
#include <vector>

class ScenarioEngine final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(EntityModel *entityModel READ entityModel CONSTANT)
    Q_PROPERTY(QString scenarioName READ scenarioName NOTIFY scenarioChanged)
    Q_PROPERTY(QString region READ region NOTIFY scenarioChanged)
    Q_PROPERTY(QString entityType READ entityType NOTIFY scenarioChanged)
    Q_PROPERTY(int entityCount READ entityCount NOTIFY scenarioChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool scenarioGenerated READ scenarioGenerated NOTIFY scenarioChanged)
    Q_PROPERTY(double centerLatitude READ centerLatitude NOTIFY scenarioChanged)
    Q_PROPERTY(double centerLongitude READ centerLongitude NOTIFY scenarioChanged)
    Q_PROPERTY(QString selectedEntityId READ selectedEntityId NOTIFY selectedEntityChanged)
    Q_PROPERTY(QString selectedEntityName READ selectedEntityName NOTIFY selectedEntityChanged)

public:
    explicit ScenarioEngine(QObject *parent = nullptr);

    EntityModel *entityModel();
    QString scenarioName() const;
    QString region() const;
    QString entityType() const;
    int entityCount() const;
    QString statusMessage() const;
    bool scenarioGenerated() const;
    double centerLatitude() const;
    double centerLongitude() const;
    const Scenario &scenario() const;
    QString selectedEntityId() const;
    QString selectedEntityName() const;

    Q_INVOKABLE bool generateScenario(const QString &prompt);
    Q_INVOKABLE bool selectAircraft(const QString &entityId);
    Q_INVOKABLE void clearSelectedAircraft();

signals:
    void scenarioChanged();
    void statusMessageChanged();
    void selectedEntityChanged();

private:
    static QString displayType(const QString &type);
    static QString scenarioNameForRegion(const QString &region);
    static void regionCenter(const QString &region, double &latitude, double &longitude);
    static const std::vector<QPair<double, double>> &pointsForRegion(const QString &region);
    void setStatusMessage(const QString &message);

    AIParser m_parser;
    EntityModel m_entityModel;
    Scenario m_scenario;
    QString m_entityType;
    QString m_selectedEntityId;
    QString m_selectedEntityName;
    QString m_statusMessage = QStringLiteral("READY");
    double m_centerLatitude = 29.0;
    double m_centerLongitude = 79.0;
    bool m_scenarioGenerated = false;
};
