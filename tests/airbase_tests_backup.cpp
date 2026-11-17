#include "../core/ScenarioEngine.h"
#include "../ai/AICommand.h"
#include "../ai/OllamaClient.h"
#include "../data/ScenarioDatabase.h"

#include <QGeoCoordinate>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QTemporaryDir>
#include <QtTest>

#include <QSet>

namespace
{
    QString *databaseWarning = nullptr;

    void captureDatabaseWarning(QtMsgType type, const QMessageLogContext &,
                                const QString &message)
    {
        if (databaseWarning && type == QtWarningMsg &&
            message.contains(QStringLiteral("removeDatabase")))
            *databaseWarning = message;
    }
}

class AirbaseScenarioTests final : public QObject
{
    Q_OBJECT

private slots:
    void registryLoadsWithoutDuplicateIds();
    void showIndiaTargetsRegisteredMarkers();
    void namedAirbasePositionsStayWithinSpawnRadius();
    void aliasesAndErrorsResolveSafely();
    void eachAirbaseHonorsEntityLimit();
    void regionScenarioPreservesExistingPlacement();
    void addPromptPreservesExistingEntities();
    void invalidAddDoesNotPartiallyModifyScenario();
    void scheduledTakeoffLifecycle();
    void scheduledAircraftRemainStationaryBeforeTakeoff();
    void airborneAircraftMoveWithControlledTime();
    void routeCompletionLandsAircraft();
    void completedAircraftRemainStationary();
    void cancelledAircraftRemainStationary();
    void inactiveAircraftAreExcludedFromConflicts();
    void resetRestoresInitialLifecycleState();
    void scheduledTakeoffPersistence();
    void simulationControlsRemainOperational();
    void validModelCommandIsValidated();
    void fencedModelCommandIsValidated();
    void malformedModelCommandIsRejected();
    void unknownOperationIsRejected();
    void invalidModelCountIsRejected();
    void modelOperationCoverageMatrix();
    void validModelOperationsExecuteAuthoritatively();
    void routeValidationRejectsOutOfBoundsOperations();
    void invalidModelCommandsDoNotMutateScenario();
    void unknownAircraftIdIsRejected();
    void mockedOllamaResponseIsParsed();
    void ollamaCommandExecutesScenarioMutation();
    void ollamaValidationFailureClearsPending();
    void ollamaExecutionFailureClearsPending();
    void ollamaUnavailableFallbackRejectsInvalidPrompt();
    void ollamaTimeoutCleansUpReply();
    void ollamaUnavailableUsesParserFallback();
    void scenarioDatabaseSavesWithoutConnectionWarning();
    void latestScenarioRestoresEntities();
    void runtimeStatePersistsAcrossSaveAndLoad();
    void scheduledEventStatesPersistAcrossSaveAndLoad();
    void legacyScenarioWithoutRuntimeStateStillLoads();
    void invalidRuntimeStateDoesNotMutateEngine();
    void resetRestoresInitialStateAfterRuntimeLoad();
};

void AirbaseScenarioTests::registryLoadsWithoutDuplicateIds()
{
    ScenarioEngine engine;
    QVERIFY(engine.airbaseModel()->rowCount() >= 30);
    QSet<QString> ids;
    for (const AirbaseRecord &record : engine.airbaseModel()->records())
    {
        QVERIFY(!record.id.isEmpty());
        QVERIFY(!ids.contains(record.id));
        ids.insert(record.id);
        QVERIFY(!record.name.isEmpty());
        QVERIFY(!record.verificationStatus.isEmpty());
    }
}

void AirbaseScenarioTests::showIndiaTargetsRegisteredMarkers()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Show airbases in India")));
    QVERIFY(!engine.scenarioGenerated());
    QCOMPARE(engine.airbaseDisplayRegion(), QStringLiteral("India"));
    QVERIFY(engine.airbaseModel()->rowCount() >= 30);
}

void AirbaseScenarioTests::namedAirbasePositionsStayWithinSpawnRadius()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 3 aircraft at Hindon Air Force Station")));
    QCOMPARE(engine.entityCount(), 3);
    QCOMPARE(engine.selectedAirbaseId(), QStringLiteral("in-hindon-afs"));

    const QGeoCoordinate origin(28.7077, 77.3619);
    QVector<QGeoCoordinate> positions;
    for (const Entity &entity : engine.scenario().entities())
    {
        const QGeoCoordinate position(entity.latitude(), entity.longitude());
        QVERIFY(position.isValid());
        QVERIFY(origin.distanceTo(position) <= 0.5 * 1852.0 + 0.01);
        positions.push_back(position);
    }
    for (int index = 0; index < positions.size(); ++index)
        for (int previous = 0; previous < index; ++previous)
            QVERIFY(positions.at(index) != positions.at(previous));
}

void AirbaseScenarioTests::aliasesAndErrorsResolveSafely()
{
    ScenarioEngine canonical;
    ScenarioEngine alias;
    QVERIFY(canonical.generateScenario(QStringLiteral("Generate 5 aircraft at Hindon")));
    QVERIFY(alias.generateScenario(QStringLiteral("Generate 5 aircraft at hindon afs")));
    QCOMPARE(canonical.selectedAirbaseId(), QStringLiteral("in-hindon-afs"));
    QCOMPARE(alias.selectedAirbaseId(), canonical.selectedAirbaseId());

    ScenarioEngine unknown;
    QVERIFY(!unknown.generateScenario(QStringLiteral("Generate 3 aircraft at Imaginary Airbase")));
    QVERIFY(unknown.statusMessage().contains(QStringLiteral("NOT CONFIGURED")));

    ScenarioEngine ambiguous;
    QVERIFY(!ambiguous.generateScenario(QStringLiteral("Generate 3 aircraft at Airbase")));
    QVERIFY(ambiguous.statusMessage().contains(QStringLiteral("AMBIGUOUS")));
}

void AirbaseScenarioTests::eachAirbaseHonorsEntityLimit()
{
    ScenarioEngine engine;
    QVERIFY(!engine.generateScenario(QStringLiteral(
        "Generate 10 aircraft at each configured airbase in India")));
    QVERIFY(engine.statusMessage().contains(QStringLiteral("EXCEED")));
}

void AirbaseScenarioTests::regionScenarioPreservesExistingPlacement()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Create 3 aircraft in North India")));
    QCOMPARE(engine.entityCount(), 3);
    QCOMPARE(engine.region(), QStringLiteral("North India"));
    QVERIFY(engine.selectedAirbaseId().isEmpty());
    for (const Entity &entity : engine.scenario().entities())
    {
        QVERIFY(entity.latitude() >= 24.0);
        QVERIFY(entity.latitude() <= 36.0);
        QVERIFY(entity.longitude() >= 74.0);
        QVERIFY(entity.longitude() <= 83.0);
    }
}

