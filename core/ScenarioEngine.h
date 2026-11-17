#pragma once

#include "Scenario.h"
#include "CommandHistoryEntry.h"
#include "../ai/AIParser.h"
#include "SimulationEngine.h"
#include "../data/AirbaseRegistry.h"
#include "../data/ScenarioRepository.h"
#include "../ai/OllamaClient.h"

#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QVariantMap>
#include <vector>

class ScenarioEngine final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(EntityModel *entityModel READ entityModel CONSTANT)
    Q_PROPERTY(AirbaseRegistry *airbaseModel READ airbaseModel CONSTANT)
    Q_PROPERTY(QString scenarioName READ scenarioName NOTIFY scenarioChanged)
    Q_PROPERTY(QString region READ region NOTIFY scenarioChanged)
    Q_PROPERTY(QString entityType READ entityType NOTIFY scenarioChanged)
    Q_PROPERTY(int entityCount READ entityCount NOTIFY scenarioChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool scenarioGenerated READ scenarioGenerated NOTIFY scenarioGeneratedChanged)
    Q_PROPERTY(double centerLatitude READ centerLatitude NOTIFY scenarioChanged)
    Q_PROPERTY(double centerLongitude READ centerLongitude NOTIFY scenarioChanged)
    Q_PROPERTY(QString selectedEntityId READ selectedEntityId NOTIFY selectedEntityChanged)
    Q_PROPERTY(QString selectedEntityName READ selectedEntityName NOTIFY selectedEntityChanged)
    Q_PROPERTY(QString selectedAirbaseId READ selectedAirbaseId NOTIFY selectedAirbaseChanged)
    Q_PROPERTY(QString selectedAirbaseName READ selectedAirbaseName NOTIFY selectedAirbaseChanged)
    Q_PROPERTY(QString selectedAirbaseState READ selectedAirbaseState NOTIFY selectedAirbaseChanged)
    Q_PROPERTY(QString selectedAirbaseCategory READ selectedAirbaseCategory NOTIFY selectedAirbaseChanged)
    Q_PROPERTY(QString selectedAirbaseRegion READ selectedAirbaseRegion NOTIFY selectedAirbaseChanged)
    Q_PROPERTY(QString selectedAirbaseVerificationStatus READ selectedAirbaseVerificationStatus NOTIFY selectedAirbaseChanged)
    Q_PROPERTY(QString airbaseDisplayRegion READ airbaseDisplayRegion NOTIFY scenarioChanged)
    Q_PROPERTY(double selectedAircraftLatitude READ selectedAircraftLatitude NOTIFY simulationStateChanged)
    Q_PROPERTY(double selectedAircraftLongitude READ selectedAircraftLongitude NOTIFY simulationStateChanged)
    Q_PROPERTY(QString selectedAircraftCallsign READ selectedAircraftCallsign NOTIFY simulationStateChanged)
    Q_PROPERTY(double selectedAircraftAltitude READ selectedAircraftAltitude NOTIFY simulationStateChanged)
    Q_PROPERTY(double selectedAircraftSpeed READ selectedAircraftSpeed NOTIFY simulationStateChanged)
    Q_PROPERTY(double selectedAircraftHeading READ selectedAircraftHeading NOTIFY simulationStateChanged)
    Q_PROPERTY(double selectedAircraftVerticalSpeed READ selectedAircraftVerticalSpeed NOTIFY simulationStateChanged)
    Q_PROPERTY(QString selectedAircraftCurrentWaypoint READ selectedAircraftCurrentWaypoint NOTIFY simulationStateChanged)
    Q_PROPERTY(QString selectedAircraftNextWaypoint READ selectedAircraftNextWaypoint NOTIFY simulationStateChanged)
    Q_PROPERTY(QString selectedAircraftStatus READ selectedAircraftStatus NOTIFY simulationStateChanged)
    Q_PROPERTY(QString selectedAircraftConflictStatus READ selectedAircraftConflictStatus NOTIFY simulationStateChanged)
    Q_PROPERTY(QString selectedAircraftMovementStatus READ selectedAircraftMovementStatus NOTIFY simulationStateChanged)
    Q_PROPERTY(QString selectedAircraftDestination READ selectedAircraftDestination NOTIFY simulationStateChanged)
    Q_PROPERTY(bool simulationRunning READ simulationRunning NOTIFY simulationStateChanged)
    Q_PROPERTY(double simulationSpeed READ simulationSpeed NOTIFY simulationStateChanged)
    Q_PROPERTY(double simulationTime READ simulationTime NOTIFY simulationStateChanged)
    Q_PROPERTY(double arrivalToleranceMeters READ arrivalToleranceMeters NOTIFY simulationStateChanged)
    Q_PROPERTY(QVariantList scheduledTakeoffs READ scheduledTakeoffs NOTIFY scheduledTakeoffsChanged)
    Q_PROPERTY(double conflictZoneRadiusMeters READ conflictZoneRadiusMeters CONSTANT)
    Q_PROPERTY(bool aiRequestPending READ aiRequestPending NOTIFY aiRequestPendingChanged)
    Q_PROPERTY(int commandHistoryCount READ commandHistoryCount NOTIFY commandHistoryChanged)
    Q_PROPERTY(QVariantList routes READ routes NOTIFY routesChanged)
    Q_PROPERTY(QVariantList routeWaypoints READ routeWaypoints NOTIFY routesChanged)

