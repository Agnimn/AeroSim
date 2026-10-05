#pragma once

#include "../core/Scenario.h"

#include <QString>

// Offline file-backed repository; the persistence boundary can later be switched to SQLite.
class ScenarioRepository
{
public:
    explicit ScenarioRepository(QString storageDirectory = {});

    bool saveScenario(const QString &prompt, const Scenario &scenario,
                      QString *savedPath = nullptr, QString *errorMessage = nullptr) const;
    bool loadScenario(const QString &filePath, Scenario &scenario, QString *prompt = nullptr,
                      QString *errorMessage = nullptr) const;
    QString storageDirectory() const;

private:
    QString m_storageDirectory;
};
