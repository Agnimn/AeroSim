#pragma once

#include "Entity.h"
#include "SimulationState.h"

#include <QVector>
#include <vector>

class FormationController final
{
public:
    static bool updateFollowers(QVector<AircraftSimulationState> &states,
                                const std::vector<Entity> &entities,
                                double globalDeltaSeconds,
                                double individualDeltaSeconds,
                                double movementTimeSeconds);
};
