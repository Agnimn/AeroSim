#pragma once

#include "../core/ScenarioRequest.h"

#include <QJsonObject>
#include <QString>

class AICommand
{
public:
    static bool fromJson(const QJsonObject &object, ScenarioRequest &request,
                         QString *errorMessage = nullptr);
    static bool fromResponse(const QString &response, ScenarioRequest &request,
                             QString *errorMessage = nullptr);
    static bool toPrompt(const ScenarioRequest &request, QString *prompt,
                         QString *errorMessage = nullptr);
};
