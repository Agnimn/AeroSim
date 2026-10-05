#pragma once

#include "../core/ScenarioRequest.h"

#include <QString>

// Deterministic parser boundary, replaceable by a local model in a later iteration.
class AIParser
{
public:
    bool parsePrompt(const QString &prompt, ScenarioRequest &request,
                     QString *errorMessage = nullptr) const;
};
