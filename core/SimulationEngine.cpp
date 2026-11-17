#include "SimulationEngine.h"
#include "FormationController.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonValue>
#include <QStringList>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>

namespace
{
    constexpr double EarthRadiusNm = 3440.065;
    constexpr double FeetPerMinuteToFeetPerSecond = 1.0 / 60.0;
    constexpr double KnotsToNmPerSecond = 1.0 / 3600.0;
    constexpr double PositionEpsilonNm = 0.02;
    constexpr double TrailRecordIntervalSeconds = 1.0;

    QString conflictPairKey(const QString &firstId, const QString &secondId)
    {
        return firstId < secondId
                   ? firstId + QLatin1Char('|') + secondId
                   : secondId + QLatin1Char('|') + firstId;
    }

    double radians(double degrees)
    {
        return qDegreesToRadians(degrees);
    }
}

SimulationEngine::SimulationEngine(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(33);
    connect(&m_timer, &QTimer::timeout, this, &SimulationEngine::tick);
}

EntityModel *SimulationEngine::entityModel()
{
    return &m_entityModel;
}

const EntityModel *SimulationEngine::entityModel() const
{
    return &m_entityModel;
}

bool SimulationEngine::running() const
{
    return m_globalRunning;
}

double SimulationEngine::speedMultiplier() const
{
    return m_speedMultiplier;
}

double SimulationEngine::arrivalToleranceMeters() const
{
    return m_arrivalToleranceMeters;
}

double SimulationEngine::simulatedSeconds() const
{
    return m_simulatedSeconds;
}

bool SimulationEngine::scheduleTakeoff(ScheduledTakeoff event, QString *errorMessage)
{
    if (event.aircraftId.isEmpty() || event.scheduledSeconds < m_simulatedSeconds)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("TAKE-OFF TIME MUST NOT BE IN THE PAST");
        return false;
    }
    const int entityIndex = entityIndexForId(event.aircraftId);
    if (entityIndex < 0 || m_entities.at(static_cast<std::size_t>(entityIndex)).type() != QStringLiteral("aircraft"))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("AIRCRAFT WAS NOT FOUND");
        return false;
    }
    if (m_entities.at(static_cast<std::size_t>(entityIndex)).route().size() < 2)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("AIRCRAFT HAS NO VALID ROUTE");
        return false;
    }
    for (const ScheduledTakeoff &existing : m_scheduledTakeoffs)
        if (existing.aircraftId == event.aircraftId && !existing.cancelled && !existing.executed)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("AIRCRAFT ALREADY HAS A PENDING TAKE-OFF");
            return false;
        }

    event.executed = false;
    event.cancelled = false;
    m_scheduledTakeoffs.push_back(event);
    m_initialScheduledTakeoffs.push_back(event);
    qInfo().noquote() << "[SCHEDULE] aircraft=" << event.aircraftId
                      << "time=" << event.scheduledSeconds
                      << "origin=" << event.originAirbaseName
                      << "destination=" << event.destinationName;
    for (AircraftSimulationState &state : m_states)
        if (state.id == event.aircraftId)
        {
            state.lifecycle = AircraftLifecycle::Scheduled;
            state.status = QStringLiteral("SCHEDULED");
            break;
        }
    updateModel();
    if (errorMessage)
        errorMessage->clear();
    emit simulationStateChanged();
    return true;
}

bool SimulationEngine::cancelScheduledTakeoff(const QString &aircraftId, QString *errorMessage)
{
    for (ScheduledTakeoff &event : m_scheduledTakeoffs)
        if (event.aircraftId == aircraftId && !event.executed && !event.cancelled)
        {
            event.cancelled = true;
            qInfo().noquote() << "[SCHEDULE] cancelled aircraft=" << aircraftId;
            for (AircraftSimulationState &state : m_states)
                if (state.id == aircraftId)
                {
                    state.lifecycle = AircraftLifecycle::Cancelled;
                    state.status = QStringLiteral("CANCELLED");
                    break;
                }
            updateModel();
            if (errorMessage)
                errorMessage->clear();
            emit simulationStateChanged();
            return true;
        }
    if (errorMessage)
        *errorMessage = QStringLiteral("NO PENDING TAKE-OFF FOUND");
    return false;
}

void SimulationEngine::setScheduledTakeoffs(const QVector<ScheduledTakeoff> &events)
{
    m_scheduledTakeoffs = events;
    m_initialScheduledTakeoffs = events;
    for (ScheduledTakeoff &event : m_initialScheduledTakeoffs)
    {
        event.executed = false;
        event.cancelled = false;
    }
    restoreScheduledLifecycle();
    updateModel();
    emit simulationStateChanged();
}

const QVector<ScheduledTakeoff> &SimulationEngine::scheduledTakeoffs() const
{
    return m_scheduledTakeoffs;
}

QJsonObject SimulationEngine::runtimeStateToJson() const
{
    QJsonArray aircraft;
    for (const AircraftSimulationState &state : m_states)
    {
        aircraft.append(QJsonObject{
            {QStringLiteral("id"), state.id},
            {QStringLiteral("latitude"), state.latitude},
            {QStringLiteral("longitude"), state.longitude},
            {QStringLiteral("altitude"), state.altitude},
            {QStringLiteral("speed"), state.speed},
            {QStringLiteral("heading"), state.heading},
            {QStringLiteral("verticalSpeed"), state.verticalSpeed},
            {QStringLiteral("currentWaypoint"), state.currentWaypoint},
            {QStringLiteral("status"), state.status},
            {QStringLiteral("movementStatus"), state.movementStatus},
            {QStringLiteral("individuallyControlled"), state.individuallyControlled},
            {QStringLiteral("lifecycle"), aircraftLifecycleName(state.lifecycle)}});
    }
    return {
        {QStringLiteral("simulationTime"), m_simulatedSeconds},
        {QStringLiteral("aircraft"), aircraft}};
}