void AirbaseScenarioTests::addPromptPreservesExistingEntities()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 3 aircraft at Ambala Airbase")));
    QVERIFY(engine.playSimulation());
    engine.setSimulationSpeed(2.0);
    QVERIFY(engine.advanceSimulationForTest(5.0));
    const double simulationTimeBeforeAdd = engine.simulationTime();
    QCOMPARE(simulationTimeBeforeAdd, 10.0);
    QVERIFY(engine.simulationRunning());
    QMap<QString, QGeoCoordinate> originalPositions;
    for (const Entity &entity : engine.scenario().entities())
        originalPositions.insert(entity.id(), QGeoCoordinate(entity.latitude(), entity.longitude()));

    QVERIFY(engine.generateScenario(QStringLiteral("Add 2 more aircrafts in south india")));
    QCOMPARE(engine.entityCount(), 5);
    QCOMPARE(engine.region(), QStringLiteral("South India"));
    QCOMPARE(engine.selectedAirbaseId(), QStringLiteral("in-ambala-afs"));
    QCOMPARE(engine.simulationSpeed(), 2.0);
    QVERIFY(engine.simulationRunning());
    QVERIFY(engine.simulationTime() >= simulationTimeBeforeAdd);
    for (auto iterator = originalPositions.cbegin(); iterator != originalPositions.cend(); ++iterator)
    {
        const Entity *found = nullptr;
        for (const Entity &entity : engine.scenario().entities())
            if (entity.id() == iterator.key())
                found = &entity;
        QVERIFY(found != nullptr);
        QCOMPARE(QGeoCoordinate(found->latitude(), found->longitude()), iterator.value());
    }
    QCOMPARE(engine.scenario().entities().at(3).id(), QStringLiteral("A04"));
    QCOMPARE(engine.scenario().entities().at(4).id(), QStringLiteral("A05"));
    for (int index = 3; index < 5; ++index)
    {
        const Entity &entity = engine.scenario().entities().at(index);
        QVERIFY(entity.latitude() >= 8.0 && entity.latitude() <= 18.8);
        QVERIFY(entity.longitude() >= 72.0 && entity.longitude() <= 84.0);
    }
    engine.pauseSimulation();
}

void AirbaseScenarioTests::invalidAddDoesNotPartiallyModifyScenario()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 3 aircraft at Ambala Airbase")));
    const std::vector<Entity> before = engine.scenario().entities();
    QVERIFY(!engine.generateScenario(QStringLiteral("Add 0 more aircrafts in South India")));
    QCOMPARE(engine.entityCount(), 3);
    QCOMPARE(engine.region(), QStringLiteral("North India"));
    QCOMPARE(engine.scenario().entities().size(), before.size());
    for (int index = 0; index < static_cast<int>(before.size()); ++index)
        QCOMPARE(engine.scenario().entities().at(index).id(), before.at(index).id());
}

void AirbaseScenarioTests::scheduledTakeoffLifecycle()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 2 aircraft at Ambala Airbase")));
    QVERIFY(engine.generateScenario(QStringLiteral(
        "Schedule AER0002 to take off from Ambala at 00:01 and fly to Delhi")));
    QCOMPARE(engine.scheduledTakeoffs().size(), 1);
    QCOMPARE(engine.scheduledTakeoffs().at(0).toMap().value(QStringLiteral("status")).toString(),
             QStringLiteral("Pending"));
    const int secondRow = engine.entityModel()->rowForId(QStringLiteral("A02"));
    QVERIFY(secondRow >= 0);
    QCOMPARE(engine.entityModel()->data(engine.entityModel()->index(secondRow),
                                        EntityModel::LifecycleRole)
                 .toString(),
             QStringLiteral("Scheduled"));

    QCOMPARE(engine.simulationTime(), 0.0);
    QVERIFY(!engine.advanceSimulationForTest(60.0));
    QCOMPARE(engine.scheduledTakeoffs().at(0).toMap().value(QStringLiteral("status")).toString(),
             QStringLiteral("Pending"));
    const Entity &scheduledEntity = engine.scenario().entities().at(1);
    const QGeoCoordinate scheduledPosition(scheduledEntity.latitude(), scheduledEntity.longitude());
    QVERIFY(engine.playSimulation());
    engine.pauseSimulation();
    QCOMPARE(engine.simulationTime(), 0.0);
    QVERIFY(!engine.advanceSimulationForTest(60.0));
    QCOMPARE(engine.scheduledTakeoffs().at(0).toMap().value(QStringLiteral("status")).toString(),
             QStringLiteral("Pending"));
    QVERIFY(engine.playSimulation());
    QVERIFY(engine.advanceSimulationForTest(59.0));
    QCOMPARE(engine.simulationTime(), 59.0);
    QCOMPARE(engine.scheduledTakeoffs().at(0).toMap().value(QStringLiteral("status")).toString(),
             QStringLiteral("Pending"));
    QCOMPARE(QGeoCoordinate(engine.scenario().entities().at(1).latitude(),
                            engine.scenario().entities().at(1).longitude()),
             scheduledPosition);
    QVERIFY(engine.advanceSimulationForTest(2.0));
    QCOMPARE(engine.simulationTime(), 61.0);
    QCOMPARE(engine.scheduledTakeoffs().at(0).toMap().value(QStringLiteral("status")).toString(),
             QStringLiteral("Executed"));
    QCOMPARE(engine.entityModel()->data(engine.entityModel()->index(secondRow),
                                        EntityModel::LifecycleRole)
                 .toString(),
             QStringLiteral("Airborne"));
    const QGeoCoordinate airbornePosition(engine.entityModel()->data(
                                                                  engine.entityModel()->index(secondRow),
                                                                  EntityModel::LatitudeRole)
                                              .toDouble(),
                                          engine.entityModel()->data(
                                                                  engine.entityModel()->index(secondRow),
                                                                  EntityModel::LongitudeRole)
                                              .toDouble());
    QVERIFY(engine.advanceSimulationForTest(10.0));
    const QGeoCoordinate movedPosition(engine.entityModel()->data(
                                                               engine.entityModel()->index(secondRow),
                                                               EntityModel::LatitudeRole)
                                           .toDouble(),
                                       engine.entityModel()->data(
                                                               engine.entityModel()->index(secondRow),
                                                               EntityModel::LongitudeRole)
                                           .toDouble());
    QVERIFY(movedPosition != airbornePosition);
    const double executionTime = engine.simulationTime();
    QVERIFY(engine.advanceSimulationForTest(10.0));
    QCOMPARE(engine.simulationTime(), executionTime + 10.0);
    QCOMPARE(engine.scheduledTakeoffs().at(0).toMap().value(QStringLiteral("status")).toString(),
             QStringLiteral("Executed"));
    engine.pauseSimulation();

    ScenarioEngine cancelled;
    QVERIFY(cancelled.generateScenario(QStringLiteral("Generate 2 aircraft at Ambala Airbase")));
    QVERIFY(cancelled.generateScenario(QStringLiteral(
        "Schedule AER0001 to take off at 01:00")));
    QVERIFY(cancelled.generateScenario(QStringLiteral(
        "Cancel the scheduled take-off for AER0001")));
    QVERIFY(cancelled.playSimulation());
    QVERIFY(cancelled.advanceSimulationForTest(10.0));
    QCOMPARE(cancelled.scheduledTakeoffs().at(0).toMap().value(QStringLiteral("status")).toString(),
             QStringLiteral("Cancelled"));
    const int cancelledRow = cancelled.entityModel()->rowForId(QStringLiteral("A01"));
    QCOMPARE(cancelled.entityModel()->data(cancelled.entityModel()->index(cancelledRow),
                                           EntityModel::LifecycleRole)
                 .toString(),
             QStringLiteral("Cancelled"));
    cancelled.pauseSimulation();
    QVERIFY(cancelled.generateScenario(QStringLiteral("Reset scenario")));
    QCOMPARE(cancelled.scheduledTakeoffs().size(), 0);
    QCOMPARE(cancelled.entityCount(), 0);
}

