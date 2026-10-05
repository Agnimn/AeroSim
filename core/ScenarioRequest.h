#pragma once

#include <QString>

// Structured output from a parser; kept separate from generation and the UI.
struct ScenarioRequest
{
    QString entityType;
    int entityCount = 1;
    QString region;
    double altitude = 10000.0;
    double speed = 450.0;
    double heading = 90.0;
};