bool SimulationEngine::restoreRuntimeState(const QJsonObject &runtimeState,
                                           QString *errorMessage)
{
    const QJsonValue timeValue = runtimeState.value(QStringLiteral("simulationTime"));
    const QJsonValue aircraftValue = runtimeState.value(QStringLiteral("aircraft"));
    if (!timeValue.isDouble() || !aircraftValue.isArray())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("RUNTIME STATE IS INCOMPLETE");
        return false;
    }

    const QJsonArray aircraft = aircraftValue.toArray();
    if (aircraft.size() != m_states.size())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("RUNTIME STATE AIRCRAFT COUNT DOES NOT MATCH SCENARIO");
        return false;
    }

    QVector<AircraftSimulationState> restored = m_states;
    QSet<QString> restoredIds;
    const double restoredTime = timeValue.toDouble(-1.0);
    if (!std::isfinite(restoredTime) || restoredTime < 0.0)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("RUNTIME SIMULATION TIME IS INVALID");
        return false;
    }

    for (const QJsonValue &value : aircraft)
    {
        if (!value.isObject())
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("RUNTIME AIRCRAFT STATE IS INVALID");
            return false;
        }
        const QJsonObject object = value.toObject();
        const QString id = object.value(QStringLiteral("id")).toString();
        int stateIndex = -1;
        for (int index = 0; index < restored.size(); ++index)
            if (restored.at(index).id == id)
                stateIndex = index;
        if (id.isEmpty() || stateIndex < 0 || restoredIds.contains(id))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("RUNTIME AIRCRAFT ID IS INVALID");
            return false;
        }

        const QString lifecycleName = object.value(QStringLiteral("lifecycle")).toString();
        AircraftLifecycle lifecycle;
        if (lifecycleName == QStringLiteral("Scheduled"))
            lifecycle = AircraftLifecycle::Scheduled;
        else if (lifecycleName == QStringLiteral("ReadyForDeparture"))
            lifecycle = AircraftLifecycle::ReadyForDeparture;
        else if (lifecycleName == QStringLiteral("Airborne"))
            lifecycle = AircraftLifecycle::Airborne;
        else if (lifecycleName == QStringLiteral("Landed"))
            lifecycle = AircraftLifecycle::Landed;
        else if (lifecycleName == QStringLiteral("Cancelled"))
            lifecycle = AircraftLifecycle::Cancelled;
        else
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("RUNTIME AIRCRAFT LIFECYCLE IS INVALID");
            return false;
        }

        const QStringList numericFields = {
            QStringLiteral("latitude"), QStringLiteral("longitude"),
            QStringLiteral("altitude"), QStringLiteral("speed"),
            QStringLiteral("heading"), QStringLiteral("verticalSpeed")};
        for (const QString &field : numericFields)
        {
            const QJsonValue fieldValue = object.value(field);
            if (!fieldValue.isDouble() || !std::isfinite(fieldValue.toDouble()))
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("RUNTIME AIRCRAFT NUMERIC STATE IS INVALID");
                return false;
            }
        }
        const int currentWaypoint = object.value(QStringLiteral("currentWaypoint")).toInt(-1);
        const int entityIndex = entityIndexForId(id);
        const int routeSize = entityIndex >= 0
                                  ? m_entities.at(static_cast<std::size_t>(entityIndex)).route().size()
                                  : 0;
        if (currentWaypoint < 0 || currentWaypoint > routeSize ||
            object.value(QStringLiteral("status")).toString().isEmpty())
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("RUNTIME AIRCRAFT PROGRESSION IS INVALID");
            return false;
        }

        AircraftSimulationState &state = restored[stateIndex];
        state.latitude = object.value(QStringLiteral("latitude")).toDouble();
        state.longitude = object.value(QStringLiteral("longitude")).toDouble();
        state.altitude = object.value(QStringLiteral("altitude")).toDouble();
        state.speed = object.value(QStringLiteral("speed")).toDouble();
        state.heading = object.value(QStringLiteral("heading")).toDouble();
        state.verticalSpeed = object.value(QStringLiteral("verticalSpeed")).toDouble();
        state.currentWaypoint = currentWaypoint;
        state.status = object.value(QStringLiteral("status")).toString();
        state.lifecycle = lifecycle;
        const QString savedMovementStatus =
            object.value(QStringLiteral("movementStatus")).toString(QStringLiteral("STATIONARY"));
        if (savedMovementStatus != QStringLiteral("STATIONARY") &&
            savedMovementStatus != QStringLiteral("MOVING") &&
            savedMovementStatus != QStringLiteral("PAUSED") &&
            savedMovementStatus != QStringLiteral("COMPLETED"))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("RUNTIME AIRCRAFT MOVEMENT STATUS IS INVALID");
            return false;
        }
        state.individuallyControlled =
            object.value(QStringLiteral("individuallyControlled")).toBool(false);
        state.movementStatus = savedMovementStatus == QStringLiteral("MOVING")
                                   ? QStringLiteral("PAUSED")
                                   : savedMovementStatus;
        if (savedMovementStatus == QStringLiteral("MOVING"))
            state.status = QStringLiteral("PAUSED");
        state.conflictStatus = QStringLiteral("NORMAL");
        state.conflictLatitude = state.latitude;
        state.conflictLongitude = state.longitude;
        state.avoidanceOffset = 0.0;
        state.avoidanceUntil = 0.0;
        state.trail.clear();
        state.trail.push_back(QGeoCoordinate(state.latitude, state.longitude));
        state.nextTrailTime = restoredTime + TrailRecordIntervalSeconds;
        restoredIds.insert(id);
    }

    m_states = std::move(restored);
    m_simulatedSeconds = restoredTime;
    m_individualSeconds = restoredTime;
    m_globalRunning = false;
    m_timer.stop();
    m_elapsedTimer.invalidate();
    m_conflictElapsedSeconds = 0.0;
    m_activeConflictPairs.clear();
    m_avoidancePairs.clear();
    m_trailsChanged = true;
    updateConflicts();
    updateModel();
    if (errorMessage)
        errorMessage->clear();
    emit simulationStateChanged();
    return true;
}

bool SimulationEngine::advanceSimulationForTest(double elapsedWallSeconds)
{
    if (!m_timer.isActive() || elapsedWallSeconds <= 0.0)
        return false;
    if (m_globalRunning)
        advanceSimulationStep(elapsedWallSeconds * m_speedMultiplier, elapsedWallSeconds);
    else
        advanceIndividualStep(elapsedWallSeconds * m_speedMultiplier);
    return true;
}

