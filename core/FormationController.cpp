#include "FormationController.h"

#include <QGeoCoordinate>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace
{
    constexpr double KnotsToNmPerSecond = 1.0 / 3600.0;
    constexpr double MaximumFormationTurnRateDegreesPerSecond = 60.0;
    constexpr double TrailRecordIntervalSeconds = 1.0;
    constexpr int MaximumTrailPoints = 600;

    int stateIndex(const QVector<AircraftSimulationState> &states,
                   const QString &id)
    {
        for (int index = 0; index < states.size(); ++index)
            if (states.at(index).id == id)
                return index;
        return -1;
    }

    QGeoCoordinate slotPosition(const AircraftSimulationState &leader,
                                const Entity &follower)
    {
        const FormationOffset offset = follower.formationOffset();
        const double distanceNm =
            qSqrt(offset.xNm * offset.xNm + offset.yNm * offset.yNm);
        const double bearing =
            leader.heading + qRadiansToDegrees(qAtan2(offset.xNm, offset.yNm));
        return QGeoCoordinate(leader.latitude, leader.longitude)
            .atDistanceAndAzimuth(distanceNm * 1852.0, bearing);
    }

    double normalizeHeading(double heading)
    {
        const double normalized = std::fmod(heading, 360.0);
        return normalized < 0.0 ? normalized + 360.0 : normalized;
    }

    double turnHeadingToward(double current, double target, double maximumChange)
    {
        const double difference =
            std::fmod(target - current + 540.0, 360.0) - 180.0;
        return normalizeHeading(current + qBound(-maximumChange, difference,
                                                   maximumChange));
    }
}

bool FormationController::updateFollowers(
    QVector<AircraftSimulationState> &states,
    const std::vector<Entity> &entities, double globalDeltaSeconds,
    double individualDeltaSeconds,
    double movementTimeSeconds)
{
    if ((!std::isfinite(globalDeltaSeconds) || globalDeltaSeconds <= 0.0) &&
        (!std::isfinite(individualDeltaSeconds) || individualDeltaSeconds <= 0.0))
        return false;

    bool trailsChanged = false;
    for (const Entity &follower : entities)
    {
        if (!follower.isFormationMember() ||
            follower.id() == follower.formationLeaderId() ||
            follower.formationState() != QStringLiteral("ACTIVE"))
            continue;

        const int followerIndex = stateIndex(states, follower.id());
        const int leaderIndex = stateIndex(states, follower.formationLeaderId());
        if (followerIndex < 0 || leaderIndex < 0)
            continue;

        AircraftSimulationState &state = states[followerIndex];
        const AircraftSimulationState &leader = states.at(leaderIndex);
        const double deltaSeconds =
            leader.individuallyControlled ? individualDeltaSeconds
                                          : globalDeltaSeconds;
        if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0)
            continue;
        const Entity *leaderEntity = nullptr;
        for (const Entity &candidate : entities)
            if (candidate.id() == leader.id)
            {
                leaderEntity = &candidate;
                break;
            }
        const bool globallyMoving =
            !leader.individuallyControlled && leaderEntity &&
            leader.currentWaypoint < leaderEntity->route().size() &&
            leaderEntity->route().size() > 1;
        if (leader.lifecycle != AircraftLifecycle::Airborne ||
            leader.movementStatus == QStringLiteral("COMPLETED") ||
            (leader.movementStatus == QStringLiteral("STATIONARY") &&
             !globallyMoving))
        {
            state.movementStatus = QStringLiteral("STATIONARY");
            if (state.status == QStringLiteral("FORMING"))
                state.status = QStringLiteral("HOLDING");
            continue;
        }
        if (leader.movementStatus == QStringLiteral("PAUSED"))
        {
            state.movementStatus = QStringLiteral("PAUSED");
            state.status = QStringLiteral("PAUSED");
            continue;
        }
        if (state.lifecycle != AircraftLifecycle::Airborne)
            continue;

        const QGeoCoordinate target = slotPosition(leader, follower);
        const QGeoCoordinate current(state.latitude, state.longitude);
        if (!target.isValid() || !current.isValid())
            continue;

        const double remainingMeters = current.distanceTo(target);
        if (!std::isfinite(remainingMeters))
            continue;
        const double course = normalizeHeading(current.azimuthTo(target));
        state.heading = turnHeadingToward(
            state.heading, course,
            MaximumFormationTurnRateDegreesPerSecond * deltaSeconds);

        const double maximumSpeedKnots =
            qMin(qMax(0.0, state.speed), qMax(0.0, leader.speed));
        const double maximumTravelMeters =
            maximumSpeedKnots * KnotsToNmPerSecond * deltaSeconds * 1852.0;
        if (remainingMeters <= maximumTravelMeters)
        {
            state.latitude = target.latitude();
            state.longitude = target.longitude();
        }
        else if (maximumTravelMeters > 0.0)
        {
            const QGeoCoordinate next =
                current.atDistanceAndAzimuth(maximumTravelMeters, course);
            if (!next.isValid())
                continue;
            state.latitude = next.latitude();
            state.longitude = next.longitude();
        }
        state.altitude = leader.altitude;
        state.lifecycle = AircraftLifecycle::Airborne;
        state.movementStatus = QStringLiteral("MOVING");
        state.status = QStringLiteral("FORMING");

        if (movementTimeSeconds >= state.nextTrailTime)
        {
            state.trail.push_back(QGeoCoordinate(state.latitude, state.longitude));
            if (state.trail.size() > MaximumTrailPoints)
                state.trail.remove(0, state.trail.size() - MaximumTrailPoints);
            state.nextTrailTime = movementTimeSeconds + TrailRecordIntervalSeconds;
            trailsChanged = true;
        }
    }
    return trailsChanged;
}