void AirbaseScenarioTests::scheduledAircraftRemainStationaryBeforeTakeoff()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 1 aircraft at Ambala Airbase")));
    QVERIFY(engine.generateScenario(QStringLiteral(
        "Schedule AER0001 to take off at 00:10 and fly to Delhi")));
    const QVariantMap before = engine.aircraftInformation(QStringLiteral("A01"));
    QVERIFY(engine.playSimulation());
    QVERIFY(engine.advanceSimulationForTest(60.0));
    const QVariantMap after = engine.aircraftInformation(QStringLiteral("A01"));
    QCOMPARE(after.value(QStringLiteral("latitude")).toDouble(),
             before.value(QStringLiteral("latitude")).toDouble());
    QCOMPARE(after.value(QStringLiteral("longitude")).toDouble(),
             before.value(QStringLiteral("longitude")).toDouble());
    QCOMPARE(after.value(QStringLiteral("status")).toString(), QStringLiteral("SCHEDULED"));
    engine.pauseSimulation();
}

void AirbaseScenarioTests::airborneAircraftMoveWithControlledTime()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 1 aircraft at Ambala Airbase")));
    QVERIFY(engine.generateScenario(QStringLiteral(
        "Schedule AER0001 to take off at 00:00 and fly to Delhi")));
    QVERIFY(engine.playSimulation());
    QVERIFY(engine.advanceSimulationForTest(1.0));
    const QVariantMap before = engine.aircraftInformation(QStringLiteral("A01"));
    QVERIFY(engine.advanceSimulationForTest(10.0));
    const QVariantMap after = engine.aircraftInformation(QStringLiteral("A01"));
    QVERIFY(after.value(QStringLiteral("latitude")).toDouble() !=
            before.value(QStringLiteral("latitude")).toDouble() ||
            after.value(QStringLiteral("longitude")).toDouble() !=
                before.value(QStringLiteral("longitude")).toDouble());
    QCOMPARE(after.value(QStringLiteral("status")).toString(), QStringLiteral("ACTIVE"));
    engine.pauseSimulation();
}

void AirbaseScenarioTests::routeCompletionLandsAircraft()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 1 aircraft at Ambala Airbase")));
    QVERIFY(engine.generateScenario(QStringLiteral(
        "Schedule AER0001 to take off at 00:00 and fly to Delhi")));
    QVERIFY(engine.playSimulation());
    QVERIFY(engine.advanceSimulationForTest(3600.0));
    const int row = engine.entityModel()->rowForId(QStringLiteral("A01"));
    QVERIFY(row >= 0);
    QCOMPARE(engine.entityModel()->data(engine.entityModel()->index(row),
                                        EntityModel::LifecycleRole)
                 .toString(),
             QStringLiteral("Landed"));
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A01"))
                 .value(QStringLiteral("status"))
                 .toString(),
             QStringLiteral("LANDED"));
    engine.pauseSimulation();
}

void AirbaseScenarioTests::completedAircraftRemainStationary()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 1 aircraft at Ambala Airbase")));
    QVERIFY(engine.generateScenario(QStringLiteral(
        "Schedule AER0001 to take off at 00:00 and fly to Delhi")));
    QVERIFY(engine.playSimulation());
    QVERIFY(engine.advanceSimulationForTest(3600.0));
    const QVariantMap before = engine.aircraftInformation(QStringLiteral("A01"));
    QVERIFY(engine.advanceSimulationForTest(60.0));
    const QVariantMap after = engine.aircraftInformation(QStringLiteral("A01"));
    QCOMPARE(after.value(QStringLiteral("latitude")).toDouble(),
             before.value(QStringLiteral("latitude")).toDouble());
    QCOMPARE(after.value(QStringLiteral("longitude")).toDouble(),
             before.value(QStringLiteral("longitude")).toDouble());
    QCOMPARE(after.value(QStringLiteral("status")).toString(), QStringLiteral("LANDED"));
    engine.pauseSimulation();
}

void AirbaseScenarioTests::cancelledAircraftRemainStationary()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 1 aircraft at Ambala Airbase")));
    QVERIFY(engine.generateScenario(QStringLiteral(
        "Schedule AER0001 to take off at 00:10 and fly to Delhi")));
    QVERIFY(engine.generateScenario(QStringLiteral(
        "Cancel the scheduled take-off for AER0001")));
    const QVariantMap before = engine.aircraftInformation(QStringLiteral("A01"));
    QVERIFY(engine.playSimulation());
    QVERIFY(engine.advanceSimulationForTest(120.0));
    const QVariantMap after = engine.aircraftInformation(QStringLiteral("A01"));
    QCOMPARE(after.value(QStringLiteral("latitude")).toDouble(),
             before.value(QStringLiteral("latitude")).toDouble());
    QCOMPARE(after.value(QStringLiteral("longitude")).toDouble(),
             before.value(QStringLiteral("longitude")).toDouble());
    QCOMPARE(after.value(QStringLiteral("status")).toString(), QStringLiteral("CANCELLED"));
    engine.pauseSimulation();
}

void AirbaseScenarioTests::inactiveAircraftAreExcludedFromConflicts()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 2 aircraft at Ambala Airbase")));
    QVERIFY(engine.generateScenario(QStringLiteral(
        "Schedule AER0001 to take off at 00:10 and fly to Delhi")));
    QVERIFY(engine.playSimulation());
    QVERIFY(engine.advanceSimulationForTest(1.0));
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A01"))
                 .value(QStringLiteral("conflictStatus"))
                 .toString(),
             QStringLiteral("NORMAL"));
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A02"))
                 .value(QStringLiteral("conflictStatus"))
                 .toString(),
             QStringLiteral("NORMAL"));
    QVERIFY(engine.generateScenario(QStringLiteral(
        "Cancel the scheduled take-off for AER0001")));
    QVERIFY(engine.advanceSimulationForTest(1.0));
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A01"))
                 .value(QStringLiteral("conflictStatus"))
                 .toString(),
             QStringLiteral("NORMAL"));
    engine.pauseSimulation();
}