double SimulationEngine::conflictZoneRadiusMeters() const
{
    return MinimumHorizontalSeparationNm * 1852.0;
}

void SimulationEngine::setArrivalToleranceMeters(double tolerance)
{
    if (!std::isfinite(tolerance) || tolerance < 10.0 || tolerance > 50000.0 ||
        qFuzzyCompare(m_arrivalToleranceMeters, tolerance))
        return;
    m_arrivalToleranceMeters = tolerance;
    emit simulationStateChanged();
}

void SimulationEngine::setScenario(const std::vector<Entity> &entities,
                                   bool preserveExistingState)
{
    const bool preserveRuntime = preserveExistingState && !entities.empty();
    const bool wasGlobalRunning = preserveRuntime && m_globalRunning;
    const QVector<AircraftSimulationState> previousStates = m_states;
    const double previousSimulatedSeconds = m_simulatedSeconds;
    const double previousConflictElapsedSeconds = m_conflictElapsedSeconds;
    if (!preserveRuntime)
    {
        m_globalRunning = false;
        m_timer.stop();
        m_elapsedTimer.invalidate();
    }
    m_entities = entities;
    m_entityRows.clear();
    for (int index = 0; index < static_cast<int>(m_entities.size()); ++index)
        m_entityRows.insert(m_entities.at(static_cast<std::size_t>(index)).id(), index);
    m_entityModel.setEntitiesIncremental(m_entities);
    m_initialStates.clear();
    m_activeConflictPairs.clear();
    m_avoidancePairs.clear();
    m_simulatedSeconds = preserveRuntime ? previousSimulatedSeconds : 0.0;
    m_individualSeconds = preserveRuntime ? m_individualSeconds : 0.0;
    m_conflictElapsedSeconds = preserveRuntime ? previousConflictElapsedSeconds : 0.0;
    m_trailsChanged = true;

    for (const Entity &entity : m_entities)
    {
        if (entity.type() != QStringLiteral("aircraft"))
            continue;

        AircraftSimulationState state;
        state.id = entity.id();
        state.callsign = entity.callsign().isEmpty() ? entity.name() : entity.callsign();
        state.latitude = entity.latitude();
        state.longitude = entity.longitude();
        state.altitude = entity.altitude();
        state.speed = entity.speed();
        state.heading = entity.heading();
        state.verticalSpeed = entity.verticalSpeed();
        state.aircraftType = entity.aircraftType();
        state.trail.push_back(QGeoCoordinate(state.latitude, state.longitude));
        state.nextTrailTime = m_individualSeconds + TrailRecordIntervalSeconds;
        m_initialStates.push_back(state);
    }

    m_states = m_initialStates;
    if (preserveRuntime)
        for (AircraftSimulationState &state : m_states)
            for (const AircraftSimulationState &previous : previousStates)
                if (state.id == previous.id)
                {
                    state = previous;
                    break;
                }
    restoreScheduledLifecycle();
    updateModel();
    m_globalRunning = wasGlobalRunning;
    if (m_globalRunning || hasIndividualMovement())
        ensureTimerRunning();
    else
        stopTimerIfIdle();
    emit simulationStateChanged();
}

bool SimulationEngine::aircraftState(const QString &id, AircraftSimulationState *state) const
{
    for (const AircraftSimulationState &candidate : m_states)
    {
        if (candidate.id == id)
        {
            if (state)
                *state = candidate;
            return true;
        }
    }
    return false;
}

bool SimulationEngine::assignIndividualRoute(const QString &id, QVector<Waypoint> route,
                                             QString *errorMessage)
{
    const int entityIndex = entityIndexForId(id);
    auto stateIterator = std::find_if(
        m_states.begin(), m_states.end(),
        [&id](const AircraftSimulationState &state) { return state.id == id; });
    if (entityIndex < 0 || stateIterator == m_states.end() ||
        m_entities.at(static_cast<std::size_t>(entityIndex)).type() != QStringLiteral("aircraft"))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("AIRCRAFT WAS NOT FOUND");
        return false;
    }
    if (route.size() < 2)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("A MOVEMENT ROUTE REQUIRES A START AND DESTINATION");
        return false;
    }
    if (stateIterator->lifecycle == AircraftLifecycle::Scheduled ||
        stateIterator->lifecycle == AircraftLifecycle::Cancelled)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("AIRCRAFT IS NOT AVAILABLE FOR MOVEMENT");
        return false;
    }
    for (const Waypoint &waypoint : route)
    {
        const QGeoCoordinate coordinate(waypoint.latitude, waypoint.longitude);
        if (!coordinate.isValid() || !std::isfinite(waypoint.altitude))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("MOVEMENT ROUTE CONTAINS AN INVALID WAYPOINT");
            return false;
        }
    }

    route[0] = {QStringLiteral("CURRENT POSITION"), stateIterator->latitude,
                stateIterator->longitude, stateIterator->altitude};
    m_entities.at(static_cast<std::size_t>(entityIndex)).setRoute(std::move(route), false);
    const QVector<Waypoint> &assignedRoute =
        m_entities.at(static_cast<std::size_t>(entityIndex)).route();
    const Waypoint &destination = assignedRoute.last();
    const double distanceToDestination =
        distanceNm(stateIterator->latitude, stateIterator->longitude,
                   destination.latitude, destination.longitude);
    stateIterator->individuallyControlled = true;
    stateIterator->currentWaypoint = 1;
    stateIterator->verticalSpeed = 0.0;
    stateIterator->avoidanceOffset = 0.0;
    stateIterator->avoidanceUntil = 0.0;
    if (distanceToDestination * 1852.0 <= m_arrivalToleranceMeters)
    {
        stateIterator->latitude = destination.latitude;
        stateIterator->longitude = destination.longitude;
        stateIterator->altitude = destination.altitude;
        stateIterator->currentWaypoint = assignedRoute.size();
        stateIterator->lifecycle = AircraftLifecycle::Landed;
        stateIterator->movementStatus = QStringLiteral("COMPLETED");
        stateIterator->status = QStringLiteral("COMPLETED");
    }
    else
    {
        stateIterator->lifecycle = AircraftLifecycle::Airborne;
        stateIterator->movementStatus = QStringLiteral("MOVING");
        stateIterator->status = QStringLiteral("MOVING");
        ensureTimerRunning();
    }
    m_entityModel.setEntitiesIncremental(m_entities);
    m_trailsChanged = true;
    updateModel();
    stopTimerIfIdle();
    if (errorMessage)
        errorMessage->clear();
    emit simulationStateChanged();
    return true;
}

