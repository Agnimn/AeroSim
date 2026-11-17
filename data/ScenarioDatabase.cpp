#include "ScenarioDatabase.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <utility>

namespace
{
    QString defaultDatabasePath()
    {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
               QStringLiteral("/scenarios/airspace_scenarios.sqlite");
    }
}

ScenarioDatabase::ScenarioDatabase(QString databasePath)
    : m_databasePath(std::move(databasePath))
{
    if (m_databasePath.isEmpty())
        m_databasePath = defaultDatabasePath();
}

bool ScenarioDatabase::initialize()
{
    QDir directory(QFileInfo(m_databasePath).absolutePath());
    if (!directory.exists() && !directory.mkpath(QStringLiteral(".")))
        return false;

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("airspace_scenario_db"));
    db.setDatabaseName(m_databasePath);
    if (!db.open())
    {
        QSqlDatabase::removeDatabase(QStringLiteral("airspace_scenario_db"));
        return false;
    }

    bool result = false;
    {
        QSqlQuery query(db);
        const bool promptsCreated = query.exec(
            QStringLiteral("CREATE TABLE IF NOT EXISTS prompts ("
                           "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                           "prompt_text TEXT NOT NULL, "
                           "saved_at TEXT NOT NULL)"));
        const bool scenariosCreated = query.exec(
            QStringLiteral("CREATE TABLE IF NOT EXISTS scenarios ("
                           "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                           "prompt_text TEXT NOT NULL, "
                           "scenario_name TEXT NOT NULL, "
                           "scenario_region TEXT NOT NULL, "
                           "entity_count INTEGER NOT NULL, "
                           "scenario_data TEXT NOT NULL, "
                           "saved_at TEXT NOT NULL)"));
        result = promptsCreated && scenariosCreated;
    }
    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(QStringLiteral("airspace_scenario_db"));
    return result;
}

bool ScenarioDatabase::saveScenario(const QString &prompt, const Scenario &scenario,
                                    QString *errorMessage) const
{
    const QString connectionName = QStringLiteral("airspace_scenario_db_save_") +
                                   QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(m_databasePath);
    if (!db.open())
    {
        if (errorMessage)
            *errorMessage = db.lastError().text();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
        return false;
    }

    bool result = false;
    QString failure;
    {
        QSqlQuery query(db);
        do
        {
            if (!query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS prompts ("
                                           "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                           "prompt_text TEXT NOT NULL, "
                                           "saved_at TEXT NOT NULL)")) ||
                !query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS scenarios ("
                                           "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                           "prompt_text TEXT NOT NULL, "
                                           "scenario_name TEXT NOT NULL, "
                                           "scenario_region TEXT NOT NULL, "
                                           "entity_count INTEGER NOT NULL, "
                                           "scenario_data TEXT NOT NULL, "
                                           "saved_at TEXT NOT NULL)")))
            {
                failure = query.lastError().text();
                break;
            }

            const QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
            query.prepare(QStringLiteral("INSERT INTO prompts(prompt_text, saved_at) VALUES(:prompt, :saved_at)"));
            query.bindValue(QStringLiteral(":prompt"), prompt.trimmed());
            query.bindValue(QStringLiteral(":saved_at"), timestamp);
            if (!query.exec())
            {
                failure = query.lastError().text();
                break;
            }

            QJsonDocument document(scenario.toJson());
            query.prepare(QStringLiteral("INSERT INTO scenarios(prompt_text, scenario_name, scenario_region, entity_count, scenario_data, saved_at) "
                                         "VALUES(:prompt, :scenario_name, :scenario_region, :entity_count, :scenario_data, :saved_at)"));
            query.bindValue(QStringLiteral(":prompt"), prompt.trimmed());
            query.bindValue(QStringLiteral(":scenario_name"), scenario.name());
            query.bindValue(QStringLiteral(":scenario_region"), scenario.region());
            query.bindValue(QStringLiteral(":entity_count"), static_cast<int>(scenario.entityCount()));
            query.bindValue(QStringLiteral(":scenario_data"), QString::fromUtf8(document.toJson(QJsonDocument::Compact)));
            query.bindValue(QStringLiteral(":saved_at"), timestamp);
            if (!query.exec())
            {
                failure = query.lastError().text();
                break;
            }

            result = true;
        } while (false);
    }

    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
    if (!result)
    {
        if (errorMessage)
            *errorMessage = failure;
        return false;
    }
    if (errorMessage)
        errorMessage->clear();
    return true;
}

QString ScenarioDatabase::databasePath() const
{
    return m_databasePath;
}