void AirbaseScenarioTests::resetRestoresInitialLifecycleState()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 1 aircraft at Ambala Airbase")));
    QVERIFY(engine.generateScenario(QStringLiteral(
        "Schedule AER0001 to take off at 00:10 and fly to Delhi")));
    QVERIFY(engine.generateScenario(QStringLiteral(
        "Cancel the scheduled take-off for AER0001")));
    QVERIFY(engine.playSimulation());
    QVERIFY(engine.advanceSimulationForTest(120.0));
    engine.resetSimulation();
    QCOMPARE(engine.simulationTime(), 0.0);
    QCOMPARE(engine.scheduledTakeoffs().at(0).toMap().value(QStringLiteral("status")).toString(),
             QStringLiteral("Pending"));
    const int row = engine.entityModel()->rowForId(QStringLiteral("A01"));
    QCOMPARE(engine.entityModel()->data(engine.entityModel()->index(row),
                                        EntityModel::LifecycleRole)
                 .toString(),
             QStringLiteral("Scheduled"));
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A01"))
                 .value(QStringLiteral("status"))
                 .toString(),
             QStringLiteral("SCHEDULED"));
}

void AirbaseScenarioTests::scheduledTakeoffPersistence()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 4 aircraft at Ambala Airbase")));
    QVERIFY(engine.generateScenario(QStringLiteral(
        "Schedule AER0002 to take off from Ambala at 10:47 AM and fly to Delhi")));
    QCOMPARE(engine.entityCount(), 4);
    QCOMPARE(engine.scenario().scheduledTakeoffs().size(), 1);
    QCOMPARE(engine.scenario().scheduledTakeoffs().first().aircraftId, QStringLiteral("A02"));
    QCOMPARE(engine.scenario().scheduledTakeoffs().first().originAirbaseId,
             QStringLiteral("in-ambala-afs"));
    QCOMPARE(engine.scenario().scheduledTakeoffs().first().destinationName, QStringLiteral("delhi"));

    QTemporaryDir storage;
    QVERIFY(storage.isValid());
    ScenarioRepository repository(storage.path());
    QString path;
    QString error;
    QVERIFY(repository.saveScenario(QStringLiteral("scheduled test"), engine.scenario(),
                                    &path, &error));
    Scenario restored;
    QVERIFY(repository.loadScenario(path, restored, nullptr, &error));
    QCOMPARE(restored.scheduledTakeoffs().size(), 1);
    QCOMPARE(restored.scheduledTakeoffs().first().aircraftId, QStringLiteral("A02"));
    QCOMPARE(restored.scheduledTakeoffs().first().scheduledSeconds, 10.0 * 3600.0 + 47.0 * 60.0);
    QVERIFY(!restored.scheduledTakeoffs().first().executed);
}

void AirbaseScenarioTests::simulationControlsRemainOperational()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 2 aircraft at Ambala Air Force Station")));
    QVERIFY(!engine.simulationRunning());
    QVERIFY(engine.playSimulation());
    QVERIFY(engine.simulationRunning());
    engine.pauseSimulation();
    QVERIFY(!engine.simulationRunning());
    engine.setSimulationSpeed(2.0);
    QCOMPARE(engine.simulationSpeed(), 2.0);
    engine.resetSimulation();
    QVERIFY(!engine.simulationRunning());
    QCOMPARE(engine.entityCount(), 2);
}

void AirbaseScenarioTests::validModelCommandIsValidated()
{
    ScenarioRequest request;
    QString error;
    QVERIFY(AICommand::fromJson(
        QJsonObject{{QStringLiteral("operation"), QStringLiteral("ADD_ENTITIES")},
                    {QStringLiteral("count"), 2},
                    {QStringLiteral("destination"), QStringLiteral("Hindon Airbase")}},
        request, &error));
    QCOMPARE(request.operation, ScenarioOperation::Add);
    QCOMPARE(request.entityCount, 2);
    QCOMPARE(request.locationName, QStringLiteral("Hindon Airbase"));
}

void AirbaseScenarioTests::fencedModelCommandIsValidated()
{
    ScenarioRequest request;
    QString error;
    QVERIFY(AICommand::fromResponse(
        QStringLiteral("```json\n{\"operation\":\"ADD_ENTITIES\",\"count\":2,"
                       "\"destination\":\"Hindon Airbase\"}\n```"),
        request, &error));
    QCOMPARE(request.operation, ScenarioOperation::Add);
    QCOMPARE(request.entityCount, 2);
}

void AirbaseScenarioTests::malformedModelCommandIsRejected()
{
    ScenarioRequest request;
    QString error;
    QVERIFY(!AICommand::fromJson(QJsonObject{}, request, &error));
    QVERIFY(!error.isEmpty());
}

void AirbaseScenarioTests::unknownOperationIsRejected()
{
    ScenarioRequest request;
    QString error;
    QVERIFY(!AICommand::fromJson(
        QJsonObject{{QStringLiteral("operation"), QStringLiteral("DELETE_ALL")}},
        request, &error));
    QVERIFY(error.contains(QStringLiteral("UNSUPPORTED")));
}

void AirbaseScenarioTests::invalidModelCountIsRejected()
{
    ScenarioRequest request;
    QString error;
    QVERIFY(!AICommand::fromJson(
        QJsonObject{{QStringLiteral("operation"), QStringLiteral("ADD_ENTITIES")},
                    {QStringLiteral("count"), 0},
                    {QStringLiteral("destination"), QStringLiteral("Delhi")}},
        request, &error));
    QVERIFY(error.contains(QStringLiteral("BETWEEN")));
}