bool SimulationEngine::startAircraft(const QString &id, QString *errorMessage)
{
    auto stateIterator = std::find_if(
        m_states.begin(), m_states.end(),
        [&id](const AircraftSimulationState &state) { return state.id == id; });
    const int entityIndex = entityIndexForId(id);
    if (stateIterator == m_states.end() || entityIndex < 0 ||
        m_entities.at(static_cast<std::size_t>(entityIndex)).type() != QStringLiteral("aircraft"))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("AIRCRAFT WAS NOT FOUND");
        return false;
    }
    if (stateIterator->movementStatus == QStringLiteral("PAUSED"))
        return resumeAircraft(id, errorMessage);
    const QVector<Waypoint> &route =
        m_entities.at(static_cast<std::size_t>(entityIndex)).route();
    if (stateIterator->movementStatus == QStringLiteral("COMPLETED") ||
        route.size() < 2 || stateIterator->currentWaypoint >= route.size())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("AIRCRAFT HAS NO REMAINING ROUTE; ASSIGN A NEW DESTINATION");
        return false;
    }
    stateIterator->individuallyControlled = true;
    stateIterator->movementStatus = QStringLiteral("MOVING");
    stateIterator->status = QStringLiteral("MOVING");
    stateIterator->lifecycle = AircraftLifecycle::Airborne;
    ensureTimerRunning();
    updateModel();
    if (errorMessage)
        errorMessage->clear();
    emit simulationStateChanged();
    return true;
}

bool SimulationEngine::pauseAircraft(const QString &id, QString *errorMessage)
{
    auto stateIterator = std::find_if(
        m_states.begin(), m_states.end(),
        [&id](const AircraftSimulationState &state) { return state.id == id; });
    if (stateIterator == m_states.end())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("AIRCRAFT WAS NOT FOUND");
        return false;
    }
    if (stateIterator->movementStatus != QStringLiteral("MOVING"))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("AIRCRAFT IS NOT MOVING");
        return false;
    }
    stateIterator->movementStatus = QStringLiteral("PAUSED");
    stateIterator->status = QStringLiteral("PAUSED");
    stateIterator->verticalSpeed = 0.0;
    updateModel();
    stopTimerIfIdle();
    if (errorMessage)
        errorMessage->clear();
    emit simulationStateChanged();
    return true;
}

bool SimulationEngine::resumeAircraft(const QString &id, QString *errorMessage)
{
    auto stateIterator = std::find_if(
        m_states.begin(), m_states.end(),
        [&id](const AircraftSimulationState &state) { return state.id == id; });
    if (stateIterator == m_states.end())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("AIRCRAFT WAS NOT FOUND");
        return false;
    }
    if (stateIterator->movementStatus != QStringLiteral("PAUSED") ||
        stateIterator->currentWaypoint >=
            m_entities.at(static_cast<std::size_t>(entityIndexForId(id))).route().size())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("AIRCRAFT HAS NO PAUSED ROUTE TO RESUME");
        return false;
    }
    stateIterator->movementStatus = QStringLiteral("MOVING");
    stateIterator->status = QStringLiteral("MOVING");
    stateIterator->lifecycle = AircraftLifecycle::Airborne;
    ensureTimerRunning();
    updateModel();
    if (errorMessage)
        errorMessage->clear();
    emit simulationStateChanged();
    return true;
}

bool SimulationEngine::stopAircraft(const QString &id, QString *errorMessage)
{
    auto stateIterator = std::find_if(
        m_states.begin(), m_states.end(),
        [&id](const AircraftSimulationState &state) { return state.id == id; });
    if (stateIterator == m_states.end())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("AIRCRAFT WAS NOT FOUND");
        return false;
    }
    stateIterator->individuallyControlled = true;
    stateIterator->movementStatus = QStringLiteral("STATIONARY");
    stateIterator->status = QStringLiteral("STOPPED");
    stateIterator->verticalSpeed = 0.0;
    if (stateIterator->lifecycle == AircraftLifecycle::Landed)
        stateIterator->lifecycle = AircraftLifecycle::Airborne;
    updateModel();
    stopTimerIfIdle();
    if (errorMessage)
        errorMessage->clear();
    emit simulationStateChanged();
    return true;
}

bool SimulationEngine::stopAllAircraft()
{
    for (AircraftSimulationState &state : m_states)
    {
        state.individuallyControlled = true;
        state.movementStatus = QStringLiteral("STATIONARY");
        state.status = QStringLiteral("STOPPED");
        state.verticalSpeed = 0.0;
        if (state.lifecycle == AircraftLifecycle::Landed)
            state.lifecycle = AircraftLifecycle::Airborne;
    }
    updateModel();
    stopTimerIfIdle();
    emit simulationStateChanged();
    return true;
}

bool SimulationEngine::stopFormation(const QString &formationId,
                                     QString *errorMessage)
{
    QString leaderId;
    QStringList memberIds;
    for (const Entity &entity : m_entities)
    {
        if (!entity.isFormationMember() || entity.formationId() != formationId)
            continue;
        memberIds.push_back(entity.id());
        if (entity.id() == entity.formationLeaderId())
            leaderId = entity.id();
    }
    if (leaderId.isEmpty())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("FORMATION LEADER WAS NOT FOUND");
        return false;
    }
    if (!stopAircraft(leaderId, errorMessage))
        return false;

    for (AircraftSimulationState &state : m_states)
        if (memberIds.contains(state.id) && state.id != leaderId)
        {
            state.individuallyControlled = false;
            state.movementStatus = QStringLiteral("STATIONARY");
            state.status = QStringLiteral("STOPPED");
            state.verticalSpeed = 0.0;
        }
    updateModel();
    emit simulationStateChanged();
    if (errorMessage)
        errorMessage->clear();
    return true;
}

