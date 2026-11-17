#include <QGuiApplication>
#include <QDebug>
#include <QDir>
#include <QQmlContext>
#include <QQmlApplicationEngine>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

#include "data/ScenarioDatabase.h"
#include "data/ScenarioRepository.h"
#include "core/ScenarioEngine.h"
#include "core/TerrainData.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("AeroSim"));
    QCoreApplication::setApplicationName(QStringLiteral("AirspaceMonitor"));

    ScenarioEngine scenarioEngine;
    TerrainData terrainData;
    ScenarioRepository scenarioRepository;
    ScenarioDatabase scenarioDatabase;
    QQmlApplicationEngine engine;

    const QString configDirectory =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QString configFilePath;
    QString mapboxAccessToken;
    QString mapboxStyleId = QStringLiteral("mapbox/streets-v12");
    QString mapboxConfigurationError;
    bool mapboxConfigured = false;

    if (configDirectory.isEmpty())
    {
        mapboxConfigurationError =
            QStringLiteral("Could not determine a safe per-user configuration directory "
                           "for Mapbox configuration. OpenStreetMap tiles will be used.");
    }
    else
    {
        configFilePath = QDir(configDirectory).filePath(QStringLiteral("secrets.ini"));
        QSettings settings(configFilePath, QSettings::IniFormat);
        settings.sync();

        if (settings.status() != QSettings::NoError)
        {
            mapboxConfigurationError =
                QStringLiteral("Could not read Mapbox configuration at %1. "
                               "Check file permissions. OpenStreetMap tiles will be used.")
                    .arg(QDir::toNativeSeparators(configFilePath));
        }
        else
        {
            mapboxAccessToken = settings.value(QStringLiteral("Mapbox/AccessToken"))
                                    .toString()
                                    .trimmed();
            mapboxStyleId = settings.value(QStringLiteral("Mapbox/StyleId"),
                                            mapboxStyleId)
                                .toString()
                                .trimmed();
        }
    }

    const QByteArray environmentToken =
        qgetenv("MAPBOX_ACCESS_TOKEN").trimmed();
    if (!environmentToken.isEmpty())
        mapboxAccessToken = QString::fromUtf8(environmentToken);

    const QByteArray environmentStyle = qgetenv("MAPBOX_STYLE_ID").trimmed();
    if (!environmentStyle.isEmpty())
        mapboxStyleId = QString::fromUtf8(environmentStyle);

    mapboxConfigured = !mapboxAccessToken.isEmpty() &&
                       !mapboxStyleId.isEmpty();
    if (!mapboxConfigured && mapboxConfigurationError.isEmpty())
    {
        mapboxConfigurationError =
            QStringLiteral("Mapbox is not configured. Add [Mapbox] AccessToken=... "
                           "to %1 or set MAPBOX_ACCESS_TOKEN. "
                           "OpenStreetMap tiles will be used.")
                .arg(QDir::toNativeSeparators(configFilePath));
    }

    engine.rootContext()->setContextProperty(
        "mapboxAccessToken",
        mapboxConfigured
            ? QString::fromLatin1(QUrl::toPercentEncoding(mapboxAccessToken))
            : QString());
    engine.rootContext()->setContextProperty("mapboxStyleId", mapboxStyleId);
    engine.rootContext()->setContextProperty("mapboxConfigured",
                                             mapboxConfigured);
    engine.rootContext()->setContextProperty("mapboxConfigurationError",
                                             mapboxConfigurationError);

    if (!scenarioDatabase.initialize())
    {
        qWarning().noquote() << "Could not initialize the local SQLite scenario database.";
    }

    const QString latestScenarioPath = scenarioRepository.latestScenarioPath();
    if (!latestScenarioPath.isEmpty())
    {
        if (!scenarioEngine.loadScenario(latestScenarioPath))
            qWarning().noquote() << "Could not restore the latest saved scenario:"
                                 << scenarioEngine.statusMessage();
    }

    engine.rootContext()->setContextProperty("scenarioEngine", &scenarioEngine);
    engine.rootContext()->setContextProperty("terrainData", &terrainData);
    QObject::connect(&scenarioEngine, &ScenarioEngine::scenarioChanged, &app,
                     [&scenarioEngine, &scenarioRepository, &scenarioDatabase]()
                     {
                         QString errorMessage;
                         const Scenario snapshot = scenarioEngine.snapshotForPersistence();
                         if (!scenarioDatabase.saveScenario(
                                 snapshot.description(),
                                 snapshot, &errorMessage))
                         {
                             qWarning().noquote()
                                 << "Could not save generated scenario in SQLite:" << errorMessage;
                         }

                         if (!scenarioRepository.saveScenario(
                                 snapshot.description(),
                                 snapshot, nullptr, &errorMessage))
                         {
                             qWarning().noquote()
                                 << "Could not save generated scenario locally:" << errorMessage;
                         }
                     });
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []()
        { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("AirspaceMonitor", "Main");

    return QGuiApplication::exec();
}
