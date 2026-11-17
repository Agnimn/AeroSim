#pragma once

#include "../core/Scenario.h"

#include <QString>

class ScenarioDatabase
{
public:
    explicit ScenarioDatabase(QString databasePath = {});

    bool initialize();
    bool saveScenario(const QString &prompt, const Scenario &scenario,
                      QString *errorMessage = nullptr) const;
    QString databasePath() const;

private:
    QString m_databasePath;
};
