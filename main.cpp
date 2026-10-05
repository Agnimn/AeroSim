#include <QGuiApplication>
#include <QDebug>
#include <QQmlContext>
#include <QQmlApplicationEngine>

#include "data/ScenarioRepository.h"
#include "core/ScenarioEngine.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    ScenarioEngine scenarioEngine;
    ScenarioRepository scenarioRepository;
    engine.rootContext()->setContextProperty("scenarioEngine", &scenarioEngine);
    QObject::connect(&scenarioEngine, &ScenarioEngine::scenarioChanged, &app,
                     [&scenarioEngine, &scenarioRepository]()
                     {
                         QString errorMessage;
                         if (!scenarioRepository.saveScenario(
                                 scenarioEngine.scenario().description(),
                                 scenarioEngine.scenario(), nullptr, &errorMessage))
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