public:
    explicit ScenarioEngine(QObject *parent = nullptr);

    EntityModel *entityModel();
    AirbaseRegistry *airbaseModel();
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
    QString selectedAirbaseId() const;
    QString selectedAirbaseName() const;
    QString selectedAirbaseState() const;
    QString selectedAirbaseCategory() const;
    QString selectedAirbaseRegion() const;
    QString selectedAirbaseVerificationStatus() const;
    QString airbaseDisplayRegion() const;
    double selectedAircraftLatitude() const;
    double selectedAircraftLongitude() const;
    QString selectedAircraftCallsign() const;
    double selectedAircraftAltitude() const;
    double selectedAircraftSpeed() const;
    double selectedAircraftHeading() const;
    double selectedAircraftVerticalSpeed() const;
    QString selectedAircraftCurrentWaypoint() const;
    QString selectedAircraftNextWaypoint() const;
    QString selectedAircraftStatus() const;
    QString selectedAircraftConflictStatus() const;
    QString selectedAircraftMovementStatus() const;
    QString selectedAircraftDestination() const;
    bool simulationRunning() const;
    double simulationSpeed() const;
    double simulationTime() const;
    double arrivalToleranceMeters() const;
    QVariantList scheduledTakeoffs() const;
    double conflictZoneRadiusMeters() const;
    bool aiRequestPending() const;
    int commandHistoryCount() const;
    QVector<CommandHistoryEntry> commandHistory() const;
    QVariantList routes() const;
    QVariantList routeWaypoints() const;
    Scenario snapshotForPersistence() const;

    Q_INVOKABLE bool generateScenario(const QString &prompt);
    Q_INVOKABLE bool generateScenarioWithAI(const QString &prompt);
    Q_INVOKABLE bool selectAircraft(const QString &entityId);
    Q_INVOKABLE bool moveSelectedAircraftToDestination(const QString &destination);
    Q_INVOKABLE bool startSelectedAircraft();
    Q_INVOKABLE bool pauseSelectedAircraft();
    Q_INVOKABLE bool resumeSelectedAircraft();
    Q_INVOKABLE bool stopSelectedAircraft();
    Q_INVOKABLE void setArrivalToleranceMeters(double tolerance);
    Q_INVOKABLE void clearSelectedAircraft();
    Q_INVOKABLE bool selectAirbase(const QString &airbaseId);
    Q_INVOKABLE void clearSelectedAirbase();
    Q_INVOKABLE bool playSimulation();
    Q_INVOKABLE void pauseSimulation();
    Q_INVOKABLE void resetSimulation();
    Q_INVOKABLE void setSimulationSpeed(double multiplier);
    Q_INVOKABLE bool advanceSimulationForTest(double simulatedSeconds);
    Q_INVOKABLE QVariantMap aircraftInformation(const QString &entityId) const;
    Q_INVOKABLE bool saveScenario(const QString &filePath = {});
    Q_INVOKABLE bool exportScenarioGeoJson(const QString &filePath);
    Q_INVOKABLE bool exportScenarioCsv(const QString &filePath);
    Q_INVOKABLE bool loadScenario(const QString &filePath);

signals:
    void scenarioChanged();
    void statusMessageChanged();
    void scenarioGeneratedChanged();
    void selectedEntityChanged();
    void selectedAirbaseChanged();
    void simulationStateChanged();
    void scheduledTakeoffsChanged();
    void aiRequestPendingChanged();
    void commandHistoryChanged();
    void routesChanged();

private:
    static QString displayType(const QString &type);
    static QString entityIdPrefix(const QString &type);
    static QString formatEntitySummary(const QStringList &types);
    static QString scenarioNameForRegion(const QString &region);
    static void regionCenter(const QString &region, double &latitude, double &longitude);
    static const std::vector<QPair<double, double>> &pointsForRegion(const QString &region);
    void setStatusMessage(const QString &message);
    void setScenarioGenerated(bool generated);
    void setAiRequestPending(bool pending);
    int recordCommandHistory(const QString &prompt, const QString &processingStatus);
    void updateCommandHistory(int sequenceNumber, const QString &processingStatus,
                              const QString &parsedOperation = {});
    bool executeScenarioPrompt(const QString &prompt, int commandSequence);
    void handleOllamaResponse(const QString &prompt, const QString &response);
    void handleOllamaFailure(const QString &prompt, const QString &message, bool unavailable);

    AirbaseRegistry m_airbaseRegistry;
    AIParser m_parser;
    OllamaClient m_ollamaClient;
    SimulationEngine m_simulationEngine;
    ScenarioRepository m_scenarioRepository;
    Scenario m_scenario;
    QString m_entityType;
    QString m_selectedEntityId;
    QString m_selectedEntityName;
    QString m_selectedAirbaseId;
    QString m_airbaseDisplayRegion;
    QString m_statusMessage = QStringLiteral("READY");
    double m_centerLatitude = 29.0;
    double m_centerLongitude = 79.0;
    bool m_scenarioGenerated = false;
    bool m_aiRequestPending = false;
    QString m_pendingAiPrompt;
    QVector<CommandHistoryEntry> m_commandHistory;
    int m_pendingCommandSequence = 0;
};
