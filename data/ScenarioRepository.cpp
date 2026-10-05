#include "ScenarioRepository.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <utility>

ScenarioRepository::ScenarioRepository(QString storageDirectory)
    : m_storageDirectory(std::move(storageDirectory))
{
    if (m_storageDirectory.isEmpty())
    {
        m_storageDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/scenarios");
    }
}

bool ScenarioRepository::saveScenario(const QString &prompt, const Scenario &scenario,
                                      QString *savedPath, QString *errorMessage) const
{
    QDir directory(m_storageDirectory);
    if (!directory.mkpath(QStringLiteral(".")))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not create scenario storage directory.");
        return false;
    }

    QJsonObject documentObject = scenario.toJson();
    documentObject.insert(QStringLiteral("prompt"), prompt);
    const QString path = directory.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".json"));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    if (file.write(QJsonDocument(documentObject).toJson(QJsonDocument::Indented)) < 0 || !file.commit())
    {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }

    if (savedPath)
        *savedPath = path;
    if (errorMessage)
        errorMessage->clear();
    return true;
}

bool ScenarioRepository::loadScenario(const QString &filePath, Scenario &scenario, QString *prompt,
                                      QString *errorMessage) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        if (errorMessage)
            *errorMessage = parseError.errorString();
        return false;
    }

    const QJsonObject object = document.object();
    scenario = Scenario::fromJson(object);
    if (prompt)
        *prompt = object.value(QStringLiteral("prompt")).toString();
    if (errorMessage)
        errorMessage->clear();
    return true;
}

QString ScenarioRepository::storageDirectory() const
{
    return m_storageDirectory;
}