void AirbaseScenarioTests::modelOperationCoverageMatrix()
{
    struct OperationCase
    {
        QJsonObject valid;
        QJsonObject missing;
        QJsonObject invalid;
    };
    const QVector<OperationCase> cases = {
        {QJsonObject{{QStringLiteral("operation"), QStringLiteral("ADD_ENTITIES")},
                     {QStringLiteral("count"), 2},
                     {QStringLiteral("destination"), QStringLiteral("Hindon Airbase")}},
         QJsonObject{{QStringLiteral("operation"), QStringLiteral("ADD_ENTITIES")},
                     {QStringLiteral("destination"), QStringLiteral("Hindon Airbase")}},
         QJsonObject{{QStringLiteral("operation"), QStringLiteral("ADD_ENTITIES")},
                     {QStringLiteral("count"), 0},
                     {QStringLiteral("destination"), QStringLiteral("Hindon Airbase")}}},
        {QJsonObject{{QStringLiteral("operation"), QStringLiteral("MOVE_ENTITY")},
                     {QStringLiteral("entity_id"), QStringLiteral("A01")},
                     {QStringLiteral("destination"), QStringLiteral("Delhi")}},
         QJsonObject{{QStringLiteral("operation"), QStringLiteral("MOVE_ENTITY")},
                     {QStringLiteral("entity_id"), QStringLiteral("A01")}},
         QJsonObject{{QStringLiteral("operation"), QStringLiteral("MOVE_ENTITY")},
                     {QStringLiteral("entity_id"), QStringLiteral("not-an-id")},
                     {QStringLiteral("destination"), QStringLiteral("Delhi")}}},
        {QJsonObject{{QStringLiteral("operation"), QStringLiteral("SET_ROUTE")},
                     {QStringLiteral("entity_id"), QStringLiteral("A01")},
                     {QStringLiteral("destination"), QStringLiteral("Delhi")}},
         QJsonObject{{QStringLiteral("operation"), QStringLiteral("SET_ROUTE")},
                     {QStringLiteral("destination"), QStringLiteral("Delhi")}},
         QJsonObject{{QStringLiteral("operation"), QStringLiteral("SET_ROUTE")},
                     {QStringLiteral("entity_id"), QStringLiteral("A01")},
                     {QStringLiteral("destination"), QStringLiteral("")}}},
        {QJsonObject{{QStringLiteral("operation"), QStringLiteral("SCHEDULE_TAKEOFF")},
                     {QStringLiteral("entity_id"), QStringLiteral("A01")},
                     {QStringLiteral("time"), QStringLiteral("10:30")}},
         QJsonObject{{QStringLiteral("operation"), QStringLiteral("SCHEDULE_TAKEOFF")},
                     {QStringLiteral("entity_id"), QStringLiteral("A01")}},
         QJsonObject{{QStringLiteral("operation"), QStringLiteral("SCHEDULE_TAKEOFF")},
                     {QStringLiteral("entity_id"), QStringLiteral("A01")},
                     {QStringLiteral("time"), QStringLiteral("25:00")}}},
        {QJsonObject{{QStringLiteral("operation"), QStringLiteral("CANCEL_EVENT")},
                     {QStringLiteral("entity_id"), QStringLiteral("A01")}},
         QJsonObject{{QStringLiteral("operation"), QStringLiteral("CANCEL_EVENT")}},
         QJsonObject{{QStringLiteral("operation"), QStringLiteral("CANCEL_EVENT")},
                     {QStringLiteral("entity_id"), QStringLiteral("invalid")}}},
        {QJsonObject{{QStringLiteral("operation"), QStringLiteral("SHOW_AIRBASES")},
                     {QStringLiteral("region"), QStringLiteral("North India")}},
         QJsonObject{{QStringLiteral("operation"), QStringLiteral("SHOW_AIRBASES")}},
         QJsonObject{{QStringLiteral("operation"), QStringLiteral("SHOW_AIRBASES")},
                     {QStringLiteral("region"), QStringLiteral("")}}},
        {QJsonObject{{QStringLiteral("operation"), QStringLiteral("QUERY_SCENARIO")}},
         QJsonObject{{QStringLiteral("operation"), QStringLiteral("QUERY_SCENARIO")}},
         QJsonObject{{QStringLiteral("operation"), QStringLiteral("UNKNOWN_OPERATION")}}}
    };

    for (const OperationCase &testCase : cases)
    {
        ScenarioRequest request;
        QString error;
        QVERIFY2(AICommand::fromJson(testCase.valid, request, &error),
                 qPrintable(error));
        QString canonicalPrompt;
        QVERIFY(AICommand::toPrompt(request, &canonicalPrompt, &error));
        if (testCase.valid.value(QStringLiteral("operation")).toString() !=
            QStringLiteral("QUERY_SCENARIO"))
        {
            QVERIFY(!AICommand::fromJson(testCase.missing, request, &error));
            QVERIFY(!AICommand::fromJson(testCase.invalid, request, &error));
        }
        else
        {
            QVERIFY(!AICommand::fromJson(testCase.invalid, request, &error));
        }
    }

    ScenarioRequest query;
    QString error;
    QVERIFY(AICommand::fromJson(cases.last().valid, query, &error));
    QVERIFY(AICommand::toPrompt(query, &error));
    QCOMPARE(query.operation, ScenarioOperation::Query);
}

void AirbaseScenarioTests::invalidModelCommandsDoNotMutateScenario()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 2 aircraft at Hindon Airbase")));
    const std::vector<Entity> before = engine.scenario().entities();
    const QString beforeRegion = engine.region();

    const QStringList invalidPrompts = {
        QStringLiteral("Move A99 to Delhi"),
        QStringLiteral("Change route for A01 to Imaginary Region"),
        QStringLiteral("Schedule A99 to take off at 10:30"),
        QStringLiteral("Cancel the scheduled take-off for A99"),
        QStringLiteral("Show airbases in Imaginary Region")
    };
    for (const QString &prompt : invalidPrompts)
        QVERIFY(!engine.generateScenario(prompt));

    QCOMPARE(engine.entityCount(), static_cast<int>(before.size()));
    QCOMPARE(engine.region(), beforeRegion);
    for (int index = 0; index < static_cast<int>(before.size()); ++index)
        QCOMPARE(engine.scenario().entities().at(index).id(), before.at(index).id());
}

void AirbaseScenarioTests::validModelOperationsExecuteAuthoritatively()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 2 aircraft at Hindon Airbase")));

    const QVector<QPair<QString, QString>> operations = {
        {QStringLiteral("Move A01 to Delhi"), QStringLiteral("UPDATED A01")},
        {QStringLiteral("Change route for A01 to Delhi"), QStringLiteral("UPDATED A01")},
        {QStringLiteral("Schedule A01 to take off at 10:30"), QStringLiteral("TAKE-OFF SCHEDULED")},
        {QStringLiteral("Cancel the scheduled take-off for A01"), QStringLiteral("TAKE-OFF CANCELLED FOR A01")},
        {QStringLiteral("Show airbases in North India"), QStringLiteral("AIRBASES SHOWN IN North India")},
        {QStringLiteral("Query scenario"), QStringLiteral("SCENARIO HAS 2 ENTITIES")}
    };
    for (const auto &operation : operations)
    {
        QVERIFY2(engine.generateScenario(operation.first), qPrintable(engine.statusMessage()));
        QVERIFY(engine.statusMessage().startsWith(operation.second));
    }
    QCOMPARE(engine.entityCount(), 2);
}