bool SimulationEngine::resumeFormation(const QString &formationId,
                                       QString *errorMessage)
{
    QString leaderId;
    QStringList memberIds;
    for (const Entity &entity : m_entities)
    {
        if (!entity.isFormationMember() || entity.formationId() != formationId)
            continue;
        memberIds.push_back(entity.id());
        if (entity.id() == entity.formationLeaderId())
            leaderId = entity.id();
    }
    if (leaderId.isEmpty())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("FORMATION LEADER WAS NOT FOUND");
        return false;
    }
    if (!startAircraft(leaderId, errorMessage))
        return false;

    for (AircraftSimulationState &state : m_states)
        if (memberIds.contains(state.id) && state.id != leaderId)
        {
            state.individuallyControlled = false;
            state.movementStatus = QStringLiteral("MOVING");
            state.status = QStringLiteral("FORMING");
            state.lifecycle = AircraftLifecycle::Airborne;
        }
    updateModel();
    emit simulationStateChanged();
    if (errorMessage)
        errorMessage->clear();
    return true;
}

bool SimulationEngine::disbandFormation(const QString &formationId)
{
    bool foundFormation = false;
    for (const Entity &entity : m_entities)
        foundFormation = foundFormation ||
                         (entity.isFormationMember() &&
                          entity.formationId() == formationId);
    if (!foundFormation)
        return false;

    QString leaderId;
    for (const Entity &entity : m_entities)
        if (entity.isFormationMember() && entity.formationId() == formationId &&
            entity.id() == entity.formationLeaderId())
            leaderId = entity.id();

    for (AircraftSimulationState &state : m_states)
        if (state.id != leaderId &&
            std::any_of(m_entities.cbegin(), m_entities.cend(),
                        [&state, &formationId](const Entity &entity)
                        {
                            return entity.id() == state.id &&
                                   entity.isFormationMember() &&
                                   entity.formationId() == formationId;
                        }))
        {
            state.individuallyControlled = false;
            state.movementStatus = QStringLiteral("STATIONARY");
            state.status = QStringLiteral("STOPPED");
            state.verticalSpeed = 0.0;
        }
    updateModel();
    stopTimerIfIdle();
    emit simulationStateChanged();
    return true;
}

bool SimulationEngine::play()
{
    if (m_states.isEmpty())
        return !m_states.isEmpty();
    if (m_globalRunning)
        return true;
    m_globalRunning = true;
    ensureTimerRunning();
    emit simulationStateChanged();
    return true;
}

void SimulationEngine::pause()
{
    if (!m_globalRunning)
        return;
    m_globalRunning = false;
    stopTimerIfIdle();
    emit simulationStateChanged();
}

void SimulationEngine::reset()
{
    m_globalRunning = false;
    m_timer.stop();
    m_elapsedTimer.invalidate();
    m_states = m_initialStates;
    m_scheduledTakeoffs = m_initialScheduledTakeoffs;
    restoreScheduledLifecycle();
    m_activeConflictPairs.clear();
    m_avoidancePairs.clear();
    m_simulatedSeconds = 0.0;
    m_individualSeconds = 0.0;
    m_conflictElapsedSeconds = 0.0;
    m_trailsChanged = true;
    updateModel();
    emit simulationStateChanged();
}

void SimulationEngine::setSpeedMultiplier(double multiplier)
{
    const double allowed[] = {0.5, 1.0, 2.0, 5.0, 10.0};
    const auto found = std::find(std::begin(allowed), std::end(allowed), multiplier);
    if (found == std::end(allowed) || qFuzzyCompare(m_speedMultiplier, multiplier))
        return;

    m_speedMultiplier = multiplier;
    emit simulationStateChanged();
}

void SimulationEngine::tick()
{
    const double elapsedSeconds = m_elapsedTimer.restart() / 1000.0;
    if (elapsedSeconds <= 0.0)
        return;

    if (m_globalRunning)
        advanceSimulationStep(qMin(elapsedSeconds, 0.25) * m_speedMultiplier,
                              elapsedSeconds);
    else
        advanceIndividualStep(elapsedSeconds * m_speedMultiplier);
}

void SimulationEngine::advanceSimulationStep(double simulatedSeconds, double elapsedWallSeconds)
{
    m_simulatedSeconds += simulatedSeconds;
    m_individualSeconds += elapsedWallSeconds * m_speedMultiplier;
    executeDueTakeoffs();
    m_trailsChanged = updateAircraft(simulatedSeconds, false, m_simulatedSeconds);
    m_trailsChanged = updateAircraft(elapsedWallSeconds * m_speedMultiplier,
                                     true, m_individualSeconds) ||
                      m_trailsChanged;
    m_trailsChanged =
        FormationController::updateFollowers(
            m_states, m_entities, simulatedSeconds,
            elapsedWallSeconds * m_speedMultiplier, m_simulatedSeconds) ||
        m_trailsChanged;

    m_conflictElapsedSeconds += elapsedWallSeconds;
    if (m_conflictElapsedSeconds >= 0.5)
    {
        m_conflictElapsedSeconds = 0.0;
        updateConflicts();
    }

    updateModel();
    emit simulationStateChanged();
    stopTimerIfIdle();
}

void SimulationEngine::advanceIndividualStep(double simulatedSeconds)
{
    m_individualSeconds += simulatedSeconds;
    m_trailsChanged = updateAircraft(simulatedSeconds, true, m_individualSeconds);
    m_trailsChanged =
        FormationController::updateFollowers(
            m_states, m_entities, simulatedSeconds, simulatedSeconds,
            m_individualSeconds) ||
        m_trailsChanged;
    updateModel();
    emit simulationStateChanged();
    stopTimerIfIdle();
}

void SimulationEngine::restoreScheduledLifecycle()
{
    for (const ScheduledTakeoff &event : m_scheduledTakeoffs)
    {
        for (AircraftSimulationState &state : m_states)
            if (state.id == event.aircraftId)
            {
                if (event.cancelled)
                {
                    state.lifecycle = AircraftLifecycle::Cancelled;
                    state.status = QStringLiteral("CANCELLED");
                }
                else if (!event.executed)
                {
                    state.lifecycle = AircraftLifecycle::Scheduled;
                    state.status = QStringLiteral("SCHEDULED");
                }
                break;
            }
    }
}

