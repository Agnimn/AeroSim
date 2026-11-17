#pragma once

#include "../models/EntityModel.h"

#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QTimer>
#include <QVector>
#include <vector>

class SimulationEngine final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(EntityModel *entityModel READ entityModel CONSTANT)
    Q_PROPERTY(bool running READ running NOTIFY simulationStateChanged)
    Q_PROPERTY(double speedMultiplier READ speedMultiplier NOTIFY simulationStateChanged)
    Q_PROPERTY(double arrivalToleranceMeters READ arrivalToleranceMeters NOTIFY simulationStateChanged)
    Q_PROPERTY(double conflictZoneRadiusMeters READ conflictZoneRadiusMeters CONSTANT)

public:
    explicit SimulationEngine(QObject *parent = nullptr);

    EntityModel *entityModel();
    const EntityModel *entityModel() const;
    bool running() const;
    double speedMultiplier() const;
    double arrivalToleranceMeters() const;
    double simulatedSeconds() const;
    double conflictZoneRadiusMeters() const;
    void setScenario(const std::vector<Entity> &entities,
                     bool preserveExistingState = true);
    bool scheduleTakeoff(ScheduledTakeoff event, QString *errorMessage = nullptr);
    bool cancelScheduledTakeoff(const QString &aircraftId, QString *errorMessage = nullptr);
    void setScheduledTakeoffs(const QVector<ScheduledTakeoff> &events);
    const QVector<ScheduledTakeoff> &scheduledTakeoffs() const;
    QJsonObject runtimeStateToJson() const;
    bool restoreRuntimeState(const QJsonObject &runtimeState, QString *errorMessage = nullptr);
    // Test/integration hook: advances only while playing and applies speedMultiplier.
    bool advanceSimulationForTest(double elapsedWallSeconds);
    bool aircraftState(const QString &id, AircraftSimulationState *state) const;
    bool assignIndividualRoute(const QString &id, QVector<Waypoint> route,
                               QString *errorMessage = nullptr);
    bool startAircraft(const QString &id, QString *errorMessage = nullptr);
    bool pauseAircraft(const QString &id, QString *errorMessage = nullptr);
    bool resumeAircraft(const QString &id, QString *errorMessage = nullptr);
    bool stopAircraft(const QString &id, QString *errorMessage = nullptr);
    bool stopAllAircraft();
    bool stopFormation(const QString &formationId, QString *errorMessage = nullptr);
    bool resumeFormation(const QString &formationId, QString *errorMessage = nullptr);
    bool disbandFormation(const QString &formationId);
    void setArrivalToleranceMeters(double tolerance);

    Q_INVOKABLE bool play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void setSpeedMultiplier(double multiplier);

signals:
    void simulationStateChanged();

private:
    struct PairPrediction
    {
        bool unsafe = false;
        bool immediate = false;
        double horizontalDistanceNm = 0.0;
        double verticalDistanceFt = 0.0;
        double timeToClosestSeconds = 0.0;
        double latitude = 0.0;
        double longitude = 0.0;
    };

    void tick();
    bool updateAircraft(double deltaSeconds, bool individualOnly,
                        double movementTimeSeconds);
    void advanceIndividualStep(double simulatedSeconds);
    bool hasIndividualMovement() const;
    void ensureTimerRunning();
    void stopTimerIfIdle();
    void updateConflicts();
    void executeDueTakeoffs();
    void restoreScheduledLifecycle();
    void advanceSimulationStep(double simulatedSeconds, double elapsedWallSeconds);
    PairPrediction predictConflict(const AircraftSimulationState &first,
                                   const AircraftSimulationState &second) const;
    void updateModel();
    int entityIndexForId(const QString &id) const;

    static double normalizeHeading(double heading);
    static double turnHeadingToward(double current, double target,
                                    double maximumChange);
    static double distanceNm(double latitude1, double longitude1,
                             double latitude2, double longitude2);
    static double bearingDegrees(double latitude1, double longitude1,
                                 double latitude2, double longitude2);
    static QGeoCoordinate destinationPoint(double latitude, double longitude,
                                           double bearing, double distanceNm);

    static constexpr double MinimumHorizontalSeparationNm = 5.0;
    static constexpr double MinimumVerticalSeparationFt = 1000.0;
    static constexpr double ConflictLookAheadSeconds = 180.0;
    static constexpr double ImmediateConflictSeconds = 30.0;
    static constexpr double AvoidanceHeadingOffsetDegrees = 40.0;
    static constexpr double AvoidanceDurationSeconds = 180.0;
    static constexpr double MaximumTurnRateDegreesPerSecond = 180.0;
    static constexpr int MaximumTrailPoints = 600;

    EntityModel m_entityModel;
    std::vector<Entity> m_entities;
    QVector<AircraftSimulationState> m_initialStates;
    QVector<AircraftSimulationState> m_states;
    QHash<QString, int> m_entityRows;
    QTimer m_timer;
    QElapsedTimer m_elapsedTimer;
    QSet<QString> m_activeConflictPairs;
    QSet<QString> m_avoidancePairs;
    double m_simulatedSeconds = 0.0;
    double m_individualSeconds = 0.0;
    double m_conflictElapsedSeconds = 0.0;
    double m_speedMultiplier = 1.0;
    double m_arrivalToleranceMeters = 1000.0;
    QVector<ScheduledTakeoff> m_scheduledTakeoffs;
    QVector<ScheduledTakeoff> m_initialScheduledTakeoffs;
    bool m_trailsChanged = true;
    bool m_globalRunning = false;
};