void AirbaseScenarioTests::routeValidationRejectsOutOfBoundsOperations()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 2 aircraft at Hindon Airbase")));

    const Entity original = engine.scenario().entities().at(0);
    const QVariantMap originalRuntime = engine.aircraftInformation(original.id());

    QVERIFY(engine.generateScenario(QStringLiteral("Move A01 to Delhi")));
    const Entity moved = engine.scenario().entities().at(0);
    QCOMPARE(moved.latitude(), 28.6139);
    QCOMPARE(moved.longitude(), 77.2090);

    QVERIFY(engine.generateScenario(QStringLiteral("Change route for A01 to Delhi")));
    QVERIFY(engine.scenario().entities().at(0).route().size() >= 2);

    const Entity beforeInvalid = engine.scenario().entities().at(0);
    const QVariantMap runtimeBeforeInvalid = engine.aircraftInformation(beforeInvalid.id());
    QVERIFY(!engine.generateScenario(QStringLiteral("Move A01 to South India")));
    QCOMPARE(engine.statusMessage(), QStringLiteral("ROUTE IS OUTSIDE SCENARIO BOUNDS"));
    const Entity afterInvalidMove = engine.scenario().entities().at(0);
    QCOMPARE(afterInvalidMove.latitude(), beforeInvalid.latitude());
    QCOMPARE(afterInvalidMove.longitude(), beforeInvalid.longitude());
    QCOMPARE(afterInvalidMove.route().size(), beforeInvalid.route().size());
    QCOMPARE(engine.aircraftInformation(afterInvalidMove.id()), runtimeBeforeInvalid);

    QVERIFY(!engine.generateScenario(QStringLiteral("Change route for A01 to South India")));
    QCOMPARE(engine.statusMessage(), QStringLiteral("ROUTE IS OUTSIDE SCENARIO BOUNDS"));
    const Entity afterInvalidRoute = engine.scenario().entities().at(0);
    QCOMPARE(afterInvalidRoute.latitude(), beforeInvalid.latitude());
    QCOMPARE(afterInvalidRoute.longitude(), beforeInvalid.longitude());
    QCOMPARE(afterInvalidRoute.route().size(), beforeInvalid.route().size());
    QCOMPARE(engine.aircraftInformation(afterInvalidRoute.id()), runtimeBeforeInvalid);
    QVERIFY(originalRuntime.value(QStringLiteral("id")).toString() == original.id());
}

void AirbaseScenarioTests::ollamaUnavailableUsesParserFallback()
{
    qputenv("AEROSIM_OLLAMA_ENDPOINT", QByteArrayLiteral("http://127.0.0.1:1"));
    ScenarioEngine engine;
    engine.generateScenarioWithAI(QStringLiteral("Generate 2 aircraft in North India"));
    QTRY_VERIFY_WITH_TIMEOUT(engine.statusMessage().contains(QStringLiteral("DETERMINISTIC")),
                             6000);
    QCOMPARE(engine.entityCount(), 2);
    qunsetenv("AEROSIM_OLLAMA_ENDPOINT");
}

void AirbaseScenarioTests::scenarioDatabaseSavesWithoutConnectionWarning()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());
    const QString databasePath = storage.filePath(QStringLiteral("scenario.sqlite"));
    ScenarioDatabase database(databasePath);
    QVERIFY(database.initialize());

    Scenario scenario;
    scenario.setName(QStringLiteral("Database test"));
    scenario.setRegion(QStringLiteral("North India"));
    scenario.addEntity(Entity(QStringLiteral("A01"), QStringLiteral("Aircraft 1"),
                              QStringLiteral("aircraft"), 28.0, 77.0, 28000.0,
                              450.0, 90.0, QStringLiteral("ACTIVE")));

    QString warning;
    databaseWarning = &warning;
    qInstallMessageHandler(captureDatabaseWarning);
    QVERIFY(database.saveScenario(QStringLiteral("database test"), scenario));
    qInstallMessageHandler(nullptr);
    databaseWarning = nullptr;
    QVERIFY2(warning.isEmpty(), qPrintable(warning));
}

void AirbaseScenarioTests::latestScenarioRestoresEntities()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());
    ScenarioRepository repository(storage.path());
    Scenario original;
    original.setName(QStringLiteral("Restart test"));
    original.setRegion(QStringLiteral("North India"));
    original.addEntity(Entity(QStringLiteral("A01"), QStringLiteral("Aircraft 1"),
                              QStringLiteral("aircraft"), 28.0, 77.0, 28000.0,
                              450.0, 90.0, QStringLiteral("ACTIVE")));
    QString path;
    QVERIFY(repository.saveScenario(QStringLiteral("restart test"), original, &path));
    QVERIFY(!repository.latestScenarioPath().isEmpty());

    ScenarioEngine restartedEngine;
    QVERIFY(restartedEngine.loadScenario(repository.latestScenarioPath()));
    QCOMPARE(restartedEngine.entityCount(), 1);
    QCOMPARE(restartedEngine.scenario().entities().at(0).id(), QStringLiteral("A01"));
}

void AirbaseScenarioTests::runtimeStatePersistsAcrossSaveAndLoad()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 1 aircraft at Ambala Airbase")));
    QVERIFY(engine.generateScenario(QStringLiteral(
        "Schedule AER0001 to take off at 00:00 and fly to Delhi")));
    QVERIFY(engine.playSimulation());
    QVERIFY(engine.advanceSimulationForTest(30.0));
    const QVariantMap before = engine.aircraftInformation(QStringLiteral("A01"));
    const double timeBefore = engine.simulationTime();
    QVERIFY(engine.saveScenario(storage.filePath(QStringLiteral("runtime.json"))));

    ScenarioRepository repository(storage.path());
    const QString savedPath = repository.latestScenarioPath();
    QVERIFY(!savedPath.isEmpty());
    ScenarioEngine restored;
    QVERIFY(restored.loadScenario(savedPath));
    const QVariantMap after = restored.aircraftInformation(QStringLiteral("A01"));
    QCOMPARE(restored.simulationTime(), timeBefore);
    QCOMPARE(after.value(QStringLiteral("latitude")).toDouble(),
             before.value(QStringLiteral("latitude")).toDouble());
    QCOMPARE(after.value(QStringLiteral("longitude")).toDouble(),
             before.value(QStringLiteral("longitude")).toDouble());
    QCOMPARE(after.value(QStringLiteral("altitude")).toDouble(),
             before.value(QStringLiteral("altitude")).toDouble());
    QCOMPARE(after.value(QStringLiteral("heading")).toDouble(),
             before.value(QStringLiteral("heading")).toDouble());
    QCOMPARE(after.value(QStringLiteral("speed")).toDouble(),
             before.value(QStringLiteral("speed")).toDouble());
    QCOMPARE(after.value(QStringLiteral("currentWaypoint")).toString(),
             before.value(QStringLiteral("currentWaypoint")).toString());
    QCOMPARE(after.value(QStringLiteral("status")).toString(),
             before.value(QStringLiteral("status")).toString());
    QCOMPARE(restored.entityModel()->data(
                 restored.entityModel()->index(restored.entityModel()->rowForId(QStringLiteral("A01"))),
                 EntityModel::LifecycleRole)
                 .toString(),
             QStringLiteral("Airborne"));
    engine.pauseSimulation();
}