void SimulationEngine::executeDueTakeoffs()
{
    bool changed = false;
    for (ScheduledTakeoff &event : m_scheduledTakeoffs)
    {
        if (event.executed || event.cancelled || event.scheduledSeconds > m_simulatedSeconds)
            continue;
        event.executed = true;
        qInfo().noquote() << "[SCHEDULE] executing aircraft=" << event.aircraftId
                          << "at=" << m_simulatedSeconds
                          << "scheduled=" << event.scheduledSeconds;
        for (AircraftSimulationState &state : m_states)
            if (state.id == event.aircraftId)
            {
                state.lifecycle = AircraftLifecycle::Airborne;
                state.status = QStringLiteral("ACTIVE");
                if (state.currentWaypoint < 1)
                    state.currentWaypoint = 1;
                changed = true;
                break;
            }
    }
    if (changed)
        updateModel();
}

bool SimulationEngine::updateAircraft(double deltaSeconds, bool individualOnly,
                                      double movementTimeSeconds)
{
    bool trailsChanged = false;
    for (AircraftSimulationState &state : m_states)
    {
        if (individualOnly
                ? (!state.individuallyControlled ||
                   state.movementStatus != QStringLiteral("MOVING"))
                : state.individuallyControlled)
            continue;
        const int entityIndex = entityIndexForId(state.id);
        if (entityIndex < 0)
            continue;
        if (state.lifecycle != AircraftLifecycle::Airborne)
            continue;
        const Entity &entity = m_entities.at(static_cast<std::size_t>(entityIndex));
        const QVector<Waypoint> &route = entity.route();
        if (route.size() < 2 || state.currentWaypoint >= route.size())
        {
            state.status = QStringLiteral("HOLDING");
            state.verticalSpeed = 0.0;
            continue;
        }

        double remainingSeconds = deltaSeconds;
        while (remainingSeconds > 0.0001 && state.currentWaypoint < route.size())
        {
            const Waypoint &target = route.at(state.currentWaypoint);
            const double remainingDistance =
                distanceNm(state.latitude, state.longitude, target.latitude, target.longitude);
            const double waypointToleranceNm =
                state.individuallyControlled &&
                        state.currentWaypoint == route.size() - 1
                    ? m_arrivalToleranceMeters / 1852.0
                    : PositionEpsilonNm;
            if (remainingDistance <= waypointToleranceNm)
            {
                state.latitude = target.latitude;
                state.longitude = target.longitude;
                if (state.currentWaypoint == route.size() - 1 && entity.routeLoop())
                    state.currentWaypoint = 0;
                else
                    ++state.currentWaypoint;
                qInfo().noquote() << "[SIM]" << state.callsign << "reached waypoint"
                                  << target.name;

                if (state.currentWaypoint >= route.size())
                {
                    state.lifecycle = AircraftLifecycle::Landed;
                    if (state.individuallyControlled)
                    {
                        state.movementStatus = QStringLiteral("COMPLETED");
                        state.status = QStringLiteral("COMPLETED");
                    }
                    else
                    {
                        state.status = QStringLiteral("LANDED");
                    }
                    state.verticalSpeed = 0.0;
                    break;
                }
                continue;
            }

            const Waypoint &activeTarget = route.at(state.currentWaypoint);
            double course = bearingDegrees(state.latitude, state.longitude,
                                           activeTarget.latitude, activeTarget.longitude);
            if (state.avoidanceUntil > m_simulatedSeconds)
            {
                course = normalizeHeading(course + state.avoidanceOffset);
                state.status = QStringLiteral("AVOIDING");
            }
            else
            {
                if (state.avoidanceUntil > 0.0)
                {
                    state.avoidanceUntil = 0.0;
                    state.avoidanceOffset = 0.0;
                    state.status = QStringLiteral("ACTIVE");
                }
                else if (state.status != QStringLiteral("HOLDING"))
                {
                    state.status = QStringLiteral("ACTIVE");
                }
            }

            const double secondsToTarget = remainingDistance /
                                           qMax(state.speed, 1.0) * 3600.0;
            const bool followingAvoidance = state.avoidanceUntil > m_simulatedSeconds;
            const bool canReachWaypoint = !followingAvoidance &&
                                          remainingSeconds >= secondsToTarget;
            const double travelSeconds = canReachWaypoint ? secondsToTarget : remainingSeconds;
            state.heading = turnHeadingToward(
                state.heading, course,
                MaximumTurnRateDegreesPerSecond * travelSeconds);
            const double travelDistance = state.speed * travelSeconds * KnotsToNmPerSecond;
            if (canReachWaypoint)
            {
                state.latitude = activeTarget.latitude;
                state.longitude = activeTarget.longitude;
            }
            else
            {
                const QGeoCoordinate nextPosition =
                    destinationPoint(state.latitude, state.longitude, course, travelDistance);
                state.latitude = nextPosition.latitude();
                state.longitude = nextPosition.longitude();
            }
            remainingSeconds -= travelSeconds;

            const double altitudeDifference = activeTarget.altitude - state.altitude;
            if (qAbs(altitudeDifference) < 1.0)
            {
                state.altitude = activeTarget.altitude;
                state.verticalSpeed = 0.0;
            }
            else
            {
                state.verticalSpeed = altitudeDifference > 0.0 ? 1800.0 : -1800.0;
                const double altitudeStep =
                    state.verticalSpeed * FeetPerMinuteToFeetPerSecond * travelSeconds;
                if (qAbs(altitudeStep) >= qAbs(altitudeDifference))
                {
                    state.altitude = activeTarget.altitude;
                    state.verticalSpeed = 0.0;
                }
                else
                {
                    state.altitude += altitudeStep;
                }
            }

            if (canReachWaypoint ||
                distanceNm(state.latitude, state.longitude,
                           activeTarget.latitude, activeTarget.longitude) <= PositionEpsilonNm)
            {
                state.latitude = activeTarget.latitude;
                state.longitude = activeTarget.longitude;
                if (state.currentWaypoint == route.size() - 1 && entity.routeLoop())
                    state.currentWaypoint = 0;
                else
                    ++state.currentWaypoint;
                qInfo().noquote() << "[SIM]" << state.callsign << "reached waypoint"
                                  << activeTarget.name;

                if (state.currentWaypoint >= route.size())
                {
                    state.lifecycle = AircraftLifecycle::Landed;
                    if (state.individuallyControlled)
                    {
                        state.movementStatus = QStringLiteral("COMPLETED");
                        state.status = QStringLiteral("COMPLETED");
                    }
                    else
                    {
                        state.status = QStringLiteral("LANDED");
                    }
                    state.verticalSpeed = 0.0;
                    break;
                }
            }
        }

        if (movementTimeSeconds >= state.nextTrailTime)
        {
            state.trail.push_back(QGeoCoordinate(state.latitude, state.longitude));
            if (state.trail.size() > static_cast<int>(MaximumTrailPoints))
                state.trail.remove(0, state.trail.size() - static_cast<int>(MaximumTrailPoints));
            state.nextTrailTime = movementTimeSeconds + TrailRecordIntervalSeconds;
            trailsChanged = true;
        }
    }
    return trailsChanged;
}

