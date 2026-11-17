#pragma once

#include "../core/ScenarioRequest.h"

#include <QString>

class AirbaseRegistry;

// Parses deterministic fallback commands and normalized Ollama commands.
class AIParser
{
public:
    explicit AIParser(const AirbaseRegistry *airbaseRegistry = nullptr);
    bool parsePrompt(const QString &prompt, ScenarioRequest &request,
                     QString *errorMessage = nullptr,
                     const QString &selectedAirbaseId = {}) const;

private:
    const AirbaseRegistry *m_airbaseRegistry = nullptr;
};