void AirbaseScenarioTests::scheduledEventStatesPersistAcrossSaveAndLoad()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 3 aircraft at Ambala Airbase")));
    QVERIFY(engine.generateScenario(QStringLiteral(
        "Schedule AER0001 to take off at 00:10 and fly to Delhi")));
    QVERIFY(engine.generateScenario(QStringLiteral(
        "Schedule AER0002 to take off at 00:00 and fly to Delhi")));
    QVERIFY(engine.generateScenario(QStringLiteral(
        "Schedule AER0003 to take off at 00:10 and fly to Delhi")));
    QVERIFY(engine.generateScenario(QStringLiteral(
        "Cancel the scheduled take-off for AER0003")));
    QVERIFY(engine.playSimulation());
    QVERIFY(engine.advanceSimulationForTest(1.0));
    QVERIFY(engine.saveScenario(storage.filePath(QStringLiteral("events.json"))));

    ScenarioRepository repository(storage.path());
    ScenarioEngine restored;
    QVERIFY(restored.loadScenario(repository.latestScenarioPath()));
    QCOMPARE(restored.scheduledTakeoffs().size(), 3);
    QCOMPARE(restored.scheduledTakeoffs().at(0).toMap().value(QStringLiteral("status")).toString(),
             QStringLiteral("Pending"));
    QCOMPARE(restored.scheduledTakeoffs().at(1).toMap().value(QStringLiteral("status")).toString(),
             QStringLiteral("Executed"));
    QCOMPARE(restored.scheduledTakeoffs().at(2).toMap().value(QStringLiteral("status")).toString(),
             QStringLiteral("Cancelled"));
    QCOMPARE(restored.aircraftInformation(QStringLiteral("A01"))
                 .value(QStringLiteral("status"))
                 .toString(),
             QStringLiteral("SCHEDULED"));
    QCOMPARE(restored.aircraftInformation(QStringLiteral("A02"))
                 .value(QStringLiteral("status"))
                 .toString(),
             QStringLiteral("ACTIVE"));
    QCOMPARE(restored.aircraftInformation(QStringLiteral("A03"))
                 .value(QStringLiteral("status"))
                 .toString(),
             QStringLiteral("CANCELLED"));
    QCOMPARE(restored.entityModel()->data(
                 restored.entityModel()->index(restored.entityModel()->rowForId(QStringLiteral("A03"))),
                 EntityModel::LifecycleRole)
                 .toString(),
             QStringLiteral("Cancelled"));
    engine.pauseSimulation();
}

void AirbaseScenarioTests::legacyScenarioWithoutRuntimeStateStillLoads()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());
    Scenario original;
    original.setName(QStringLiteral("Legacy"));
    original.setDescription(QStringLiteral("legacy scenario"));
    original.setRegion(QStringLiteral("North India"));
    original.addEntity(Entity(QStringLiteral("A01"), QStringLiteral("Aircraft 1"),
                              QStringLiteral("aircraft"), 28.0, 77.0, 28000.0,
                              450.0, 90.0, QStringLiteral("ACTIVE")));
    ScenarioRepository repository(storage.path());
    QString path;
    QVERIFY(repository.saveScenario(QStringLiteral("legacy"), original, &path));
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 1 aircraft at Ambala Airbase")));
    QVERIFY(engine.loadScenario(path));
    QCOMPARE(engine.simulationTime(), 0.0);
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A01"))
                 .value(QStringLiteral("latitude"))
                 .toDouble(),
             28.0);
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A01"))
                 .value(QStringLiteral("status"))
                 .toString(),
             QStringLiteral("ACTIVE"));
}

void AirbaseScenarioTests::invalidRuntimeStateDoesNotMutateEngine()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());
    Scenario source;
    source.setName(QStringLiteral("Invalid runtime"));
    source.setRegion(QStringLiteral("North India"));
    source.addEntity(Entity(QStringLiteral("A01"), QStringLiteral("Aircraft 1"),
                            QStringLiteral("aircraft"), 28.0, 77.0, 28000.0,
                            450.0, 90.0, QStringLiteral("ACTIVE")));
    QJsonObject document = source.toJson();
    document.insert(QStringLiteral("runtimeState"),
                    QJsonObject{{QStringLiteral("simulationTime"), 10.0},
                                {QStringLiteral("aircraft"),
                                 QJsonArray{QJsonObject{
                                     {QStringLiteral("id"), QStringLiteral("A01")},
                                     {QStringLiteral("latitude"), QStringLiteral("not-a-number")}}}}});
    const QString path = storage.filePath(QStringLiteral("invalid-runtime.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QJsonDocument(document).toJson()) > 0);
    file.close();

    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 1 aircraft at Ambala Airbase")));
    const int countBefore = engine.entityCount();
    const double timeBefore = engine.simulationTime();
    QVERIFY(!engine.loadScenario(path));
    QCOMPARE(engine.entityCount(), countBefore);
    QCOMPARE(engine.simulationTime(), timeBefore);
    QCOMPARE(engine.scenario().entities().at(0).id(), QStringLiteral("A01"));
}

void AirbaseScenarioTests::resetRestoresInitialStateAfterRuntimeLoad()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());
    ScenarioEngine source;
    QVERIFY(source.generateScenario(QStringLiteral("Generate 1 aircraft at Ambala Airbase")));
    QVERIFY(source.generateScenario(QStringLiteral(
        "Schedule AER0001 to take off at 00:00 and fly to Delhi")));
    QVERIFY(source.playSimulation());
    QVERIFY(source.advanceSimulationForTest(30.0));
    const double initialLatitude = source.scenario().entities().at(0).latitude();
    const double initialLongitude = source.scenario().entities().at(0).longitude();
    QVERIFY(source.saveScenario(storage.filePath(QStringLiteral("reset.json"))));

    ScenarioRepository repository(storage.path());
    ScenarioEngine restored;
    QVERIFY(restored.loadScenario(repository.latestScenarioPath()));
    QVERIFY(restored.simulationTime() > 0.0);
    restored.resetSimulation();
    QCOMPARE(restored.simulationTime(), 0.0);
    QCOMPARE(restored.scheduledTakeoffs().at(0).toMap().value(QStringLiteral("status")).toString(),
             QStringLiteral("Pending"));
    QCOMPARE(restored.aircraftInformation(QStringLiteral("A01"))
                 .value(QStringLiteral("latitude"))
                 .toDouble(),
             initialLatitude);
    QCOMPARE(restored.aircraftInformation(QStringLiteral("A01"))
                 .value(QStringLiteral("longitude"))
                 .toDouble(),
             initialLongitude);
    QCOMPARE(restored.aircraftInformation(QStringLiteral("A01"))
                 .value(QStringLiteral("status"))
                 .toString(),
             QStringLiteral("SCHEDULED"));
    source.pauseSimulation();
}

void AirbaseScenarioTests::unknownAircraftIdIsRejected()
{
    ScenarioEngine engine;
    QVERIFY(!engine.generateScenario(QStringLiteral("Move A99 to Delhi")));
    QVERIFY(engine.statusMessage().contains(QStringLiteral("NOT FOUND")));
}