bool SimulationEngine::hasIndividualMovement() const
{
    return std::any_of(m_states.cbegin(), m_states.cend(),
                       [](const AircraftSimulationState &state)
                       {
                           return state.individuallyControlled &&
                                  state.movementStatus == QStringLiteral("MOVING");
                       });
}

void SimulationEngine::ensureTimerRunning()
{
    if (m_timer.isActive())
        return;
    m_elapsedTimer.start();
    m_timer.start();
}

void SimulationEngine::stopTimerIfIdle()
{
    if (m_globalRunning || hasIndividualMovement() || !m_timer.isActive())
        return;
    m_timer.stop();
    m_elapsedTimer.invalidate();
}

void SimulationEngine::updateConflicts()
{
    QSet<QString> currentPairs;
    QStringList newConflictMessages;
    QStringList newAvoidanceMessages;
    QStringList resolvedMessages;
    for (AircraftSimulationState &state : m_states)
    {
        state.conflictStatus = QStringLiteral("NORMAL");
        state.conflictLatitude = state.latitude;
        state.conflictLongitude = state.longitude;
    }

    for (int firstIndex = 0; firstIndex < m_states.size(); ++firstIndex)
    {
        AircraftSimulationState &first = m_states[firstIndex];
        if (first.lifecycle != AircraftLifecycle::Airborne)
            continue;
        for (int secondIndex = firstIndex + 1; secondIndex < m_states.size(); ++secondIndex)
        {
            AircraftSimulationState &second = m_states[secondIndex];
            if (second.lifecycle != AircraftLifecycle::Airborne)
                continue;
            const PairPrediction prediction = predictConflict(first, second);
            if (!prediction.unsafe)
                continue;

            const QString key = conflictPairKey(first.id, second.id);
            currentPairs.insert(key);
            const QString severity =
                prediction.immediate ? QStringLiteral("CONFLICT") : QStringLiteral("CAUTION");
            if (first.conflictStatus != QStringLiteral("CONFLICT"))
                first.conflictStatus = severity;
            if (second.conflictStatus != QStringLiteral("CONFLICT"))
                second.conflictStatus = severity;
            first.conflictLatitude = second.conflictLatitude = prediction.latitude;
            first.conflictLongitude = second.conflictLongitude = prediction.longitude;

            if (!m_activeConflictPairs.contains(key))
            {
                newConflictMessages << QStringLiteral("%1 <-> %2 (%3 s)")
                                           .arg(first.callsign, second.callsign)
                                           .arg(qRound(prediction.timeToClosestSeconds));
            }

            if (!m_avoidancePairs.contains(key) &&
                first.avoidanceUntil <= m_simulatedSeconds &&
                second.avoidanceUntil <= m_simulatedSeconds)
            {
                AircraftSimulationState &maneuvering =
                    second.id > first.id ? second : first;
                const double sign = (maneuvering.id.toUtf8().back() % 2) == 0 ? 1.0 : -1.0;
                maneuvering.avoidanceOffset = sign * AvoidanceHeadingOffsetDegrees;
                maneuvering.avoidanceUntil = m_simulatedSeconds + AvoidanceDurationSeconds;
                maneuvering.status = QStringLiteral("AVOIDING");
                m_avoidancePairs.insert(key);
                newAvoidanceMessages << maneuvering.callsign;
            }
        }
    }

    for (const QString &key : std::as_const(m_activeConflictPairs))
    {
        if (!currentPairs.contains(key))
            resolvedMessages << key;
    }
    for (auto it = m_avoidancePairs.begin(); it != m_avoidancePairs.end();)
    {
        if (!currentPairs.contains(*it))
            it = m_avoidancePairs.erase(it);
        else
            ++it;
    }
    m_activeConflictPairs = std::move(currentPairs);

    constexpr qsizetype MaximumDetailedConflictLogs = 10;
    for (qsizetype index = 0; index < qMin(newConflictMessages.size(), MaximumDetailedConflictLogs); ++index)
        qInfo().noquote() << "[CONFLICT]" << newConflictMessages.at(index);
    if (newConflictMessages.size() > MaximumDetailedConflictLogs)
        qInfo().noquote() << "[CONFLICT]" << newConflictMessages.size()
                          << "new aircraft-pair conflicts detected";
    for (qsizetype index = 0; index < qMin(newAvoidanceMessages.size(), MaximumDetailedConflictLogs); ++index)
        qInfo().noquote() << "[AVOIDANCE]" << newAvoidanceMessages.at(index)
                          << "temporarily adjusting heading";
    if (newAvoidanceMessages.size() > MaximumDetailedConflictLogs)
        qInfo().noquote() << "[AVOIDANCE]" << newAvoidanceMessages.size()
                          << "aircraft assigned temporary heading adjustments";
    for (qsizetype index = 0; index < qMin(resolvedMessages.size(), MaximumDetailedConflictLogs); ++index)
        qInfo().noquote() << "[CONFLICT RESOLVED]" << resolvedMessages.at(index);
    if (resolvedMessages.size() > MaximumDetailedConflictLogs)
        qInfo().noquote() << "[CONFLICT RESOLVED]" << resolvedMessages.size()
                          << "aircraft-pair conflicts cleared";
}

SimulationEngine::PairPrediction SimulationEngine::predictConflict(
    const AircraftSimulationState &first, const AircraftSimulationState &second) const
{
    PairPrediction result;
    const double meanLatitude = radians((first.latitude + second.latitude) * 0.5);
    const double eastNm = (second.longitude - first.longitude) *
                          60.0 * qCos(meanLatitude);
    const double northNm = (second.latitude - first.latitude) * 60.0;

    const double firstHeading = radians(first.heading);
    const double secondHeading = radians(second.heading);
    const double firstGroundSpeed =
        first.status == QStringLiteral("HOLDING") ? 0.0 : first.speed;
    const double secondGroundSpeed =
        second.status == QStringLiteral("HOLDING") ? 0.0 : second.speed;
    const double firstEastSpeed = firstGroundSpeed * qSin(firstHeading);
    const double firstNorthSpeed = firstGroundSpeed * qCos(firstHeading);
    const double secondEastSpeed = secondGroundSpeed * qSin(secondHeading);
    const double secondNorthSpeed = secondGroundSpeed * qCos(secondHeading);
    const double relativeEastSpeed = (secondEastSpeed - firstEastSpeed) * KnotsToNmPerSecond;
    const double relativeNorthSpeed = (secondNorthSpeed - firstNorthSpeed) * KnotsToNmPerSecond;
    const double relativeSpeedSquared =
        relativeEastSpeed * relativeEastSpeed + relativeNorthSpeed * relativeNorthSpeed;

    double timeToClosest = 0.0;
    if (relativeSpeedSquared > 1e-12)
    {
        timeToClosest = qBound(
            0.0,
            -(eastNm * relativeEastSpeed + northNm * relativeNorthSpeed) /
                relativeSpeedSquared,
            ConflictLookAheadSeconds);
    }

    const double closestEast = eastNm + relativeEastSpeed * timeToClosest;
    const double closestNorth = northNm + relativeNorthSpeed * timeToClosest;
    const double closestHorizontal = qSqrt(closestEast * closestEast +
                                           closestNorth * closestNorth);
    const double closestVertical =
        qAbs((second.altitude - first.altitude) +
             (second.verticalSpeed - first.verticalSpeed) *
                 FeetPerMinuteToFeetPerSecond * timeToClosest);

    result.unsafe = closestHorizontal < MinimumHorizontalSeparationNm &&
                    closestVertical < MinimumVerticalSeparationFt;
    result.immediate =
        result.unsafe && (timeToClosest <= ImmediateConflictSeconds ||
                          distanceNm(first.latitude, first.longitude,
                                     second.latitude, second.longitude) <
                              MinimumHorizontalSeparationNm);
    result.horizontalDistanceNm = closestHorizontal;
    result.verticalDistanceFt = closestVertical;
    result.timeToClosestSeconds = timeToClosest;

    const double centerEastNm = (firstEastSpeed + secondEastSpeed) *
                                0.5 * KnotsToNmPerSecond * timeToClosest;
    const double centerNorthNm = (firstNorthSpeed + secondNorthSpeed) *
                                 0.5 * KnotsToNmPerSecond * timeToClosest;
    result.latitude = first.latitude + centerNorthNm / 60.0;
    result.longitude = first.longitude +
                       centerEastNm /
                           qMax(0.01, 60.0 * qCos(meanLatitude));
    return result;
}

void SimulationEngine::updateModel()
{
    m_entityModel.setAircraftStates(m_states, m_trailsChanged);
    m_trailsChanged = false;
}

int SimulationEngine::entityIndexForId(const QString &id) const
{
    return m_entityRows.value(id, -1);
}

double SimulationEngine::normalizeHeading(double heading)
{
    const double normalized = std::fmod(heading, 360.0);
    return normalized < 0.0 ? normalized + 360.0 : normalized;
}

double SimulationEngine::turnHeadingToward(double current, double target,
                                           double maximumChange)
{
    const double difference =
        normalizeHeading(target - current + 180.0) - 180.0;
    const double change = qBound(-maximumChange, difference, maximumChange);
    return normalizeHeading(current + change);
}

double SimulationEngine::distanceNm(double latitude1, double longitude1,
                                    double latitude2, double longitude2)
{
    const double lat1 = radians(latitude1);
    const double lat2 = radians(latitude2);
    const double deltaLatitude = lat2 - lat1;
    const double deltaLongitude = radians(longitude2 - longitude1);
    const double a = qSin(deltaLatitude * 0.5) * qSin(deltaLatitude * 0.5) +
                     qCos(lat1) * qCos(lat2) *
                         qSin(deltaLongitude * 0.5) * qSin(deltaLongitude * 0.5);
    return EarthRadiusNm * 2.0 * qAtan2(qSqrt(a), qSqrt(qMax(0.0, 1.0 - a)));
}

double SimulationEngine::bearingDegrees(double latitude1, double longitude1,
                                        double latitude2, double longitude2)
{
    const double lat1 = radians(latitude1);
    const double lat2 = radians(latitude2);
    const double deltaLongitude = radians(longitude2 - longitude1);
    const double y = qSin(deltaLongitude) * qCos(lat2);
    const double x = qCos(lat1) * qSin(lat2) -
                     qSin(lat1) * qCos(lat2) * qCos(deltaLongitude);
    return normalizeHeading(qRadiansToDegrees(qAtan2(y, x)));
}

QGeoCoordinate SimulationEngine::destinationPoint(double latitude, double longitude,
                                                  double bearing, double distance)
{
    const double angularDistance = distance / EarthRadiusNm;
    const double bearingRadians = radians(bearing);
    const double latitudeRadians = radians(latitude);
    const double longitudeRadians = radians(longitude);
    const double destinationLatitude = qAsin(
        qSin(latitudeRadians) * qCos(angularDistance) +
        qCos(latitudeRadians) * qSin(angularDistance) * qCos(bearingRadians));
    const double destinationLongitude = longitudeRadians + qAtan2(
                                                               qSin(bearingRadians) * qSin(angularDistance) * qCos(latitudeRadians),
                                                               qCos(angularDistance) - qSin(latitudeRadians) * qSin(destinationLatitude));
    return QGeoCoordinate(qRadiansToDegrees(destinationLatitude),
                          qRadiansToDegrees(destinationLongitude));
}