void AirbaseScenarioTests::mockedOllamaResponseIsParsed()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::AnyIPv4));
    OllamaClient client;
    client.setEndpoint(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())));
    QSignalSpy responseSpy(&client, &OllamaClient::responseReady);
    connect(&server, &QTcpServer::newConnection, &server, [&server]()
            {
                QTcpSocket *socket = server.nextPendingConnection();
                const QByteArray body =
                    QByteArrayLiteral("{\"response\":\"{\\\"operation\\\":\\\"QUERY_SCENARIO\\\"}\"}");
                const QByteArray headers =
                    QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ") +
                    QByteArray::number(body.size()) + QByteArrayLiteral("\r\nConnection: close\r\n\r\n");
                socket->write(headers + body);
                socket->flush();
            });
    client.generate(QStringLiteral("query"));
    QTRY_COMPARE_WITH_TIMEOUT(responseSpy.count(), 1, 3000);
    QCOMPARE(responseSpy.at(0).at(0).toString(),
             QStringLiteral("{\"operation\":\"QUERY_SCENARIO\"}"));
}

void AirbaseScenarioTests::ollamaCommandExecutesScenarioMutation()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::AnyIPv4));
    connect(&server, &QTcpServer::newConnection, &server, [&server]()
            {
                QTcpSocket *socket = server.nextPendingConnection();
                const QByteArray modelCommand =
                    QByteArrayLiteral("```json\n{\"operation\":\"ADD_ENTITIES\","
                                      "\"count\":2,\"entity_type\":\"aircraft\","
                                      "\"destination\":\"Hindon Airbase\"}\n```");
                const QByteArray body =
                    QByteArrayLiteral("{\"response\":\"") +
                    QByteArray(modelCommand).replace("\\", "\\\\").replace("\"", "\\\"")
                                             .replace("\n", "\\n") +
                    QByteArrayLiteral("\"}");
                const QByteArray headers =
                    QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ") +
                    QByteArray::number(body.size()) + QByteArrayLiteral("\r\nConnection: close\r\n\r\n");
                socket->write(headers + body);
                socket->flush();
            });

    qputenv("AEROSIM_OLLAMA_ENDPOINT",
            QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
    ScenarioEngine engine;
    QCOMPARE(engine.entityCount(), 0);
    QVERIFY(engine.generateScenarioWithAI(QStringLiteral("Add 2 aircraft at Hindon Airbase")));
    QTRY_COMPARE_WITH_TIMEOUT(engine.entityCount(), 2, 3000);
    QCOMPARE(engine.statusMessage(), QStringLiteral("GENERATED"));
    qunsetenv("AEROSIM_OLLAMA_ENDPOINT");
}

void AirbaseScenarioTests::ollamaValidationFailureClearsPending()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::AnyIPv4));
    connect(&server, &QTcpServer::newConnection, &server, [&server]()
            {
                QTcpSocket *socket = server.nextPendingConnection();
                const QByteArray body = QByteArrayLiteral("{\"response\":\"not json\"}");
                socket->write(QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ") +
                              QByteArray::number(body.size()) +
                              QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + body);
                socket->flush();
            });
    qputenv("AEROSIM_OLLAMA_ENDPOINT",
            QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
    ScenarioEngine engine;
    QVERIFY(engine.generateScenarioWithAI(QStringLiteral("query scenario")));
    QTRY_VERIFY_WITH_TIMEOUT(!engine.aiRequestPending(), 6000);
    QVERIFY(engine.statusMessage().contains(QStringLiteral("JSON")));
    QCOMPARE(engine.entityCount(), 0);
    qunsetenv("AEROSIM_OLLAMA_ENDPOINT");
}

void AirbaseScenarioTests::ollamaExecutionFailureClearsPending()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::AnyIPv4));
    connect(&server, &QTcpServer::newConnection, &server, [&server]()
            {
                QTcpSocket *socket = server.nextPendingConnection();
                const QByteArray command =
                    QByteArrayLiteral("{\"operation\":\"MOVE_ENTITY\",\"entity_id\":\"A99\","
                                      "\"destination\":\"Delhi\"}");
                const QByteArray body = QByteArrayLiteral("{\"response\":\"") +
                                        QByteArray(command).replace("\\", "\\\\")
                                            .replace("\"", "\\\"") +
                                        QByteArrayLiteral("\"}");
                socket->write(QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ") +
                              QByteArray::number(body.size()) +
                              QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + body);
                socket->flush();
            });
    qputenv("AEROSIM_OLLAMA_ENDPOINT",
            QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
    ScenarioEngine engine;
    QVERIFY(engine.generateScenarioWithAI(QStringLiteral("move A99 to Delhi")));
    QTRY_VERIFY_WITH_TIMEOUT(!engine.aiRequestPending(), 3000);
    QVERIFY(engine.statusMessage().contains(QStringLiteral("AI COMMAND REJECTED")));
    QCOMPARE(engine.entityCount(), 0);
    qunsetenv("AEROSIM_OLLAMA_ENDPOINT");
}

void AirbaseScenarioTests::ollamaUnavailableFallbackRejectsInvalidPrompt()
{
    qputenv("AEROSIM_OLLAMA_ENDPOINT", QByteArrayLiteral("http://127.0.0.1:1"));
    ScenarioEngine engine;
    QVERIFY(engine.generateScenarioWithAI(QStringLiteral("Move A99 to Imaginary Region")));
    QTRY_VERIFY_WITH_TIMEOUT(!engine.aiRequestPending(), 6000);
    QVERIFY(engine.statusMessage().contains(QStringLiteral("DETERMINISTIC PARSER REJECTED")));
    QVERIFY(!engine.statusMessage().contains(QStringLiteral("EXECUTED")));
    QCOMPARE(engine.entityCount(), 0);
    qunsetenv("AEROSIM_OLLAMA_ENDPOINT");
}

void AirbaseScenarioTests::ollamaTimeoutCleansUpReply()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::AnyIPv4));
    connect(&server, &QTcpServer::newConnection, &server, [&server]()
            {
                QTcpSocket *socket = server.nextPendingConnection();
                socket->setParent(&server);
            });

    OllamaClient client;
    client.setEndpoint(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())));
    client.setTimeoutMs(1000);
    QSignalSpy failureSpy(&client, &OllamaClient::requestFailed);
    client.generate(QStringLiteral("timeout"));

    QTRY_COMPARE_WITH_TIMEOUT(failureSpy.count(), 1, 3000);
    QCOMPARE(failureSpy.at(0).at(1).toBool(), true);
    client.generate(QStringLiteral("second request"));
    QTRY_COMPARE_WITH_TIMEOUT(failureSpy.count(), 2, 3000);
}

QTEST_MAIN(AirbaseScenarioTests)
#include "airbase_tests.moc"
