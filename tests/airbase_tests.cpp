
#include "../core/ScenarioEngine.h"
#include "../core/FormationGenerator.h"
#include "../ai/AICommand.h"
#include "../ai/OllamaClient.h"
#include "../data/AirbaseRegistry.h"
#include "../data/ScenarioDatabase.h"
#include "../data/CsvExporter.h"
#include "../data/GeoJsonExporter.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QGeoCoordinate>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QTemporaryDir>
#include <QUrl>
#include <QtMath>
#include <QtTest>

#include <QSet>

namespace
{
    QString *databaseWarning = nullptr;

    class ScopedEnvironmentVariable final
    {
    public:
        ScopedEnvironmentVariable(const char *name, const QByteArray &value)
            : m_name(name), m_wasSet(qEnvironmentVariableIsSet(name)),
              m_previousValue(qgetenv(name))
        {
            qputenv(name, value);
        }

        ~ScopedEnvironmentVariable()
        {
            if (m_wasSet)
                qputenv(m_name.constData(), m_previousValue);
            else
                qunsetenv(m_name.constData());
        }

    private:
        QByteArray m_name;
        bool m_wasSet;
        QByteArray m_previousValue;
    };

    void captureDatabaseWarning(QtMsgType type, const QMessageLogContext &,
                                const QString &message)
    {
        if (databaseWarning && type == QtWarningMsg &&
            message.contains(QStringLiteral("removeDatabase")))
            *databaseWarning = message;
    }

    void queueOllamaResponse(QTcpServer &server, const QString &response)
    {
        const QByteArray body = QJsonDocument(
                                    QJsonObject{{QStringLiteral("response"), response}})
                                    .toJson(QJsonDocument::Compact);
        QObject::connect(&server, &QTcpServer::newConnection, &server,
                         [&server, body]()
                         {
                             QTcpSocket *socket = server.nextPendingConnection();
                             socket->write(QByteArrayLiteral(
                                               "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ") +
                                           QByteArray::number(body.size()) +
                                           QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + body);
                             socket->flush();
                         });
    }

    bool executeLiveOllamaCommand(ScenarioEngine &engine, const QString &prompt)
    {
        if (!engine.generateScenarioWithAI(prompt))
            return false;
        QElapsedTimer timeout;
        timeout.start();
        while (engine.aiRequestPending() && timeout.elapsed() < 90000)
            QTest::qWait(50);
        return !engine.aiRequestPending() &&
               engine.statusMessage().startsWith(
                   QStringLiteral("OLLAMA GENERATED JSON; COMMAND EXECUTED:"));
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
    void routeIntentParsingIsEquivalent_data();
    void routeIntentParsingIsEquivalent();
    void individualAircraftIntentParsing_data();
    void individualAircraftIntentParsing();
    void aircraftManagementIntentParsing();
    void aircraftDeletionPreservesUnrelatedData();
    void aircraftDeletionDisbandsFormationsAndKeepsIdsUnique();
    void formationMovementStopResumeAndDisband();
    void individualMovementResolvesVerifiedAirbases();
    void individualMovementHeadingFollowsCardinalRoutes();
    void individualAircraftKeepDistinctHeadingsAfterReroute();
    void headingWraparoundUsesShortestTurn();
    void multiRouteIntentParsingIsEquivalent_data();
    void multiRouteIntentParsingIsEquivalent();
    void naturalLanguageMultiWaypointRoute();
    void invalidNaturalLanguageRouteIsRejected();
    void aircraftCanBeAssignedNaturalLanguageRoute();
    void naturalLanguageRouteExports();
    void routeIntentGeneratesOneChennaiRoute_data();
    void routeIntentGeneratesOneChennaiRoute();
    void multiRouteIntentGeneratesOnlyRequestedRoutes();
    void individualMovementIsIsolatedAndUsesVerifiedRoute();
    void individualMovementToSulurFromLandedAircraft();
    void individualMovementPauseResumeStopAndCompletes();
    void individualDestinationReplacementAndUnknownRequests();
    void globalAndIndividualSimulationControlsRemainIndependent();
    void individualMovementRuntimeStatePersistsPaused();
    void addPromptPreservesExistingEntities();
    void generatePromptReplacesExistingScenario();
    void replacementResetsSimulationAndModelState();
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
    void individualModelCommandsAreValidated();
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
    void ollamaRequestUsesContextLimit_data();
    void ollamaRequestUsesContextLimit();
    void ollamaCreatePromptPreservesIntent_data();
    void ollamaCreatePromptPreservesIntent();
    void ollamaRegionAddCommandExecutesScenarioMutation();
    void ollamaAddPreservesExistingEntities();
    void ollamaRejectsModelRouteThatDropsRequestedRoute();
    void ollamaRejectedModelCommandDoesNotMutateScenario();
    void ollamaHttpFailureReportsReasonAndPreservesScenario();
    void ollamaCommandExecutesScenarioMutation();
    void ollamaValidationFailureClearsPending();
    void ollamaExecutionFailureClearsPending();
    void ollamaMovementCommandExecutesAuthoritatively();
    void ollamaRejectedMovementDoesNotExecuteFallback();
    void ollamaLiveMovementCommands();
    void ollamaLiveWorkflowCommands();
    void ollamaLiveBengaluruRoute();
    void ollamaLiveAircraftManagementCommands();
    void ollamaUnavailableFallbackRejectsInvalidPrompt();
    void ollamaFallbackCanBeDisabled();
    void ollamaTimeoutCleansUpReply();
    void ollamaUnavailableUsesParserFallback();
    void ollamaUnavailableSafelyExecutesIndividualMovement();
    void scenarioDatabaseSavesWithoutConnectionWarning();
    void latestScenarioRestoresEntities();
    void runtimeStatePersistsAcrossSaveAndLoad();
    void scheduledEventStatesPersistAcrossSaveAndLoad();
    void legacyScenarioWithoutRuntimeStateStillLoads();
    void invalidRuntimeStateDoesNotMutateEngine();
    void resetRestoresInitialStateAfterRuntimeLoad();
    void twoAircraftLineFormation();
    void fiveAircraftVFormation();
    void eightAircraftDiamondFormation();
    void tenAircraftColumnFormation();
    void formationOddEvenCountsHaveUniqueSlotsAndLeader();
    void formationLeaderMovesFollowersMaintainSlots();
    void formationFollowersTurnSmoothlyAndRespectSpeed();
    void formationFollowersPauseAndResumeWithLeader();

    // GeoJSON exporter regression tests
    void geoJsonExportsValidRouteAndMetadata();
    void geoJsonExportsFiveAircraftPositions();
    void geoJsonExportsAirbaseFormationAndRoute();
    void geoJsonRejectsInvalidCoordinates();
    void geoJsonSkipsSingleWaypointRoute();
    void geoJsonExportsScenarioWithoutRoutes();
    void geoJsonExportsEmptyScenario();
    void geoJsonWritesAndOverwritesFileUrl();
    void geoJsonWritesRelativePathAsAbsolutePath();
    void geoJsonWritesToDesktopAndDocuments();
    void geoJsonRejectsUnavailablePath();
    void csvExportsAircraftAndAirbases();
    void csvExportsEmptyScenario();
    void csvRejectsMissingStateAndUnavailablePath();
    void commandHistoryCapturesOriginalPromptsAndFailures();
    void commandHistorySurvivesScenarioMutations();
    void exportersIncludeMatchingCommandHistory();
    void exportsCurrentSimulationStateToBothFormats();
};

namespace
{
    std::vector<Entity> makeMovingFormation(const QString &type,
                                           const QGeoCoordinate &turnPoint = {})
    {
        constexpr double heading = 90.0;
        constexpr double spacingNm = 3.0;
        const QGeoCoordinate origin(12.9716, 77.5946);
        QVector<FormationSlot> formationSlots;
        QString error;
        if (!FormationGenerator::generate(3, type, origin, heading, spacingNm,
                                          &formationSlots, &error))
            return {};

        const QGeoCoordinate destination =
            turnPoint.isValid()
                ? turnPoint.atDistanceAndAzimuth(10.0 * 1852.0, 80.0)
                : origin.atDistanceAndAzimuth(10.0 * 1852.0, heading);
        std::vector<Entity> entities;
        entities.reserve(static_cast<std::size_t>(formationSlots.size()));
        const QString leaderId = QStringLiteral("F01");
        for (int index = 0; index < formationSlots.size(); ++index)
        {
            const FormationSlot &slot = formationSlots.at(index);
            QVector<Waypoint> route = {
                {QStringLiteral("FORMATION SLOT"), slot.coordinate.latitude(),
                 slot.coordinate.longitude(), 10000.0}};
            if (index == 0)
            {
                if (turnPoint.isValid())
                    route.push_back({QStringLiteral("TURN"), turnPoint.latitude(),
                                     turnPoint.longitude(), 10000.0});
                route.push_back({QStringLiteral("DESTINATION"),
                                 destination.latitude(), destination.longitude(),
                                 10000.0});
            }
            Entity entity(QStringLiteral("F%1").arg(index + 1, 2, 10, QLatin1Char('0')),
                          QStringLiteral("Formation aircraft %1").arg(index + 1),
                          QStringLiteral("aircraft"), slot.coordinate.latitude(),
                          slot.coordinate.longitude(), 10000.0, 450.0, heading,
                          QStringLiteral("ACTIVE"), QStringLiteral("F%1").arg(index + 1),
                          0.0, QStringLiteral("TEST"), route);
            entity.setFormation(slot.formationId, type, leaderId, slot.slot,
                                slot.offset, spacingNm, heading,
                                QStringLiteral("ACTIVE"));
            entities.push_back(std::move(entity));
        }
        return entities;
    }

    QGeoCoordinate followerTarget(const AircraftSimulationState &leader,
                                  const Entity &follower)
    {
        const FormationOffset offset = follower.formationOffset();
        const double distanceNm = qSqrt(offset.xNm * offset.xNm +
                                        offset.yNm * offset.yNm);
        const double bearing =
            leader.heading + qRadiansToDegrees(qAtan2(offset.xNm, offset.yNm));
        return QGeoCoordinate(leader.latitude, leader.longitude)
            .atDistanceAndAzimuth(distanceNm * 1852.0, bearing);
    }

    bool verifyFormation(int count, const QString &type)
    {
        QVector<FormationSlot> generatedSlots;
        QString error;
        if (!FormationGenerator::generate(
                count, type, QGeoCoordinate(12.9716, 77.5946),
                37.0, 3.0, &generatedSlots, &error) ||
            generatedSlots.size() != count)
            return false;

        const FormationSlot &leader = generatedSlots.first();
        if (leader.formationId.isEmpty() || leader.leaderSlotId.isEmpty() ||
            !leader.isLeader || leader.slot != 0 ||
            leader.slotId != leader.leaderSlotId)
            return false;

        QSet<QString> slotIds;
        for (int index = 0; index < count; ++index)
        {
            const FormationSlot &slot = generatedSlots.at(index);
            if (slot.slot != index || slot.formationId != leader.formationId ||
                slot.leaderSlotId != leader.leaderSlotId ||
                slot.isLeader != (index == 0) || slot.slotId.isEmpty() ||
                slotIds.contains(slot.slotId) || !slot.coordinate.isValid())
                return false;
            slotIds.insert(slot.slotId);
            for (int previous = 0; previous < index; ++previous)
                if (slot.coordinate.distanceTo(
                        generatedSlots.at(previous).coordinate) < 100.0)
                    return false;
        }

        QVector<FormationSlot> repeated;
        if (!FormationGenerator::generate(
                count, type, QGeoCoordinate(12.9716, 77.5946),
                37.0, 3.0, &repeated, &error))
            return false;
        for (int index = 0; index < count; ++index)
            if (generatedSlots.at(index).coordinate != repeated.at(index).coordinate ||
                generatedSlots.at(index).offset.xNm != repeated.at(index).offset.xNm ||
                generatedSlots.at(index).offset.yNm != repeated.at(index).offset.yNm)
                return false;
        return true;
    }
}

void AirbaseScenarioTests::twoAircraftLineFormation()
{
    QVERIFY(verifyFormation(2, QStringLiteral("LINE")));
    QVector<FormationSlot> generated;
    QString error;
    const QGeoCoordinate reference(12.9716, 77.5946);
    QVERIFY(FormationGenerator::generate(
        2, QStringLiteral("LINE"), reference, 90.0, 3.0, &generated, &error));
    QCOMPARE(generated.at(1).offset.xNm, 3.0);
    QCOMPARE(generated.at(1).offset.yNm, 0.0);
    QVERIFY(generated.at(1).coordinate.latitude() < reference.latitude());
}

void AirbaseScenarioTests::fiveAircraftVFormation()
{
    QVERIFY(verifyFormation(5, QStringLiteral("V")));

    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(
        QStringLiteral("Create 5 aircraft in V formation.")));
    QCOMPARE(engine.entityCount(), 5);
    const std::vector<Entity> &entities = engine.scenario().entities();
    const QString leaderId = entities.front().id();
    const QString formationId = entities.front().formationId();
    QVERIFY(!formationId.isEmpty());
    for (int index = 0; index < static_cast<int>(entities.size()); ++index)
    {
        const Entity &aircraft = entities.at(static_cast<std::size_t>(index));
        QVERIFY(aircraft.isFormationMember());
        QCOMPARE(aircraft.formationId(), formationId);
        QCOMPARE(aircraft.formationLeaderId(), leaderId);
        QCOMPARE(aircraft.formationSlot(), index);
        QCOMPARE(aircraft.route().size(), 1);
        QCOMPARE(aircraft.id() == leaderId, index == 0);
    }

    const Entity restored = Entity::fromJson(entities.front().toJson());
    QCOMPARE(restored.formationId(), formationId);
    QCOMPARE(restored.formationLeaderId(), leaderId);
    QVERIFY(restored.isFormationMember());
}

void AirbaseScenarioTests::eightAircraftDiamondFormation()
{
    QVERIFY(verifyFormation(8, QStringLiteral("DIAMOND")));
}

void AirbaseScenarioTests::tenAircraftColumnFormation()
{
    QVERIFY(verifyFormation(10, QStringLiteral("COLUMN")));
}

void AirbaseScenarioTests::formationOddEvenCountsHaveUniqueSlotsAndLeader()
{
    const int counts[] = {2, 3, 4, 5, 8, 10};
    const QStringList additionalTypes = {
        QStringLiteral("INVERTED_V"), QStringLiteral("WEDGE"),
        QStringLiteral("ECHELON_LEFT"), QStringLiteral("ECHELON_RIGHT")};
    for (int count : counts)
        for (const QString &type : additionalTypes)
            QVERIFY(verifyFormation(count, type));

    QVector<FormationSlot> generatedSlots;
    QString error;
    QVERIFY(!FormationGenerator::generate(
        1, QStringLiteral("V"), QGeoCoordinate(12.9716, 77.5946),
        90.0, 3.0, &generatedSlots, &error));
    QVERIFY(!FormationGenerator::generate(
        5, QStringLiteral("unsupported"), QGeoCoordinate(12.9716, 77.5946),
        90.0, 3.0, &generatedSlots, &error));
}

void AirbaseScenarioTests::formationLeaderMovesFollowersMaintainSlots()
{
    const std::vector<Entity> entities = makeMovingFormation(QStringLiteral("LINE"));
    QVERIFY(entities.size() == 3);
    SimulationEngine simulation;
    simulation.setScenario(entities, false);
    QVERIFY(simulation.play());

    AircraftSimulationState leaderBefore;
    AircraftSimulationState followerBefore;
    QVERIFY(simulation.aircraftState(QStringLiteral("F01"), &leaderBefore));
    QVERIFY(simulation.aircraftState(QStringLiteral("F02"), &followerBefore));
    QVERIFY(simulation.advanceSimulationForTest(1.0));

    AircraftSimulationState leaderAfter;
    AircraftSimulationState followerAfter;
    QVERIFY(simulation.aircraftState(QStringLiteral("F01"), &leaderAfter));
    QVERIFY(simulation.aircraftState(QStringLiteral("F02"), &followerAfter));
    QVERIFY(QGeoCoordinate(leaderBefore.latitude, leaderBefore.longitude)
                .distanceTo(QGeoCoordinate(leaderAfter.latitude,
                                           leaderAfter.longitude)) > 100.0);
    const QGeoCoordinate target = followerTarget(leaderAfter, entities.at(1));
    QVERIFY(QGeoCoordinate(followerAfter.latitude, followerAfter.longitude)
                .distanceTo(target) < 5.0);
    simulation.pause();
}

void AirbaseScenarioTests::formationFollowersTurnSmoothlyAndRespectSpeed()
{
    const QGeoCoordinate origin(12.9716, 77.5946);
    const QGeoCoordinate turnPoint =
        origin.atDistanceAndAzimuth(1852.0, 90.0);
    const std::vector<Entity> entities =
        makeMovingFormation(QStringLiteral("LINE"), turnPoint);
    QVERIFY(entities.size() == 3);
    SimulationEngine simulation;
    simulation.setScenario(entities, false);
    QString error;
    QVERIFY2(simulation.startAircraft(QStringLiteral("F01"), &error), qPrintable(error));
    QVERIFY(simulation.advanceSimulationForTest(8.0));

    AircraftSimulationState followerBefore;
    QVERIFY(simulation.aircraftState(QStringLiteral("F02"), &followerBefore));
    QVERIFY(simulation.advanceSimulationForTest(1.0));

    AircraftSimulationState leaderAfter;
    AircraftSimulationState followerAfter;
    QVERIFY(simulation.aircraftState(QStringLiteral("F01"), &leaderAfter));
    QVERIFY(simulation.aircraftState(QStringLiteral("F02"), &followerAfter));
    const double followerTravel = QGeoCoordinate(followerBefore.latitude,
                                                  followerBefore.longitude)
                                     .distanceTo(QGeoCoordinate(
                                         followerAfter.latitude,
                                         followerAfter.longitude));
    const QGeoCoordinate targetBefore = followerTarget(
        followerBefore, entities.at(1));
    QVERIFY(followerTravel <= 450.0 * 1852.0 / 3600.0 + 1.0);
    QVERIFY(followerTravel > 0.0);
    QVERIFY(qAbs(leaderAfter.heading - 80.0) < 1.0);
    const QGeoCoordinate target = followerTarget(leaderAfter, entities.at(1));
    QVERIFY(targetBefore.distanceTo(target) > 100.0);
    QVERIFY(QGeoCoordinate(followerAfter.latitude, followerAfter.longitude)
                .distanceTo(target) < 1500.0);
}

void AirbaseScenarioTests::formationFollowersPauseAndResumeWithLeader()
{
    const std::vector<Entity> entities = makeMovingFormation(QStringLiteral("LINE"));
    QVERIFY(entities.size() == 3);
    SimulationEngine simulation;
    simulation.setScenario(entities, false);
    QVERIFY(simulation.play());
    QString error;
    QVERIFY2(simulation.startAircraft(QStringLiteral("F01"), &error), qPrintable(error));
    QVERIFY(simulation.advanceSimulationForTest(1.0));
    QVERIFY2(simulation.pauseAircraft(QStringLiteral("F01"), &error), qPrintable(error));

    AircraftSimulationState followerBeforePause;
    QVERIFY(simulation.aircraftState(QStringLiteral("F02"), &followerBeforePause));
    QVERIFY(simulation.advanceSimulationForTest(1.0));
    AircraftSimulationState followerWhilePaused;
    QVERIFY(simulation.aircraftState(QStringLiteral("F02"), &followerWhilePaused));
    QCOMPARE(followerWhilePaused.latitude, followerBeforePause.latitude);
    QCOMPARE(followerWhilePaused.longitude, followerBeforePause.longitude);

    QVERIFY2(simulation.resumeAircraft(QStringLiteral("F01"), &error), qPrintable(error));
    QVERIFY(simulation.advanceSimulationForTest(1.0));
    AircraftSimulationState followerAfterResume;
    QVERIFY(simulation.aircraftState(QStringLiteral("F02"), &followerAfterResume));
    QVERIFY(QGeoCoordinate(followerWhilePaused.latitude,
                           followerWhilePaused.longitude)
                .distanceTo(QGeoCoordinate(followerAfterResume.latitude,
                                           followerAfterResume.longitude)) > 0.0);
    simulation.pause();
}

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

void AirbaseScenarioTests::routeIntentParsingIsEquivalent_data()
{
    QTest::addColumn<QString>("prompt");
    QTest::newRow("exact Bengaluru to Chennai request")
        << QStringLiteral("Create 5 aircraft in Bengaluru and route one aircraft from Bengaluru to Chennai.");
    QTest::newRow("requested wording")
        << QStringLiteral("Create 5 aircraft near Bengaluru and send one toward Chennai.");
    QTest::newRow("numeric and Bangalore alias")
        << QStringLiteral("Generate 5 planes around Bangalore; route one aircraft to Chennai.");
    QTest::newRow("spelled-out count")
        << QStringLiteral("Make five aircraft near Bengaluru with one aircraft flying towards Chennai.");
}

void AirbaseScenarioTests::routeIntentParsingIsEquivalent()
{
    QFETCH(QString, prompt);
    AIParser parser;
    ScenarioRequest request;
    QString error;
    QVERIFY2(parser.parsePrompt(prompt, request, &error), qPrintable(error));
    QCOMPARE(request.operation, ScenarioOperation::Add);
    QCOMPARE(request.entityCount, 5);
    QCOMPARE(request.entityType, QStringLiteral("aircraft"));
    QCOMPARE(request.region, QStringLiteral("Bengaluru"));
    QCOMPARE(request.locationName, QStringLiteral("Bengaluru"));
    QCOMPARE(request.routeDestinations, QStringList{QStringLiteral("Chennai")});
    QCOMPARE(request.entities.size(), 1);
    QCOMPARE(request.entities.first().count, 5);
}

void AirbaseScenarioTests::individualAircraftIntentParsing_data()
{
    QTest::addColumn<QString>("prompt");
    QTest::addColumn<ScenarioOperation>("operation");
    QTest::addColumn<QString>("aircraft");
    QTest::addColumn<QString>("destination");

    QTest::newRow("move from current position")
        << QStringLiteral("Move AERO002 from its current position to Pune Airbase.")
        << ScenarioOperation::Move << QStringLiteral("AERO002") << QStringLiteral("pune airbase");
    QTest::newRow("send to airbase")
        << QStringLiteral("Send AERO001 to Bengaluru Airbase.")
        << ScenarioOperation::Move << QStringLiteral("AERO001") << QStringLiteral("bengaluru airbase");
    QTest::newRow("route from current location")
        << QStringLiteral("Route AERO003 from its current location to Chennai.")
        << ScenarioOperation::Move << QStringLiteral("AERO003") << QStringLiteral("chennai");
    QTest::newRow("change destination")
        << QStringLiteral("Change AERO002's destination to Goa.")
        << ScenarioOperation::ChangeRoute << QStringLiteral("AERO002") << QStringLiteral("goa");
    QTest::newRow("contraction and Yelahanka airbase")
        << QStringLiteral("Move AERO004 from it's location to Yelahanka Airbase")
        << ScenarioOperation::Move << QStringLiteral("AERO004")
        << QStringLiteral("yelahanka airbase");
    QTest::newRow("Nal Bikaner airbase")
        << QStringLiteral("Move AERO002 to Nal/Bikaner Airbase")
        << ScenarioOperation::Move << QStringLiteral("AERO002")
        << QStringLiteral("nal/bikaner airbase");
    QTest::newRow("Chennai bearing")
        << QStringLiteral("Move AERO003 towards Chennai")
        << ScenarioOperation::Move << QStringLiteral("AERO003")
        << QStringLiteral("chennai");
    QTest::newRow("stop")
        << QStringLiteral("Stop AERO002.")
        << ScenarioOperation::StopAircraft << QStringLiteral("AERO002") << QString();
    QTest::newRow("resume")
        << QStringLiteral("Resume AERO002")
        << ScenarioOperation::ResumeAircraft << QStringLiteral("AERO002") << QString();
}

void AirbaseScenarioTests::individualAircraftIntentParsing()
{
    QFETCH(QString, prompt);
    QFETCH(ScenarioOperation, operation);
    QFETCH(QString, aircraft);
    QFETCH(QString, destination);
    AirbaseRegistry registry;
    AIParser parser(&registry);
    ScenarioRequest request;
    QString error;
    QVERIFY2(parser.parsePrompt(prompt, request, &error), qPrintable(error));
    QCOMPARE(request.operation, operation);
    QCOMPARE(request.operationTargetId, aircraft);
    QCOMPARE(request.operationLocation, destination);
}

void AirbaseScenarioTests::aircraftManagementIntentParsing()
{
    const struct
    {
        const char *prompt;
        ScenarioOperation operation;
    } commands[] = {
        {"Remove all aircraft.", ScenarioOperation::RemoveAllAircraft},
        {"Remove aircraft AERO001.", ScenarioOperation::RemoveAircraft},
        {"Remove last 3 aircraft.", ScenarioOperation::RemoveLastAircraft},
        {"Stop all aircraft.", ScenarioOperation::StopAllAircraft},
        {"Move the V formation towards Chennai.",
         ScenarioOperation::MoveFormation},
        {"Move all formations towards Chennai.",
         ScenarioOperation::MoveFormation},
        {"Stop the V formation.", ScenarioOperation::StopFormation},
        {"Resume the formation.", ScenarioOperation::ResumeFormation},
        {"Disband the formation.", ScenarioOperation::DisbandFormation}};

    AIParser parser;
    for (const auto &command : commands)
    {
        ScenarioRequest request;
        QString error;
        QVERIFY2(parser.parsePrompt(QString::fromLatin1(command.prompt),
                                    request, &error),
                 qPrintable(error));
        QCOMPARE(request.operation, command.operation);
    }

    ScenarioRequest request;
    QString error;
    QVERIFY(parser.parsePrompt(QStringLiteral("Remove all aircraft"),
                               request, &error));
    QCOMPARE(request.operation, ScenarioOperation::RemoveAllAircraft);
    QVERIFY(request.operationTargetId.isEmpty());
    QVERIFY(parser.parsePrompt(QStringLiteral("Move the V formation towards Chennai"),
                               request, &error));
    QVERIFY(request.formationTypeSpecified);
    QCOMPARE(request.formationType, QStringLiteral("V"));
    QVERIFY(parser.parsePrompt(QStringLiteral("Move all formations towards Chennai"),
                               request, &error));
    QVERIFY(request.allFormations);

    const QVector<QJsonObject> modelCommands = {
        QJsonObject{{QStringLiteral("operation"), QStringLiteral("REMOVE_ALL_AIRCRAFT")}},
        QJsonObject{{QStringLiteral("operation"), QStringLiteral("REMOVE_AIRCRAFT")},
                    {QStringLiteral("entity_id"), QStringLiteral("AERO001")}},
        QJsonObject{{QStringLiteral("operation"), QStringLiteral("REMOVE_LAST_AIRCRAFT")},
                    {QStringLiteral("count"), 3}},
        QJsonObject{{QStringLiteral("operation"), QStringLiteral("STOP_ALL_AIRCRAFT")}},
        QJsonObject{{QStringLiteral("operation"), QStringLiteral("MOVE_FORMATION")},
                    {QStringLiteral("formation_type"), QStringLiteral("V")},
                    {QStringLiteral("destination"), QStringLiteral("Chennai")}},
        QJsonObject{{QStringLiteral("operation"), QStringLiteral("STOP_FORMATION")},
                    {QStringLiteral("formation_type"), QStringLiteral("V")}},
        QJsonObject{{QStringLiteral("operation"), QStringLiteral("RESUME_FORMATION")}},
        QJsonObject{{QStringLiteral("operation"), QStringLiteral("DISBAND_FORMATION")}}};
    for (const QJsonObject &modelCommand : modelCommands)
    {
        QVERIFY2(AICommand::fromJson(modelCommand, request, &error),
                 qPrintable(error));
        QString prompt;
        QVERIFY2(AICommand::toPrompt(request, &prompt, &error), qPrintable(error));
        ScenarioRequest parsedPrompt;
        QVERIFY2(parser.parsePrompt(prompt, parsedPrompt, &error), qPrintable(error));
        QCOMPARE(parsedPrompt.operation, request.operation);
    }
}

void AirbaseScenarioTests::aircraftDeletionPreservesUnrelatedData()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(
        QStringLiteral("Generate 2 aircraft and 1 radar site in North India")));
    QCOMPARE(engine.entityCount(), 3);
    const int airbaseCount = engine.airbaseModel()->rowCount();
    QVERIFY(engine.generateScenario(
        QStringLiteral("Schedule AERO001 to take off at 10:30")));
    QCOMPARE(engine.scheduledTakeoffs().size(), 1);

    QVERIFY2(engine.generateScenario(QStringLiteral("Remove aircraft AERO001")),
             qPrintable(engine.statusMessage()));
    QCOMPARE(engine.entityCount(), 2);
    QCOMPARE(engine.entityModel()->rowCount(), 2);
    QVERIFY(engine.aircraftInformation(QStringLiteral("A01")).isEmpty());
    QCOMPARE(engine.scheduledTakeoffs().size(), 0);

    int remainingAircraft = 0;
    bool radarSiteRemains = false;
    for (const Entity &entity : engine.scenario().entities())
    {
        remainingAircraft += entity.type() == QStringLiteral("aircraft") ? 1 : 0;
        radarSiteRemains = radarSiteRemains ||
                           entity.type() == QStringLiteral("radar_site");
    }
    QCOMPARE(remainingAircraft, 1);
    QVERIFY(radarSiteRemains);

    QVERIFY(engine.generateScenario(QStringLiteral("Remove all aircraft.")));
    QCOMPARE(engine.entityCount(), 1);
    QCOMPARE(engine.entityModel()->rowCount(), 1);
    QVERIFY(engine.scenario().entities().front().type() ==
            QStringLiteral("radar_site"));
    QCOMPARE(engine.airbaseModel()->rowCount(), airbaseCount);
    QVERIFY(engine.scenarioGenerated());

    QVERIFY(engine.generateScenario(QStringLiteral("Remove all aircraft.")));
    QCOMPARE(engine.entityCount(), 1);
    QVERIFY(engine.statusMessage().contains(QStringLiteral("NO AIRCRAFT")));

    ScenarioEngine lastAircraft;
    QVERIFY(lastAircraft.generateScenario(
        QStringLiteral("Generate 4 aircraft at Hindon Airbase")));
    QVERIFY(lastAircraft.generateScenario(QStringLiteral("Remove last 2 aircraft")));
    QCOMPARE(lastAircraft.entityCount(), 2);
    QCOMPARE(lastAircraft.entityModel()->rowCount(), 2);
    QCOMPARE(lastAircraft.scenario().entities().at(0).id(), QStringLiteral("A01"));
    QCOMPARE(lastAircraft.scenario().entities().at(1).id(), QStringLiteral("A02"));

    ScenarioEngine emptyAfterRemoval;
    QVERIFY(emptyAfterRemoval.generateScenario(
        QStringLiteral("Generate 2 aircraft at Hindon Airbase")));
    const int emptyAirbaseCount = emptyAfterRemoval.airbaseModel()->rowCount();
    QVERIFY(emptyAfterRemoval.generateScenario(QStringLiteral("Remove all aircraft")));
    QCOMPARE(emptyAfterRemoval.entityCount(), 0);
    QCOMPARE(emptyAfterRemoval.entityModel()->rowCount(), 0);
    QVERIFY(!emptyAfterRemoval.scenarioGenerated());
    QVERIFY(emptyAfterRemoval.aircraftInformation(QStringLiteral("A01")).isEmpty());
    QCOMPARE(emptyAfterRemoval.airbaseModel()->rowCount(), emptyAirbaseCount);
}

void AirbaseScenarioTests::aircraftDeletionDisbandsFormationsAndKeepsIdsUnique()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(
        QStringLiteral("Create 5 aircraft in a V formation")));
    QVERIFY(engine.generateScenario(QStringLiteral("Remove aircraft AERO001")));
    QCOMPARE(engine.entityCount(), 4);
    for (const Entity &entity : engine.scenario().entities())
        QVERIFY(!entity.isFormationMember());
    QVERIFY(engine.aircraftInformation(QStringLiteral("A01")).isEmpty());

    QVERIFY(engine.generateScenario(QStringLiteral("Add 1 aircraft at Hindon Airbase")));
    QCOMPARE(engine.entityCount(), 5);
    QSet<QString> ids;
    QSet<QString> callsigns;
    for (const Entity &entity : engine.scenario().entities())
    {
        QVERIFY(!ids.contains(entity.id().toUpper()));
        ids.insert(entity.id().toUpper());
        if (entity.type() == QStringLiteral("aircraft"))
        {
            QVERIFY(!callsigns.contains(entity.callsign().toUpper()));
            callsigns.insert(entity.callsign().toUpper());
        }
    }
}

void AirbaseScenarioTests::formationMovementStopResumeAndDisband()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(
        QStringLiteral("Create 5 aircraft in a V formation")));
    QVERIFY(engine.generateScenario(
        QStringLiteral("Move the V formation towards Chennai.")));

    QString formationId;
    QString leaderId;
    for (const Entity &entity : engine.scenario().entities())
        if (entity.isFormationMember())
        {
            formationId = entity.formationId();
            if (entity.id() == entity.formationLeaderId())
            {
                leaderId = entity.id();
                QCOMPARE(entity.route().last().latitude, 13.0827);
                QCOMPARE(entity.route().last().longitude, 80.2707);
            }
        }
    QVERIFY(!formationId.isEmpty());
    QVERIFY(!leaderId.isEmpty());

    QVERIFY(engine.generateScenario(QStringLiteral("Stop the V formation.")));
    QCOMPARE(engine.aircraftInformation(leaderId)
                 .value(QStringLiteral("movementStatus")).toString(),
             QStringLiteral("STATIONARY"));
    QVERIFY(engine.generateScenario(QStringLiteral("Resume the formation.")));
    QCOMPARE(engine.aircraftInformation(leaderId)
                 .value(QStringLiteral("movementStatus")).toString(),
             QStringLiteral("MOVING"));
    QVERIFY(engine.generateScenario(QStringLiteral("Disband the formation.")));
    QCOMPARE(engine.entityCount(), 5);
    for (const Entity &entity : engine.scenario().entities())
        QVERIFY(!entity.isFormationMember());

    QVERIFY(engine.generateScenario(
        QStringLiteral("Create 3 aircraft in a V formation")));
    QVERIFY(engine.generateScenario(
        QStringLiteral("Create 3 aircraft in a line formation")));
    QVERIFY(!engine.generateScenario(
        QStringLiteral("Move the formation towards Chennai")));
    QVERIFY(engine.generateScenario(
        QStringLiteral("Move all formations towards Chennai")));

    int movedLeaders = 0;
    for (const Entity &entity : engine.scenario().entities())
        if (entity.isFormationMember() &&
            entity.id() == entity.formationLeaderId())
        {
            ++movedLeaders;
            QCOMPARE(entity.route().last().latitude, 13.0827);
            QCOMPARE(entity.route().last().longitude, 80.2707);
        }
    QCOMPARE(movedLeaders, 2);
}

void AirbaseScenarioTests::individualMovementResolvesVerifiedAirbases()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 4 aircraft at Hindon Airbase")));

    const QStringList commands = {
        QStringLiteral("Move AERO004 from it's location to Yelahanka Airbase"),
        QStringLiteral("Move AERO002 to Nal/Bikaner Airbase"),
        QStringLiteral("Move AERO001 to Pune Airbase"),
        QStringLiteral("Move AERO003 towards Chennai")};
    const QVector<QPair<double, double>> destinations = {
        {13.135, 77.606}, {28.0706, 73.2072}, {18.582, 73.919},
        {13.0827, 80.2707}};
    const QVector<QString> aircraftIds = {
        QStringLiteral("A04"), QStringLiteral("A02"),
        QStringLiteral("A01"), QStringLiteral("A03")};

    for (int index = 0; index < commands.size(); ++index)
    {
        const QVariantMap before =
            engine.aircraftInformation(aircraftIds.at(index));
        QVERIFY2(engine.generateScenario(commands.at(index)),
                 qPrintable(engine.statusMessage()));
        const int row = aircraftIds.at(index).right(2).toInt() - 1;
        const Entity &entity = engine.scenario().entities().at(
            static_cast<std::size_t>(row));
        QVERIFY(!entity.route().isEmpty());
        QCOMPARE(entity.route().first().latitude,
                 before.value(QStringLiteral("latitude")).toDouble());
        QCOMPARE(entity.route().first().longitude,
                 before.value(QStringLiteral("longitude")).toDouble());
        QCOMPARE(entity.route().last().latitude, destinations.at(index).first);
        QCOMPARE(entity.route().last().longitude, destinations.at(index).second);
    }

    QVERIFY(!engine.generateScenario(QStringLiteral("Move AERO004 to Unknown Airbase")));
    QVERIFY(engine.statusMessage().contains(QStringLiteral("DESTINATION")));
}

void AirbaseScenarioTests::individualMovementHeadingFollowsCardinalRoutes()
{
    const struct
    {
        const char *name;
        double latitude;
        double longitude;
        double expectedHeading;
    } cases[] = {
        {"north", 1.0, 0.0, 0.0},
        {"east", 0.0, 1.0, 90.0},
        {"south", -1.0, 0.0, 180.0},
        {"west", 0.0, -1.0, 270.0}};

    for (const auto &testCase : cases)
    {
        SimulationEngine simulation;
        const QVector<Waypoint> route = {
            {QStringLiteral("origin"), 0.0, 0.0, 10000.0},
            {QStringLiteral("destination"), testCase.latitude,
             testCase.longitude, 10000.0}};
        Entity entity(QStringLiteral("A01"), QStringLiteral("Test aircraft"),
                      QStringLiteral("aircraft"), 0.0, 0.0, 10000.0,
                      120.0, 0.0, QStringLiteral("ACTIVE"),
                      QStringLiteral("AERO001"), 0.0, QStringLiteral("test"),
                      route, false);
        simulation.setScenario({entity});
        QString error;
        QVERIFY2(simulation.assignIndividualRoute(QStringLiteral("A01"), route,
                                                   &error),
                 qPrintable(error));
        QVERIFY(simulation.advanceSimulationForTest(2.0));

        AircraftSimulationState state;
        QVERIFY(simulation.aircraftState(QStringLiteral("A01"), &state));
        QVERIFY2(qAbs(state.heading - testCase.expectedHeading) < 0.01,
                 testCase.name);
        QCOMPARE(simulation.entityModel()->data(simulation.entityModel()->index(0),
                                                 EntityModel::HeadingRole).toDouble(),
                 state.heading);
    }
}

void AirbaseScenarioTests::headingWraparoundUsesShortestTurn()
{
    SimulationEngine simulation;
    const QVector<Waypoint> route = {
        {QStringLiteral("origin"), 0.0, 0.0, 10000.0},
        {QStringLiteral("destination"), 1.0, 0.017455, 10000.0}};
    Entity entity(QStringLiteral("A01"), QStringLiteral("Test aircraft"),
                  QStringLiteral("aircraft"), 0.0, 0.0, 10000.0,
                  120.0, 359.0, QStringLiteral("ACTIVE"),
                  QStringLiteral("AERO001"), 0.0, QStringLiteral("test"),
                  route, false);
    simulation.setScenario({entity});
    QString error;
    QVERIFY2(simulation.assignIndividualRoute(QStringLiteral("A01"), route, &error),
             qPrintable(error));
    QVERIFY(simulation.advanceSimulationForTest(0.1));

    AircraftSimulationState state;
    QVERIFY(simulation.aircraftState(QStringLiteral("A01"), &state));
    QVERIFY(state.heading < 5.0 || state.heading > 355.0);
}

void AirbaseScenarioTests::individualAircraftKeepDistinctHeadingsAfterReroute()
{
    const QVector<Waypoint> northRoute = {
        {QStringLiteral("origin"), 0.0, 0.0, 10000.0},
        {QStringLiteral("north"), 1.0, 0.0, 10000.0}};
    const QVector<Waypoint> eastRoute = {
        {QStringLiteral("origin"), 0.0, 0.0, 10000.0},
        {QStringLiteral("east"), 0.0, 1.0, 10000.0}};
    const QVector<Waypoint> westRoute = {
        {QStringLiteral("origin"), 0.0, 0.0, 10000.0},
        {QStringLiteral("west"), 0.0, -1.0, 10000.0}};
    const Entity northbound(QStringLiteral("A01"), QStringLiteral("Northbound"),
                            QStringLiteral("aircraft"), 0.0, 0.0, 10000.0,
                            120.0, 0.0, QStringLiteral("ACTIVE"),
                            QStringLiteral("AERO001"), 0.0, QStringLiteral("test"),
                            northRoute, false);
    const Entity eastbound(QStringLiteral("A02"), QStringLiteral("Eastbound"),
                           QStringLiteral("aircraft"), 0.0, 0.0, 10000.0,
                           120.0, 0.0, QStringLiteral("ACTIVE"),
                           QStringLiteral("AERO002"), 0.0, QStringLiteral("test"),
                           eastRoute, false);
    SimulationEngine simulation;
    simulation.setScenario({northbound, eastbound});
    QString error;
    QVERIFY2(simulation.assignIndividualRoute(QStringLiteral("A01"), northRoute, &error),
             qPrintable(error));
    QVERIFY2(simulation.assignIndividualRoute(QStringLiteral("A02"), eastRoute, &error),
             qPrintable(error));
    QVERIFY(simulation.advanceSimulationForTest(0.5));

    AircraftSimulationState northState;
    AircraftSimulationState eastState;
    QVERIFY(simulation.aircraftState(QStringLiteral("A01"), &northState));
    QVERIFY(simulation.aircraftState(QStringLiteral("A02"), &eastState));
    QCOMPARE(northState.heading, 0.0);
    QCOMPARE(eastState.heading, 90.0);

    QVector<Waypoint> reroute = {
        {QStringLiteral("origin"), eastState.latitude, eastState.longitude, 10000.0},
        {QStringLiteral("west"), 0.0, -1.0, 10000.0}};
    QVERIFY2(simulation.assignIndividualRoute(QStringLiteral("A02"), reroute, &error),
             qPrintable(error));
    QVERIFY(simulation.advanceSimulationForTest(0.5));
    QVERIFY(simulation.aircraftState(QStringLiteral("A02"), &eastState));
    QVERIFY(eastState.heading < 1.0 || eastState.heading > 359.0);
    QVERIFY(simulation.aircraftState(QStringLiteral("A01"), &northState));
    QCOMPARE(northState.heading, 0.0);
}

void AirbaseScenarioTests::multiRouteIntentParsingIsEquivalent_data()
{
    QTest::addColumn<QString>("prompt");
    QTest::newRow("requested wording")
        << QStringLiteral("Create 3 aircraft near Mumbai, send one to Pune, and make another fly toward Goa.");
    QTest::newRow("alternate wording")
        << QStringLiteral("Generate three planes around Bombay; route one to Pune and direct another towards Goa.");
}

void AirbaseScenarioTests::multiRouteIntentParsingIsEquivalent()
{
    QFETCH(QString, prompt);
    AIParser parser;
    ScenarioRequest request;
    QString error;
    QVERIFY2(parser.parsePrompt(prompt, request, &error), qPrintable(error));
    QCOMPARE(request.entityCount, 3);
    QCOMPARE(request.entityType, QStringLiteral("aircraft"));
    QCOMPARE(request.region, QStringLiteral("Mumbai"));
    QCOMPARE(request.locationName, QStringLiteral("Mumbai"));
    QCOMPARE(request.entities.size(), 1);
    QCOMPARE(request.entities.first().count, 3);
    QCOMPARE(request.routeDestinations,
             QStringList({QStringLiteral("Pune"), QStringLiteral("Goa")}));
}

void AirbaseScenarioTests::naturalLanguageMultiWaypointRoute()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(
        QStringLiteral("Create a route from Bengaluru to Hyderabad to Mumbai to Delhi.")));
    QCOMPARE(engine.scenario().routes().size(), 1);
    const ScenarioRoute &route = engine.scenario().routes().first();
    QCOMPARE(route.waypoints.size(), 4);
    QCOMPARE(route.waypoints.at(0).name, QStringLiteral("Bengaluru"));
    QCOMPARE(route.waypoints.at(1).name, QStringLiteral("Hyderabad"));
    QCOMPARE(route.waypoints.at(2).name, QStringLiteral("Mumbai"));
    QCOMPARE(route.waypoints.at(3).name, QStringLiteral("Delhi"));
    QCOMPARE(route.waypoints.at(0).longitude, 77.5946);
    QCOMPARE(route.waypoints.at(3).latitude, 28.6139);
    QCOMPARE(engine.routeWaypoints().size(), 4);
}

void AirbaseScenarioTests::invalidNaturalLanguageRouteIsRejected()
{
    ScenarioEngine engine;
    QVERIFY(!engine.generateScenario(
        QStringLiteral("Create a route from Bengaluru to Unknownville")));
    QCOMPARE(engine.scenario().routes().size(), 0);
    QVERIFY(engine.generateScenario(
        QStringLiteral("Create a route from Bengaluru to Chennai")));
    QVERIFY(!engine.generateScenario(
        QStringLiteral("Create a route from Bengaluru to Bengaluru")));
    QCOMPARE(engine.scenario().routes().size(), 1);
}

void AirbaseScenarioTests::aircraftCanBeAssignedNaturalLanguageRoute()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(
        QStringLiteral("Generate one aircraft in Bengaluru")));
    QVERIFY(engine.generateScenario(
        QStringLiteral("Assign the route from Bengaluru to Chennai to aircraft A01")));
    QCOMPARE(engine.scenario().routes().size(), 1);
    QCOMPARE(engine.scenario().routes().first().aircraftId, QStringLiteral("A01"));
    QCOMPARE(engine.scenario().entities().at(0).route().last().name,
             QStringLiteral("Chennai"));
}

void AirbaseScenarioTests::naturalLanguageRouteExports()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(
        QStringLiteral("Create a route from Bengaluru to Hyderabad to Mumbai")));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString geoJsonPath = directory.filePath(QStringLiteral("route.geojson"));
    const QString csvPath = directory.filePath(QStringLiteral("route.csv"));
    QVERIFY(engine.exportScenarioGeoJson(geoJsonPath));
    QVERIFY(engine.exportScenarioCsv(csvPath));

    QFile geoJsonFile(geoJsonPath);
    QVERIFY(geoJsonFile.open(QIODevice::ReadOnly));
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(geoJsonFile.readAll(), &parseError);
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    QCOMPARE(document.object().value(QStringLiteral("type")).toString(),
             QStringLiteral("FeatureCollection"));
    bool foundRoute = false;
    for (const QJsonValue &feature : document.object().value(QStringLiteral("features")).toArray())
        if (feature.toObject().value(QStringLiteral("properties"))
                    .toObject()
                    .value(QStringLiteral("featureKind"))
                    .toString() == QStringLiteral("scenarioRoute"))
            foundRoute = true;
    QVERIFY(foundRoute);

    QFile csvFile(csvPath);
    QVERIFY(csvFile.open(QIODevice::ReadOnly));
    const QString csv = QString::fromUtf8(csvFile.readAll());
    QVERIFY(csv.contains(QStringLiteral("RecordType")));
    QVERIFY(csv.contains(QStringLiteral("Route")));
    QVERIFY(csv.contains(QStringLiteral("Hyderabad")));
}

void AirbaseScenarioTests::routeIntentGeneratesOneChennaiRoute_data()
{
    QTest::addColumn<QString>("prompt");
    QTest::newRow("exact Bengaluru to Chennai request")
        << QStringLiteral("Create 5 aircraft in Bengaluru and route one aircraft from Bengaluru to Chennai.");
    QTest::newRow("requested wording")
        << QStringLiteral("Create 5 aircraft near Bengaluru and send one toward Chennai.");
    QTest::newRow("numeric and Bangalore alias")
        << QStringLiteral("Generate 5 planes around Bangalore; route one aircraft to Chennai.");
    QTest::newRow("spelled-out count")
        << QStringLiteral("Make five aircraft near Bengaluru with one aircraft flying towards Chennai.");
}

void AirbaseScenarioTests::routeIntentGeneratesOneChennaiRoute()
{
    QFETCH(QString, prompt);
    ScenarioEngine engine;
    QVERIFY2(engine.generateScenario(prompt), qPrintable(engine.statusMessage()));

    QCOMPARE(engine.region(), QStringLiteral("Bengaluru"));
    QCOMPARE(engine.entityCount(), 5);
    QCOMPARE(engine.entityModel()->rowCount(), 5);
    QCOMPARE(engine.statusMessage(),
             QStringLiteral("GENERATED: 5 AIRCRAFT; A01 ROUTED TO CHENNAI"));

    int chennaiRoutes = 0;
    for (int index = 0; index < engine.scenario().entities().size(); ++index)
    {
        const Entity &entity = engine.scenario().entities().at(static_cast<std::size_t>(index));
        const QVector<Waypoint> &route = entity.route();
        QVERIFY(route.size() >= 2);
        if (route.last().name == QStringLiteral("Chennai"))
        {
            ++chennaiRoutes;
            QCOMPARE(route.last().latitude, 13.0827);
            QCOMPARE(route.last().longitude, 80.2707);
            QCOMPARE(route.first().latitude, entity.latitude());
            QCOMPARE(route.first().longitude, entity.longitude());
            const QVariantList mapRoute = engine.entityModel()->data(
                                                                  engine.entityModel()->index(index), EntityModel::RouteRole)
                                              .toList();
            QCOMPARE(mapRoute.size(), route.size());
            QCOMPARE(mapRoute.last().value<QGeoCoordinate>(), QGeoCoordinate(13.0827, 80.2707));
        }
    }
    QCOMPARE(chennaiRoutes, 1);
}

void AirbaseScenarioTests::multiRouteIntentGeneratesOnlyRequestedRoutes()
{
    ScenarioEngine engine;
    QVERIFY2(engine.generateScenario(QStringLiteral(
                 "Create 3 aircraft near Mumbai, send one to Pune, and make another fly toward Goa.")),
             qPrintable(engine.statusMessage()));

    QCOMPARE(engine.region(), QStringLiteral("Mumbai"));
    QCOMPARE(engine.entityCount(), 3);
    QCOMPARE(engine.entityModel()->rowCount(), 3);

    const auto &entities = engine.scenario().entities();
    QCOMPARE(entities.size(), 3);
    QCOMPARE(entities.at(0).route().last().name, QStringLiteral("Pune"));
    QCOMPARE(entities.at(1).route().last().name, QStringLiteral("Goa"));
    QVERIFY(entities.at(2).route().last().name != QStringLiteral("Pune"));
    QVERIFY(entities.at(2).route().last().name != QStringLiteral("Goa"));
    for (const Entity &entity : entities)
    {
        QVERIFY(entity.route().size() >= 2);
        QCOMPARE(entity.route().first().latitude, entity.latitude());
        QCOMPARE(entity.route().first().longitude, entity.longitude());
    }
    QCOMPARE(engine.statusMessage(),
             QStringLiteral("GENERATED: 3 AIRCRAFT; ROUTES: A01 TO PUNE; A02 TO GOA"));
}

void AirbaseScenarioTests::individualMovementIsIsolatedAndUsesVerifiedRoute()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 3 aircraft at Hindon Airbase")));
    const QVariantMap aircraft1Before = engine.aircraftInformation(QStringLiteral("A01"));
    const QVariantMap aircraft2Before = engine.aircraftInformation(QStringLiteral("A02"));
    const QVariantMap aircraft3Before = engine.aircraftInformation(QStringLiteral("A03"));

    QVERIFY2(engine.generateScenario(QStringLiteral(
                 "Move AERO002 from its current position to Pune Airbase.")),
             qPrintable(engine.statusMessage()));
    QCOMPARE(engine.simulationRunning(), false);
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A02"))
                 .value(QStringLiteral("movementStatus")).toString(),
             QStringLiteral("MOVING"));

    const Entity &movingEntity = engine.scenario().entities().at(1);
    const QVector<Waypoint> &route = movingEntity.route();
    QVERIFY(route.size() > 2);
    QCOMPARE(route.first().latitude,
             aircraft2Before.value(QStringLiteral("latitude")).toDouble());
    QCOMPARE(route.first().longitude,
             aircraft2Before.value(QStringLiteral("longitude")).toDouble());
    QCOMPARE(route.last().name, QStringLiteral("Pune Airbase"));
    QCOMPARE(route.last().latitude, 18.582);
    QCOMPARE(route.last().longitude, 73.919);

    QVERIFY(engine.advanceSimulationForTest(600.0));
    const QVariantMap aircraft2After = engine.aircraftInformation(QStringLiteral("A02"));
    QVERIFY(aircraft2After.value(QStringLiteral("latitude")).toDouble() !=
            aircraft2Before.value(QStringLiteral("latitude")).toDouble() ||
            aircraft2After.value(QStringLiteral("longitude")).toDouble() !=
                aircraft2Before.value(QStringLiteral("longitude")).toDouble());
    QVERIFY(aircraft2After.value(QStringLiteral("currentWaypoint")).toString() !=
            QStringLiteral("CURRENT POSITION"));
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A01")), aircraft1Before);
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A03")), aircraft3Before);
    QCOMPARE(engine.simulationTime(), 0.0);
    QVERIFY(engine.generateScenario(QStringLiteral("Stop AERO002")));
}

void AirbaseScenarioTests::individualMovementToSulurFromLandedAircraft()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 3 aircraft at Hindon Airbase")));
    QCOMPARE(engine.selectedAirbaseId(), QStringLiteral("in-hindon-afs"));

    const QVariantMap aircraft1Before = engine.aircraftInformation(QStringLiteral("A01"));
    const QVariantMap aircraft3Before = engine.aircraftInformation(QStringLiteral("A03"));
    const QVariantMap startingAircraft2 = engine.aircraftInformation(QStringLiteral("A02"));
    QVERIFY(engine.generateScenario(QStringLiteral("Move AERO002 to Pune Airbase")));
    QVERIFY(engine.advanceSimulationForTest(100000.0));

    const QVariantMap landedAircraft2 = engine.aircraftInformation(QStringLiteral("A02"));
    QCOMPARE(engine.entityModel()
                 ->data(engine.entityModel()->index(1), EntityModel::LifecycleRole)
                 .toString(),
             QStringLiteral("Landed"));
    QCOMPARE(landedAircraft2.value(QStringLiteral("movementStatus")).toString(),
             QStringLiteral("COMPLETED"));
    const double landedLatitude = landedAircraft2.value(QStringLiteral("latitude")).toDouble();
    const double landedLongitude = landedAircraft2.value(QStringLiteral("longitude")).toDouble();

    QVERIFY(engine.generateScenario(QStringLiteral("Move AERO002 to Sulur Airbase.")));
    const Entity &movingAircraft = engine.scenario().entities().at(1);
    const QVector<Waypoint> &route = movingAircraft.route();
    QVERIFY(route.size() >= 2);
    QCOMPARE(route.first().latitude, landedLatitude);
    QCOMPARE(route.first().longitude, landedLongitude);
    QCOMPARE(route.last().name, QStringLiteral("Sulur Airbase"));
    QCOMPARE(route.last().latitude, 11.005);
    QCOMPARE(route.last().longitude, 77.127);
    QCOMPARE(engine.entityModel()
                 ->data(engine.entityModel()->index(1), EntityModel::LifecycleRole)
                 .toString(),
             QStringLiteral("Airborne"));
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A02"))
                 .value(QStringLiteral("movementStatus")).toString(),
             QStringLiteral("MOVING"));
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A01")), aircraft1Before);
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A03")), aircraft3Before);
    QVERIFY(startingAircraft2.value(QStringLiteral("latitude")).toDouble() != landedLatitude ||
            startingAircraft2.value(QStringLiteral("longitude")).toDouble() != landedLongitude);

    QVERIFY(engine.advanceSimulationForTest(300.0));
    const QVariantMap aircraft2Moving = engine.aircraftInformation(QStringLiteral("A02"));
    QVERIFY(aircraft2Moving.value(QStringLiteral("latitude")).toDouble() != landedLatitude ||
            aircraft2Moving.value(QStringLiteral("longitude")).toDouble() != landedLongitude);
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A01")), aircraft1Before);
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A03")), aircraft3Before);

    QVERIFY(!engine.generateScenario(QStringLiteral("Move AERO002 to Imaginary Airbase")));
    QVERIFY(engine.statusMessage().contains(QStringLiteral("DESTINATION")));
}

void AirbaseScenarioTests::individualMovementPauseResumeStopAndCompletes()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 1 aircraft at Hindon Airbase")));
    QVERIFY(engine.generateScenario(QStringLiteral("Move AERO001 to Pune Airbase")));
    QVERIFY(engine.advanceSimulationForTest(180.0));
    const QVariantMap beforePause = engine.aircraftInformation(QStringLiteral("A01"));

    QVERIFY(engine.selectAircraft(QStringLiteral("A01")));
    QVERIFY(engine.generateScenario(QStringLiteral("Pause AERO001")));
    QCOMPARE(engine.selectedAircraftMovementStatus(), QStringLiteral("PAUSED"));
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A01"))
                 .value(QStringLiteral("movementStatus")).toString(),
             QStringLiteral("PAUSED"));
    QTest::qWait(80);
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A01"))
                 .value(QStringLiteral("latitude")).toDouble(),
             beforePause.value(QStringLiteral("latitude")).toDouble());
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A01"))
                 .value(QStringLiteral("longitude")).toDouble(),
             beforePause.value(QStringLiteral("longitude")).toDouble());

    QVERIFY(engine.generateScenario(QStringLiteral("Resume AERO001")));
    QVERIFY(engine.advanceSimulationForTest(180.0));
    const QVariantMap afterResume = engine.aircraftInformation(QStringLiteral("A01"));
    QVERIFY(afterResume.value(QStringLiteral("latitude")).toDouble() !=
                beforePause.value(QStringLiteral("latitude")).toDouble() ||
            afterResume.value(QStringLiteral("longitude")).toDouble() !=
                beforePause.value(QStringLiteral("longitude")).toDouble());

    QVERIFY(engine.generateScenario(QStringLiteral("Stop AERO001")));
    const QVariantMap stopped = engine.aircraftInformation(QStringLiteral("A01"));
    QCOMPARE(stopped.value(QStringLiteral("movementStatus")).toString(),
             QStringLiteral("STATIONARY"));
    QCOMPARE(stopped.value(QStringLiteral("status")).toString(), QStringLiteral("STOPPED"));
    QCOMPARE(engine.generateScenario(QStringLiteral("Start AERO001")), true);
    QVERIFY(engine.advanceSimulationForTest(100000.0));
    const QVariantMap completed = engine.aircraftInformation(QStringLiteral("A01"));
    QCOMPARE(completed.value(QStringLiteral("movementStatus")).toString(),
             QStringLiteral("COMPLETED"));
    QCOMPARE(completed.value(QStringLiteral("latitude")).toDouble(), 18.582);
    QCOMPARE(completed.value(QStringLiteral("longitude")).toDouble(), 73.919);
    QCOMPARE(engine.simulationRunning(), false);
}

void AirbaseScenarioTests::individualDestinationReplacementAndUnknownRequests()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 3 aircraft at Hindon Airbase")));
    QVERIFY(engine.generateScenario(QStringLiteral("Move AERO002 to Pune Airbase")));
    QVERIFY(engine.advanceSimulationForTest(300.0));

    const QVariantMap beforeReroute = engine.aircraftInformation(QStringLiteral("A02"));
    QSignalSpy routeUpdates(engine.entityModel(), &QAbstractItemModel::dataChanged);
    QVERIFY2(engine.generateScenario(
                 QStringLiteral("Change AERO002's destination to Goa.")),
             qPrintable(engine.statusMessage()));
    const Entity &reroutedEntity = engine.scenario().entities().at(1);
    QCOMPARE(reroutedEntity.route().first().latitude,
             beforeReroute.value(QStringLiteral("latitude")).toDouble());
    QCOMPARE(reroutedEntity.route().first().longitude,
             beforeReroute.value(QStringLiteral("longitude")).toDouble());
    QCOMPARE(reroutedEntity.route().last().name, QStringLiteral("Goa"));
    QCOMPARE(reroutedEntity.route().last().latitude, 15.38);
    QCOMPARE(reroutedEntity.route().last().longitude, 73.811);
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A02"))
                 .value(QStringLiteral("movementStatus")).toString(),
             QStringLiteral("MOVING"));
    QVERIFY(!routeUpdates.isEmpty());
    const int routePointCount = static_cast<int>(reroutedEntity.route().size());
    const QVariantList mapRoute = engine.entityModel()->data(
        engine.entityModel()->index(1), EntityModel::RouteRole).toList();
    QCOMPARE(mapRoute.size(), routePointCount);

    const std::vector<Entity> entitiesBeforeRejections = engine.scenario().entities();
    const QVariantMap stateBeforeRejections = engine.aircraftInformation(QStringLiteral("A02"));
    QVERIFY(!engine.generateScenario(QStringLiteral("Move AERO002 to Atlantis")));
    QVERIFY(engine.statusMessage().contains(QStringLiteral("DESTINATION")));
    QVERIFY(!engine.generateScenario(QStringLiteral("Move AERO999 to Pune Airbase")));
    QVERIFY(engine.statusMessage().contains(QStringLiteral("NOT FOUND")));
    QCOMPARE(engine.scenario().entities().at(1).route().size(),
             entitiesBeforeRejections.at(1).route().size());
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A02")), stateBeforeRejections);
    engine.stopSelectedAircraft();
}

void AirbaseScenarioTests::globalAndIndividualSimulationControlsRemainIndependent()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 3 aircraft at Hindon Airbase")));
    const QVariantMap aircraft1Before = engine.aircraftInformation(QStringLiteral("A01"));
    const QVariantMap aircraft3Before = engine.aircraftInformation(QStringLiteral("A03"));
    QVERIFY(engine.generateScenario(QStringLiteral("Move AERO002 to Pune Airbase")));
    QVERIFY(engine.advanceSimulationForTest(120.0));
    QCOMPARE(engine.simulationRunning(), false);
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A01")), aircraft1Before);
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A03")), aircraft3Before);

    QVERIFY(engine.playSimulation());
    QVERIFY(engine.simulationRunning());
    QVERIFY(engine.advanceSimulationForTest(60.0));
    QVERIFY(engine.simulationTime() > 0.0);
    QVERIFY(engine.aircraftInformation(QStringLiteral("A01"))
                .value(QStringLiteral("latitude")).toDouble() !=
                aircraft1Before.value(QStringLiteral("latitude")).toDouble() ||
            engine.aircraftInformation(QStringLiteral("A01"))
                .value(QStringLiteral("longitude")).toDouble() !=
                aircraft1Before.value(QStringLiteral("longitude")).toDouble());
    engine.pauseSimulation();
    QVERIFY(!engine.simulationRunning());
    QVERIFY(engine.aircraftInformation(QStringLiteral("A02"))
                .value(QStringLiteral("movementStatus")).toString() ==
            QStringLiteral("MOVING"));
    QVERIFY(engine.generateScenario(QStringLiteral("Stop AERO002")));
}

void AirbaseScenarioTests::individualMovementRuntimeStatePersistsPaused()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());
    ScenarioEngine source;
    QVERIFY(source.generateScenario(QStringLiteral("Generate 1 aircraft at Hindon Airbase")));
    QVERIFY(source.generateScenario(QStringLiteral("Move AERO001 to Pune Airbase")));
    QVERIFY(source.advanceSimulationForTest(240.0));
    QVERIFY(source.generateScenario(QStringLiteral("Pause AERO001")));
    const double currentLatitude =
        source.aircraftInformation(QStringLiteral("A01"))
            .value(QStringLiteral("latitude")).toDouble();
    QVERIFY(source.saveScenario(storage.filePath(QStringLiteral("individual-movement.json"))));

    ScenarioEngine restored;
    ScenarioRepository repository(storage.path());
    QVERIFY2(restored.loadScenario(repository.latestScenarioPath()),
             qPrintable(restored.statusMessage()));
    QCOMPARE(restored.aircraftInformation(QStringLiteral("A01"))
                 .value(QStringLiteral("movementStatus")).toString(),
             QStringLiteral("PAUSED"));
    QCOMPARE(restored.aircraftInformation(QStringLiteral("A01"))
                 .value(QStringLiteral("latitude")).toDouble(),
             currentLatitude);
    QVERIFY(!restored.simulationRunning());
    QVERIFY(restored.generateScenario(QStringLiteral("Resume AERO001")));
    QVERIFY(restored.advanceSimulationForTest(120.0));
    QVERIFY(restored.aircraftInformation(QStringLiteral("A01"))
                .value(QStringLiteral("latitude")).toDouble() != currentLatitude ||
            restored.aircraftInformation(QStringLiteral("A01"))
                .value(QStringLiteral("longitude")).toDouble() !=
                source.aircraftInformation(QStringLiteral("A01"))
                    .value(QStringLiteral("longitude")).toDouble());
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
    QMap<QString, QVariantMap> originalRuntimeStates;
    for (const Entity &entity : engine.scenario().entities())
    {
        originalPositions.insert(entity.id(), QGeoCoordinate(entity.latitude(), entity.longitude()));
        originalRuntimeStates.insert(entity.id(), engine.aircraftInformation(entity.id()));
    }

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
        const QVariantMap runtime = engine.aircraftInformation(iterator.key());
        QCOMPARE(runtime.value(QStringLiteral("latitude")),
                 originalRuntimeStates.value(iterator.key()).value(QStringLiteral("latitude")));
        QCOMPARE(runtime.value(QStringLiteral("longitude")),
                 originalRuntimeStates.value(iterator.key()).value(QStringLiteral("longitude")));
    }
    QCOMPARE(engine.entityModel()->rowCount(), 5);
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

void AirbaseScenarioTests::replacementResetsSimulationAndModelState()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 2 aircraft in North India")));
    QVERIFY(engine.playSimulation());
    QVERIFY(engine.advanceSimulationForTest(30.0));
    engine.pauseSimulation();
    QVERIFY(engine.simulationTime() > 0.0);

    QVERIFY(engine.generateScenario(QStringLiteral("REPLACE SCENARIO WITH 2 AIRCRAFT IN SOUTH INDIA")));

    QCOMPARE(engine.region(), QStringLiteral("South India"));
    QCOMPARE(engine.entityCount(), 2);
    QCOMPARE(engine.entityModel()->rowCount(), 2);
    QCOMPARE(engine.simulationTime(), 0.0);
    for (int index = 0; index < engine.entityModel()->rowCount(); ++index)
    {
        const Entity &entity = engine.scenario().entities().at(static_cast<std::size_t>(index));
        const QModelIndex modelIndex = engine.entityModel()->index(index);
        QCOMPARE(engine.entityModel()->data(modelIndex, EntityModel::LatitudeRole).toDouble(),
                 entity.latitude());
        QCOMPARE(engine.entityModel()->data(modelIndex, EntityModel::LongitudeRole).toDouble(),
                 entity.longitude());
        const QVariantMap runtime = engine.aircraftInformation(entity.id());
        QCOMPARE(runtime.value(QStringLiteral("latitude")).toDouble(), entity.latitude());
        QCOMPARE(runtime.value(QStringLiteral("longitude")).toDouble(), entity.longitude());
        QVERIFY(entity.latitude() >= 8.0 && entity.latitude() <= 18.8);
        QVERIFY(entity.longitude() >= 72.0 && entity.longitude() <= 84.0);
    }
}

void AirbaseScenarioTests::generatePromptReplacesExistingScenario()
{
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 3 aircraft in North India")));

    QVERIFY(engine.generateScenario(QStringLiteral("Replace scenario with 2 aircraft in South India")));

    QCOMPARE(engine.region(), QStringLiteral("South India"));
    QCOMPARE(engine.entityCount(), 2);
    QCOMPARE(engine.entityModel()->rowCount(), 2);
    for (const Entity &entity : engine.scenario().entities())
    {
        QVERIFY(entity.latitude() >= 8.0 && entity.latitude() <= 18.8);
        QVERIFY(entity.longitude() >= 72.0 && entity.longitude() <= 84.0);
    }
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

    QVERIFY(AICommand::fromJson(
        QJsonObject{{QStringLiteral("operation"), QStringLiteral("ADD_ENTITIES")},
                    {QStringLiteral("count"), 5},
                    {QStringLiteral("entity_type"), QStringLiteral("aircraft")},
                    {QStringLiteral("region"), QStringLiteral("South India")}},
        request, &error));
    QCOMPARE(request.operation, ScenarioOperation::Add);
    QCOMPARE(request.entityCount, 5);
    QCOMPARE(request.locationName, QStringLiteral("South India"));

    QVERIFY(!AICommand::fromJson(
        QJsonObject{{QStringLiteral("operation"), QStringLiteral("ADD_ENTITIES")},
                    {QStringLiteral("count"), 2},
                    {QStringLiteral("destination"), QStringLiteral("Hindon Airbase")},
                    {QStringLiteral("region"), QStringLiteral(" ")}},
        request, &error));
    QVERIFY(error.contains(QStringLiteral("region")));

    QVERIFY2(AICommand::fromJson(
                 QJsonObject{{QStringLiteral("operation"), QStringLiteral("ADD_ENTITIES")},
                             {QStringLiteral("count"), 3},
                             {QStringLiteral("destination"), QStringLiteral("Mumbai")},
                             {QStringLiteral("route_destinations"),
                              QJsonArray{QStringLiteral("Pune"), QStringLiteral("Goa")}}},
                 request, &error),
             qPrintable(error));
    QCOMPARE(request.routeDestinations,
             QStringList({QStringLiteral("Pune"), QStringLiteral("Goa")}));
}

void AirbaseScenarioTests::individualModelCommandsAreValidated()
{
    const QVector<QPair<QJsonObject, ScenarioOperation>> commands = {
        {QJsonObject{{QStringLiteral("operation"), QStringLiteral("MOVE_ENTITY")},
                     {QStringLiteral("entity_id"), QStringLiteral("AERO002")},
                     {QStringLiteral("destination"), QStringLiteral("Pune Airbase")}},
         ScenarioOperation::Move},
        {QJsonObject{{QStringLiteral("operation"), QStringLiteral("PAUSE_AIRCRAFT")},
                     {QStringLiteral("entity_id"), QStringLiteral("AERO002")}},
         ScenarioOperation::PauseAircraft},
        {QJsonObject{{QStringLiteral("operation"), QStringLiteral("RESUME_AIRCRAFT")},
                     {QStringLiteral("entity_id"), QStringLiteral("AERO002")}},
         ScenarioOperation::ResumeAircraft},
        {QJsonObject{{QStringLiteral("operation"), QStringLiteral("STOP_AIRCRAFT")},
                     {QStringLiteral("entity_id"), QStringLiteral("AERO002")}},
         ScenarioOperation::StopAircraft}};
    for (const auto &command : commands)
    {
        ScenarioRequest request;
        QString error;
        QVERIFY2(AICommand::fromJson(command.first, request, &error), qPrintable(error));
        QCOMPARE(request.operation, command.second);
        QCOMPARE(request.operationTargetId, QStringLiteral("AERO002"));
        QString prompt;
        QVERIFY2(AICommand::toPrompt(request, &prompt, &error), qPrintable(error));
        ScenarioRequest parsedPrompt;
        QVERIFY2(AIParser().parsePrompt(prompt, parsedPrompt, &error), qPrintable(error));
        QCOMPARE(parsedPrompt.operation, request.operation);
        QCOMPARE(parsedPrompt.operationTargetId, request.operationTargetId);
        QCOMPARE(parsedPrompt.operationLocation.toLower(),
                 request.operationLocation.toLower());
    }

    ScenarioRequest invalidRequest;
    QString error;
    QVERIFY(!AICommand::fromJson(
        QJsonObject{{QStringLiteral("operation"), QStringLiteral("MOVE_ENTITY")},
                    {QStringLiteral("entity_id"), QStringLiteral("AERO002")},
                    {QStringLiteral("destination"), QStringLiteral("")}},
        invalidRequest, &error));
    QVERIFY(!error.isEmpty());
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
         QJsonObject{{QStringLiteral("operation"), QStringLiteral("UNKNOWN_OPERATION")}}}};

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
        QStringLiteral("Show airbases in Imaginary Region")};
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
        {QStringLiteral("Move A01 to Delhi"), QStringLiteral("AIRCRAFT A01 MOVING TO Delhi")},
        {QStringLiteral("Change route for A01 to Delhi"), QStringLiteral("AIRCRAFT A01 MOVING TO Delhi")},
        {QStringLiteral("Schedule A01 to take off at 10:30"), QStringLiteral("TAKE-OFF SCHEDULED")},
        {QStringLiteral("Cancel the scheduled take-off for A01"), QStringLiteral("TAKE-OFF CANCELLED FOR A01")},
        {QStringLiteral("Show airbases in North India"), QStringLiteral("AIRBASES SHOWN IN North India")},
        {QStringLiteral("Query scenario"), QStringLiteral("SCENARIO HAS 2 ENTITIES")}};
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
    QCOMPARE(moved.latitude(), original.latitude());
    QCOMPARE(moved.longitude(), original.longitude());
    QCOMPARE(moved.route().first().latitude, originalRuntime.value(QStringLiteral("latitude")).toDouble());
    QCOMPARE(moved.route().first().longitude, originalRuntime.value(QStringLiteral("longitude")).toDouble());
    QCOMPARE(moved.route().last().latitude, 28.6139);
    QCOMPARE(moved.route().last().longitude, 77.2090);

    QVERIFY(engine.generateScenario(QStringLiteral("Change route for A01 to Delhi")));
    QVERIFY(engine.scenario().entities().at(0).route().size() >= 2);

    const Entity beforeInvalid = engine.scenario().entities().at(0);
    const QVariantMap runtimeBeforeInvalid = engine.aircraftInformation(beforeInvalid.id());
    QVERIFY(!engine.generateScenario(QStringLiteral("Move A01 to South India")));
    QVERIFY(engine.statusMessage().contains(QStringLiteral("SPECIFIC VERIFIED AIRBASE OR CITY")));
    const Entity afterInvalidMove = engine.scenario().entities().at(0);
    QCOMPARE(afterInvalidMove.latitude(), beforeInvalid.latitude());
    QCOMPARE(afterInvalidMove.longitude(), beforeInvalid.longitude());
    QCOMPARE(afterInvalidMove.route().size(), beforeInvalid.route().size());
    QCOMPARE(engine.aircraftInformation(afterInvalidMove.id()), runtimeBeforeInvalid);

    QVERIFY(!engine.generateScenario(QStringLiteral("Change route for A01 to South India")));
    QVERIFY(engine.statusMessage().contains(QStringLiteral("SPECIFIC VERIFIED AIRBASE OR CITY")));
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

void AirbaseScenarioTests::ollamaUnavailableSafelyExecutesIndividualMovement()
{
    const bool hadPreviousEndpoint =
        qEnvironmentVariableIsSet("AEROSIM_OLLAMA_ENDPOINT");
    const QByteArray previousEndpoint = qgetenv("AEROSIM_OLLAMA_ENDPOINT");
    qputenv("AEROSIM_OLLAMA_ENDPOINT", QByteArrayLiteral("http://127.0.0.1:1"));
    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 3 aircraft at Hindon Airbase")));
    QVERIFY(engine.generateScenarioWithAI(
        QStringLiteral("Move AERO002 from its current position to Pune Airbase.")));
    QTRY_VERIFY_WITH_TIMEOUT(
        engine.statusMessage().contains(QStringLiteral("LOCAL AI UNAVAILABLE")),
        6000);
    QCOMPARE(engine.aircraftInformation(QStringLiteral("A02"))
                 .value(QStringLiteral("movementStatus")).toString(),
             QStringLiteral("MOVING"));
    const QVector<Waypoint> &route = engine.scenario().entities().at(1).route();
    QCOMPARE(route.last().latitude, 18.582);
    QCOMPARE(route.last().longitude, 73.919);
    if (hadPreviousEndpoint)
        qputenv("AEROSIM_OLLAMA_ENDPOINT", previousEndpoint);
    else
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
                socket->flush(); });
    client.generate(QStringLiteral("query"));
    QTRY_COMPARE_WITH_TIMEOUT(responseSpy.count(), 1, 3000);
    QCOMPARE(responseSpy.at(0).at(0).toString(),
             QStringLiteral("{\"operation\":\"QUERY_SCENARIO\"}"));
}

void AirbaseScenarioTests::ollamaRequestUsesContextLimit_data()
{
    QTest::addColumn<QString>("contextSetting");
    QTest::addColumn<int>("expectedContext");
    QTest::newRow("default") << QString() << 2048;
    QTest::newRow("environment override") << QStringLiteral("3072") << 3072;
}

void AirbaseScenarioTests::ollamaRequestUsesContextLimit()
{
    QFETCH(QString, contextSetting);
    QFETCH(int, expectedContext);

    const bool hadPreviousSetting = qEnvironmentVariableIsSet("AEROSIM_OLLAMA_NUM_CTX");
    const QByteArray previousSetting = qgetenv("AEROSIM_OLLAMA_NUM_CTX");
    if (contextSetting.isEmpty())
        qunsetenv("AEROSIM_OLLAMA_NUM_CTX");
    else
        qputenv("AEROSIM_OLLAMA_NUM_CTX", contextSetting.toUtf8());

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::AnyIPv4));
    QByteArray requestData;
    QJsonObject requestPayload;
    connect(&server, &QTcpServer::newConnection, &server, [&server, &requestData, &requestPayload]()
            {
                QTcpSocket *socket = server.nextPendingConnection();
                connect(socket, &QTcpSocket::readyRead, socket,
                        [socket, &requestData, &requestPayload]()
                        {
                            requestData += socket->readAll();
                            const qsizetype headerEnd = requestData.indexOf("\r\n\r\n");
                            if (headerEnd < 0)
                                return;
                            int contentLength = 0;
                            for (const QByteArray &line : requestData.left(headerEnd).split('\n'))
                                if (line.toLower().startsWith("content-length:"))
                                    contentLength = line.mid(line.indexOf(':') + 1).trimmed().toInt();
                            if (requestData.size() < headerEnd + 4 + contentLength)
                                return;
                            requestPayload = QJsonDocument::fromJson(
                                requestData.mid(headerEnd + 4, contentLength)).object();
                            const QByteArray body = QByteArrayLiteral("{\"response\":\"OK\"}");
                            socket->write(QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ") +
                                          QByteArray::number(body.size()) +
                                          QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + body);
                            socket->flush();
                        }); });

    OllamaClient client;
    if (hadPreviousSetting)
        qputenv("AEROSIM_OLLAMA_NUM_CTX", previousSetting);
    else
        qunsetenv("AEROSIM_OLLAMA_NUM_CTX");
    client.setEndpoint(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())));
    client.setModel(QStringLiteral("qwen2.5-coder:1.5b"));
    QSignalSpy responseSpy(&client, &OllamaClient::responseReady);
    client.generate(QStringLiteral("query"));
    QTRY_COMPARE_WITH_TIMEOUT(responseSpy.count(), 1, 3000);
    const qsizetype headerEnd = requestData.indexOf(QByteArrayLiteral("\r\n\r\n"));
    QVERIFY(headerEnd >= 0);
    QVERIFY(requestData.startsWith(QByteArrayLiteral("POST /api/generate ")));
    QVERIFY(requestData.left(headerEnd).toLower().contains(
        QByteArrayLiteral("content-type: application/json")));
    QCOMPARE(requestPayload.value(QStringLiteral("options")).toObject().value(QStringLiteral("num_ctx")).toInt(),
             expectedContext);
    QCOMPARE(requestPayload.value(QStringLiteral("model")).toString(), client.model());
    QCOMPARE(requestPayload.value(QStringLiteral("prompt")).toString(),
             QStringLiteral("query"));
    QCOMPARE(requestPayload.value(QStringLiteral("stream")).toBool(), false);
    QCOMPARE(client.timeoutMs(), 180000);
    QCOMPARE(requestPayload.value(QStringLiteral("options")).toObject().value(QStringLiteral("num_gpu")).toInt(),
             0);
}

void AirbaseScenarioTests::ollamaCreatePromptPreservesIntent_data()
{
    QTest::addColumn<QString>("modelCommand");
    QTest::newRow("missing location")
        << QStringLiteral("{\"operation\":\"ADD_ENTITIES\",\"count\":2,"
                          "\"entity_type\":\"aircraft\"}");
    QTest::newRow("empty region")
        << QStringLiteral("{\"operation\":\"ADD_ENTITIES\",\"count\":2,"
                          "\"entity_type\":\"aircraft\",\"region\":\" \"}");
    QTest::newRow("incorrect count")
        << QStringLiteral("{\"operation\":\"ADD_ENTITIES\",\"count\":3,"
                          "\"entity_type\":\"aircraft\",\"destination\":\"South India\"}");
    QTest::newRow("incorrect region")
        << QStringLiteral("{\"operation\":\"ADD_ENTITIES\",\"count\":2,"
                          "\"entity_type\":\"aircraft\",\"destination\":\"North India\"}");
}

void AirbaseScenarioTests::ollamaCreatePromptPreservesIntent()
{
    QFETCH(QString, modelCommand);

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::AnyIPv4));
    queueOllamaResponse(server, modelCommand);

    const bool hadPreviousEndpoint = qEnvironmentVariableIsSet("AEROSIM_OLLAMA_ENDPOINT");
    const QByteArray previousEndpoint = qgetenv("AEROSIM_OLLAMA_ENDPOINT");
    qputenv("AEROSIM_OLLAMA_ENDPOINT",
            QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
    ScenarioEngine engine;
    if (hadPreviousEndpoint)
        qputenv("AEROSIM_OLLAMA_ENDPOINT", previousEndpoint);
    else
        qunsetenv("AEROSIM_OLLAMA_ENDPOINT");

    QVERIFY(engine.generateScenario(QStringLiteral("Generate 3 aircraft in North India")));
    QVERIFY(engine.generateScenarioWithAI(QStringLiteral("CREATE 2 AIRCRAFT IN SOUTH INDIA")));
    QTRY_VERIFY_WITH_TIMEOUT(!engine.aiRequestPending(), 6000);

    QVERIFY(engine.statusMessage().startsWith(QStringLiteral("MODEL RESPONSE REJECTED:")));
    QVERIFY(!engine.statusMessage().contains(QStringLiteral("EXECUTED")));
    QCOMPARE(engine.region(), QStringLiteral("North India"));
    QCOMPARE(engine.entityCount(), 3);
    QCOMPARE(engine.entityModel()->rowCount(), 3);
}

void AirbaseScenarioTests::ollamaRejectsModelRouteThatDropsRequestedRoute()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::AnyIPv4));
    queueOllamaResponse(server,
                        QStringLiteral("{\"operation\":\"ADD_ENTITIES\",\"count\":5,"
                                       "\"entity_type\":\"aircraft\","
                                       "\"destination\":\"Bengaluru\"}"));

    const bool hadPreviousEndpoint = qEnvironmentVariableIsSet("AEROSIM_OLLAMA_ENDPOINT");
    const QByteArray previousEndpoint = qgetenv("AEROSIM_OLLAMA_ENDPOINT");
    qputenv("AEROSIM_OLLAMA_ENDPOINT",
            QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
    ScenarioEngine engine;
    if (hadPreviousEndpoint)
        qputenv("AEROSIM_OLLAMA_ENDPOINT", previousEndpoint);
    else
        qunsetenv("AEROSIM_OLLAMA_ENDPOINT");

    QVERIFY(engine.generateScenarioWithAI(
        QStringLiteral("Create 5 aircraft near Bengaluru and send one toward Chennai.")));
    QTRY_VERIFY_WITH_TIMEOUT(!engine.aiRequestPending(), 6000);

    QVERIFY(engine.statusMessage().startsWith(QStringLiteral("MODEL RESPONSE REJECTED:")));
    QVERIFY(!engine.statusMessage().contains(QStringLiteral("EXECUTED")));
    QCOMPARE(engine.entityCount(), 0);
}

void AirbaseScenarioTests::ollamaRejectedModelCommandDoesNotMutateScenario()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::AnyIPv4));
    queueOllamaResponse(server,
                        QStringLiteral("{\"operation\":\"ADD_ENTITIES\",\"count\":2,"
                                       "\"entity_type\":\"aircraft\"}"));

    const bool hadPreviousEndpoint = qEnvironmentVariableIsSet("AEROSIM_OLLAMA_ENDPOINT");
    const QByteArray previousEndpoint = qgetenv("AEROSIM_OLLAMA_ENDPOINT");
    qputenv("AEROSIM_OLLAMA_ENDPOINT",
            QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
    ScenarioEngine engine;
    if (hadPreviousEndpoint)
        qputenv("AEROSIM_OLLAMA_ENDPOINT", previousEndpoint);
    else
        qunsetenv("AEROSIM_OLLAMA_ENDPOINT");

    QVERIFY(engine.generateScenario(QStringLiteral("Generate 3 aircraft in North India")));
    QVERIFY(engine.generateScenarioWithAI(QStringLiteral("CREATE 2 AIRCRAFT NEAR THE MOON")));
    QTRY_VERIFY_WITH_TIMEOUT(!engine.aiRequestPending(), 6000);

    QVERIFY(engine.statusMessage().contains(QStringLiteral("MODEL RESPONSE REJECTED")));
    QVERIFY(!engine.statusMessage().contains(QStringLiteral("DETERMINISTIC PARSER EXECUTED")));
    QVERIFY(!engine.statusMessage().contains(QStringLiteral("GENERATED")));
    QCOMPARE(engine.region(), QStringLiteral("North India"));
    QCOMPARE(engine.entityCount(), 3);
    QCOMPARE(engine.entityModel()->rowCount(), 3);
}

void AirbaseScenarioTests::ollamaMovementCommandExecutesAuthoritatively()
{
    const QString prompt =
        QStringLiteral("Move AERO004 from it's location to Yelahanka Airbase");
    const QString modelCommand =
        QStringLiteral("{\"operation\":\"MOVE_ENTITY\",\"entity_id\":\"AERO004\","
                       "\"destination\":\"Yelahanka Airbase\"}");
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::AnyIPv4));
    const QByteArray responseBody = QJsonDocument(
        QJsonObject{{QStringLiteral("response"), modelCommand}})
                                        .toJson(QJsonDocument::Compact);
    QByteArray requestBytes;
    bool responseSent = false;
    connect(&server, &QTcpServer::newConnection, &server, [&]()
    {
        QTcpSocket *socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, &server, [&, socket]()
        {
            requestBytes += socket->readAll();
            const qsizetype headerEnd = requestBytes.indexOf(QByteArrayLiteral("\r\n\r\n"));
            if (responseSent || headerEnd < 0)
                return;
            const QByteArray headers = requestBytes.left(headerEnd);
            const QRegularExpression lengthPattern(
                QStringLiteral("(?:^|\\r\\n)Content-Length:\\s*(\\d+)"),
                QRegularExpression::CaseInsensitiveOption);
            const QRegularExpressionMatch lengthMatch =
                lengthPattern.match(QString::fromLatin1(headers));
            if (!lengthMatch.hasMatch() ||
                requestBytes.size() - static_cast<int>(headerEnd) - 4 <
                    lengthMatch.captured(1).toInt())
                return;
            responseSent = true;
            socket->write(QByteArrayLiteral(
                              "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ") +
                          QByteArray::number(responseBody.size()) +
                          QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + responseBody);
            socket->flush();
        });
    });

    const bool hadPreviousEndpoint = qEnvironmentVariableIsSet("AEROSIM_OLLAMA_ENDPOINT");
    const QByteArray previousEndpoint = qgetenv("AEROSIM_OLLAMA_ENDPOINT");
    qputenv("AEROSIM_OLLAMA_ENDPOINT",
            QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
    ScenarioEngine engine;
    if (hadPreviousEndpoint)
        qputenv("AEROSIM_OLLAMA_ENDPOINT", previousEndpoint);
    else
        qunsetenv("AEROSIM_OLLAMA_ENDPOINT");

    QVERIFY(engine.generateScenario(QStringLiteral("Generate 4 aircraft at Hindon Airbase")));
    QVERIFY(engine.generateScenarioWithAI(prompt));
    QTRY_VERIFY_WITH_TIMEOUT(!engine.aiRequestPending(), 6000);

    QVERIFY(responseSent);
    QVERIFY(requestBytes.startsWith(QByteArrayLiteral("POST /api/generate ")));
    const QString configuredModel =
        qEnvironmentVariable("AEROSIM_OLLAMA_MODEL", QStringLiteral("qwen2.5-coder:7b"));
    const qsizetype headerEnd = requestBytes.indexOf(QByteArrayLiteral("\r\n\r\n"));
    QVERIFY(headerEnd >= 0);
    const QJsonDocument requestDocument =
        QJsonDocument::fromJson(requestBytes.mid(headerEnd + 4));
    QVERIFY(requestDocument.isObject());
    QCOMPARE(requestDocument.object().value(QStringLiteral("model")).toString(),
             configuredModel);
    QVERIFY(requestDocument.object().value(QStringLiteral("prompt")).toString()
                .contains(prompt));
    QVERIFY(engine.statusMessage().startsWith(
        QStringLiteral("OLLAMA GENERATED JSON; COMMAND EXECUTED: AIRCRAFT A04 MOVING TO")));
    const Entity &aircraft = engine.scenario().entities().at(3);
    QCOMPARE(aircraft.route().first().latitude, aircraft.latitude());
    QCOMPARE(aircraft.route().first().longitude, aircraft.longitude());
    QCOMPARE(aircraft.route().last().latitude, 13.135);
    QCOMPARE(aircraft.route().last().longitude, 77.606);
    QCOMPARE(engine.entityCount(), 4);
}

void AirbaseScenarioTests::ollamaRejectedMovementDoesNotExecuteFallback()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::AnyIPv4));
    queueOllamaResponse(
        server,
        QStringLiteral("{\"operation\":\"MOVE_ENTITY\",\"entity_id\":\"AERO001\","
                       "\"destination\":\"Goa\"}"));

    const bool hadPreviousEndpoint = qEnvironmentVariableIsSet("AEROSIM_OLLAMA_ENDPOINT");
    const QByteArray previousEndpoint = qgetenv("AEROSIM_OLLAMA_ENDPOINT");
    qputenv("AEROSIM_OLLAMA_ENDPOINT",
            QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
    ScenarioEngine engine;
    if (hadPreviousEndpoint)
        qputenv("AEROSIM_OLLAMA_ENDPOINT", previousEndpoint);
    else
        qunsetenv("AEROSIM_OLLAMA_ENDPOINT");

    QVERIFY(engine.generateScenario(QStringLiteral("Generate 1 aircraft at Hindon Airbase")));
    const QVector<Waypoint> originalRoute = engine.scenario().entities().at(0).route();
    QVERIFY(engine.generateScenarioWithAI(
        QStringLiteral("Move AERO001 to Pune Airbase")));
    QTRY_VERIFY_WITH_TIMEOUT(!engine.aiRequestPending(), 6000);

    QVERIFY(engine.statusMessage().contains(QStringLiteral("MODEL RESPONSE REJECTED")));
    QVERIFY(engine.statusMessage().contains(
        QStringLiteral("DOES NOT MATCH THE REQUESTED AIRCRAFT ACTION")));
    QVERIFY(!engine.statusMessage().contains(QStringLiteral("EXECUTED")));
    QCOMPARE(engine.entityCount(), 1);
    QCOMPARE(engine.scenario().entities().at(0).route().size(), originalRoute.size());
    QCOMPARE(engine.scenario().entities().at(0).route().last().name,
             originalRoute.last().name);
}

void AirbaseScenarioTests::ollamaLiveMovementCommands()
{
    if (qEnvironmentVariableIsEmpty("AEROSIM_TEST_LIVE_OLLAMA"))
        QSKIP("Set AEROSIM_TEST_LIVE_OLLAMA=1 to run against the configured local Ollama.");

    ScenarioEngine engine;
    QVERIFY(engine.generateScenario(QStringLiteral("Generate 4 aircraft at Hindon Airbase")));
    const QStringList prompts = {
        QStringLiteral("Move AERO004 from it's location to Yelahanka Airbase"),
        QStringLiteral("Move AERO002 to Nal/Bikaner Airbase"),
        QStringLiteral("Move AERO001 to Pune Airbase"),
        QStringLiteral("Move AERO003 towards Chennai")};
    const QVector<QString> ids = {QStringLiteral("A04"), QStringLiteral("A02"),
                                  QStringLiteral("A01"), QStringLiteral("A03")};
    const QVector<QPair<double, double>> destinations = {
        {13.135, 77.606}, {28.0706, 73.2072}, {18.582, 73.919},
        {13.0827, 80.2707}};

    for (int index = 0; index < prompts.size(); ++index)
    {
        const QVariantMap before = engine.aircraftInformation(ids.at(index));
        QVERIFY(engine.generateScenarioWithAI(prompts.at(index)));
        QTRY_VERIFY_WITH_TIMEOUT(!engine.aiRequestPending(), 65000);
        QVERIFY2(engine.statusMessage().startsWith(
                     QStringLiteral("OLLAMA GENERATED JSON; COMMAND EXECUTED: AIRCRAFT ")),
                 qPrintable(engine.statusMessage()));
        const int row = ids.at(index).right(2).toInt() - 1;
        const Entity &entity = engine.scenario().entities().at(
            static_cast<std::size_t>(row));
        QCOMPARE(entity.route().first().latitude,
                 before.value(QStringLiteral("latitude")).toDouble());
        QCOMPARE(entity.route().first().longitude,
                 before.value(QStringLiteral("longitude")).toDouble());
        QCOMPARE(entity.route().last().latitude, destinations.at(index).first);
        QCOMPARE(entity.route().last().longitude, destinations.at(index).second);
    }
}

void AirbaseScenarioTests::ollamaLiveWorkflowCommands()
{
    if (qEnvironmentVariableIsEmpty("AEROSIM_TEST_LIVE_OLLAMA"))
        QSKIP("Set AEROSIM_TEST_LIVE_OLLAMA=1 to run against the configured local Ollama.");

    ScopedEnvironmentVariable endpointSetting(
        "AEROSIM_OLLAMA_ENDPOINT", QByteArrayLiteral("http://127.0.0.1:11434"));
    ScopedEnvironmentVariable modelSetting(
        "AEROSIM_OLLAMA_MODEL", QByteArrayLiteral("qwen2.5-coder:1.5b"));
    ScopedEnvironmentVariable fallbackSetting(
        "AEROSIM_DISABLE_DETERMINISTIC_FALLBACK", QByteArrayLiteral("1"));
    ScenarioEngine engine;

    const QStringList commands = {
        QStringLiteral("Create 5 aircraft at Bangalore"),
        QStringLiteral("Add 3 more aircraft at Delhi"),
        QStringLiteral("Create 7 aircraft in a V formation"),
        QStringLiteral("Move the formation towards Chennai")};
    const QVector<int> expectedCounts = {5, 8, 15, 15};

    for (int index = 0; index < commands.size(); ++index)
    {
        QVERIFY(engine.generateScenarioWithAI(commands.at(index)));
        QTRY_VERIFY_WITH_TIMEOUT(!engine.aiRequestPending(), 90000);
        QVERIFY2(engine.statusMessage().startsWith(
                     QStringLiteral("OLLAMA GENERATED JSON; COMMAND EXECUTED:")),
                 qPrintable(engine.statusMessage()));
        QCOMPARE(engine.entityCount(), expectedCounts.at(index));
    }

    int formationMembers = 0;
    QString formationLeaderId;
    for (const Entity &entity : engine.scenario().entities())
    {
        if (!entity.isFormationMember())
            continue;
        ++formationMembers;
        if (entity.id() == entity.formationLeaderId())
        {
            formationLeaderId = entity.id();
            QCOMPARE(entity.formationType(), QStringLiteral("V"));
        }
    }
    QCOMPARE(formationMembers, 7);
    QVERIFY(!formationLeaderId.isEmpty());
    const Entity *leader = nullptr;
    for (const Entity &entity : engine.scenario().entities())
        if (entity.id() == formationLeaderId)
            leader = &entity;
    QVERIFY(leader);
    QCOMPARE(leader->route().last().latitude, 13.0827);
    QCOMPARE(leader->route().last().longitude, 80.2707);
}

void AirbaseScenarioTests::ollamaLiveBengaluruRoute()
{
    if (qEnvironmentVariableIsEmpty("AEROSIM_TEST_LIVE_OLLAMA"))
        QSKIP("Set AEROSIM_TEST_LIVE_OLLAMA=1 to run against the configured local Ollama.");

    ScopedEnvironmentVariable endpointSetting(
        "AEROSIM_OLLAMA_ENDPOINT", QByteArrayLiteral("http://127.0.0.1:11434"));
    ScopedEnvironmentVariable modelSetting(
        "AEROSIM_OLLAMA_MODEL", QByteArrayLiteral("qwen2.5-coder:1.5b"));
    ScopedEnvironmentVariable fallbackSetting(
        "AEROSIM_DISABLE_DETERMINISTIC_FALLBACK", QByteArrayLiteral("1"));
    ScenarioEngine engine;

    QVERIFY(engine.generateScenarioWithAI(
        QStringLiteral("Create 5 aircraft in Bengaluru and route one aircraft from Bengaluru to Chennai.")));
    QTRY_VERIFY_WITH_TIMEOUT(!engine.aiRequestPending(), 90000);
    QVERIFY2(engine.statusMessage().startsWith(
                 QStringLiteral("OLLAMA GENERATED JSON; COMMAND EXECUTED:")),
             qPrintable(engine.statusMessage()));
    QCOMPARE(engine.entityCount(), 5);
    QCOMPARE(engine.entityModel()->rowCount(), 5);
    QCOMPARE(engine.region(), QStringLiteral("Bengaluru"));

    int routedAircraftCount = 0;
    for (const Entity &entity : engine.scenario().entities())
    {
        const bool routeToChennai = !entity.route().isEmpty() &&
                                    entity.route().last().name == QStringLiteral("Chennai");
        if (routeToChennai)
            ++routedAircraftCount;
    }
    QCOMPARE(routedAircraftCount, 1);
}

void AirbaseScenarioTests::ollamaLiveAircraftManagementCommands()
{
    if (qEnvironmentVariableIsEmpty("AEROSIM_TEST_LIVE_OLLAMA"))
        QSKIP("Set AEROSIM_TEST_LIVE_OLLAMA=1 to run against the configured local Ollama.");

    ScopedEnvironmentVariable endpointSetting(
        "AEROSIM_OLLAMA_ENDPOINT", QByteArrayLiteral("http://127.0.0.1:11434"));
    ScopedEnvironmentVariable modelSetting(
        "AEROSIM_OLLAMA_MODEL", QByteArrayLiteral("qwen2.5-coder:1.5b"));
    ScopedEnvironmentVariable fallbackSetting(
        "AEROSIM_DISABLE_DETERMINISTIC_FALLBACK", QByteArrayLiteral("1"));
    ScenarioEngine engine;

    QVERIFY(engine.generateScenario(
        QStringLiteral("Create 3 aircraft at Hindon Airbase")));
    QVERIFY2(executeLiveOllamaCommand(
                 engine, QStringLiteral("Remove all aircraft.")),
             qPrintable(engine.statusMessage()));
    QCOMPARE(engine.entityCount(), 0);
    QCOMPARE(engine.entityModel()->rowCount(), 0);
    QVERIFY(!engine.scenarioGenerated());

    QVERIFY(engine.generateScenario(
        QStringLiteral("Create 3 aircraft at Hindon Airbase")));
    QVERIFY2(executeLiveOllamaCommand(
                 engine, QStringLiteral("Remove aircraft AERO001.")),
             qPrintable(engine.statusMessage()));
    QCOMPARE(engine.entityCount(), 2);
    QVERIFY(engine.aircraftInformation(QStringLiteral("A01")).isEmpty());

    QVERIFY2(executeLiveOllamaCommand(
                 engine, QStringLiteral("Stop all aircraft.")),
             qPrintable(engine.statusMessage()));
    for (const Entity &entity : engine.scenario().entities())
        QCOMPARE(engine.aircraftInformation(entity.id())
                     .value(QStringLiteral("movementStatus")).toString(),
                 QStringLiteral("STATIONARY"));

    QVERIFY2(executeLiveOllamaCommand(
                 engine, QStringLiteral("Create 7 aircraft in a V formation.")),
             qPrintable(engine.statusMessage()));
    QCOMPARE(engine.entityCount(), 9);
    QString formationId;
    QString leaderId;
    for (const Entity &entity : engine.scenario().entities())
        if (entity.isFormationMember())
        {
            formationId = entity.formationId();
            if (entity.id() == entity.formationLeaderId())
                leaderId = entity.id();
        }
    QVERIFY(!formationId.isEmpty());
    QVERIFY(!leaderId.isEmpty());

    QVERIFY2(executeLiveOllamaCommand(
                 engine, QStringLiteral("Move the V formation towards Chennai.")),
             qPrintable(engine.statusMessage()));
    const Entity *leader = nullptr;
    for (const Entity &entity : engine.scenario().entities())
        if (entity.id() == leaderId)
            leader = &entity;
    QVERIFY(leader);
    QCOMPARE(leader->route().last().latitude, 13.0827);
    QCOMPARE(leader->route().last().longitude, 80.2707);

    QVERIFY2(executeLiveOllamaCommand(
                 engine, QStringLiteral("Stop the V formation.")),
             qPrintable(engine.statusMessage()));
    QCOMPARE(engine.aircraftInformation(leaderId)
                 .value(QStringLiteral("movementStatus")).toString(),
             QStringLiteral("STATIONARY"));
    QVERIFY2(executeLiveOllamaCommand(
                 engine, QStringLiteral("Resume the formation.")),
             qPrintable(engine.statusMessage()));
    QCOMPARE(engine.aircraftInformation(leaderId)
                 .value(QStringLiteral("movementStatus")).toString(),
             QStringLiteral("MOVING"));
    QVERIFY2(executeLiveOllamaCommand(
                 engine, QStringLiteral("Disband the formation.")),
             qPrintable(engine.statusMessage()));
    QCOMPARE(engine.entityCount(), 9);
    for (const Entity &entity : engine.scenario().entities())
        QVERIFY(!entity.isFormationMember());
}

void AirbaseScenarioTests::ollamaHttpFailureReportsReasonAndPreservesScenario()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::AnyIPv4));
    connect(&server, &QTcpServer::newConnection, &server, [&server]()
            {
                QTcpSocket *socket = server.nextPendingConnection();
                const QByteArray body = QByteArrayLiteral("{\"error\":\"CUDA out of memory\"}");
                socket->write(QByteArrayLiteral(
                                  "HTTP/1.1 500 Internal Server Error\r\nContent-Type: application/json\r\nContent-Length: ") +
                              QByteArray::number(body.size()) +
                              QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + body);
                socket->flush(); });

    const bool hadPreviousEndpoint = qEnvironmentVariableIsSet("AEROSIM_OLLAMA_ENDPOINT");
    const QByteArray previousEndpoint = qgetenv("AEROSIM_OLLAMA_ENDPOINT");
    qputenv("AEROSIM_OLLAMA_ENDPOINT",
            QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
    ScenarioEngine engine;
    if (hadPreviousEndpoint)
        qputenv("AEROSIM_OLLAMA_ENDPOINT", previousEndpoint);
    else
        qunsetenv("AEROSIM_OLLAMA_ENDPOINT");

    QVERIFY(engine.generateScenario(QStringLiteral("Generate 3 aircraft in North India")));
    QVERIFY(engine.generateScenarioWithAI(QStringLiteral("CREATE 2 AIRCRAFT IN SOUTH INDIA")));
    QTRY_VERIFY_WITH_TIMEOUT(!engine.aiRequestPending(), 3000);

    QVERIFY(engine.statusMessage().contains(
        QStringLiteral("OLLAMA HTTP ERROR 500: CUDA out of memory")));
    QVERIFY(engine.statusMessage().contains(
        QStringLiteral("DETERMINISTIC FALLBACK NOT USED")));
    QVERIFY(!engine.statusMessage().contains(QStringLiteral("GENERATED")));
    QCOMPARE(engine.region(), QStringLiteral("North India"));
    QCOMPARE(engine.entityCount(), 3);
    QCOMPARE(engine.entityModel()->rowCount(), 3);
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
                socket->flush(); });

    qputenv("AEROSIM_OLLAMA_ENDPOINT",
            QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
    ScenarioEngine engine;
    QCOMPARE(engine.entityCount(), 0);
    QVERIFY(engine.generateScenarioWithAI(QStringLiteral("Add 2 aircraft at Hindon Airbase")));
    QTRY_COMPARE_WITH_TIMEOUT(engine.entityCount(), 2, 3000);
    QCOMPARE(engine.statusMessage(),
             QStringLiteral("OLLAMA GENERATED JSON; COMMAND EXECUTED: GENERATED"));
    const QVector<CommandHistoryEntry> history = engine.commandHistory();
    QCOMPARE(history.size(), 1);
    QCOMPARE(history.first().sequenceNumber, 1);
    QCOMPARE(history.first().prompt,
             QStringLiteral("Add 2 aircraft at Hindon Airbase"));
    QCOMPARE(history.first().parsedOperation, QStringLiteral("ADD_ENTITIES"));
    QCOMPARE(history.first().processingStatus, QStringLiteral("succeeded"));
    qunsetenv("AEROSIM_OLLAMA_ENDPOINT");
}

void AirbaseScenarioTests::ollamaRegionAddCommandExecutesScenarioMutation()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::AnyIPv4));
    queueOllamaResponse(
        server,
        QStringLiteral("{\"operation\":\"ADD_ENTITIES\",\"count\":5,"
                       "\"entity_type\":\"aircraft\",\"region\":\"South India\"}"));

    const bool hadPreviousEndpoint =
        qEnvironmentVariableIsSet("AEROSIM_OLLAMA_ENDPOINT");
    const QByteArray previousEndpoint = qgetenv("AEROSIM_OLLAMA_ENDPOINT");
    qputenv("AEROSIM_OLLAMA_ENDPOINT",
            QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
    ScenarioEngine engine;
    if (hadPreviousEndpoint)
        qputenv("AEROSIM_OLLAMA_ENDPOINT", previousEndpoint);
    else
        qunsetenv("AEROSIM_OLLAMA_ENDPOINT");

    QVERIFY(engine.generateScenario(
        QStringLiteral("Create 2 aircraft in North India.")));
    QVERIFY(engine.generateScenarioWithAI(
        QStringLiteral("Create 5 aircraft in South India.")));
    QTRY_VERIFY_WITH_TIMEOUT(!engine.aiRequestPending(), 6000);
    QCOMPARE(engine.statusMessage(),
             QStringLiteral("OLLAMA GENERATED JSON; COMMAND EXECUTED: GENERATED"));
    QCOMPARE(engine.region(), QStringLiteral("South India"));
    QCOMPARE(engine.entityCount(), 7);
    QCOMPARE(engine.entityModel()->rowCount(), 7);
}

void AirbaseScenarioTests::ollamaAddPreservesExistingEntities()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::AnyIPv4));
    queueOllamaResponse(
        server,
        QStringLiteral("{\"operation\":\"ADD_ENTITIES\",\"count\":3,"
                       "\"entity_type\":\"aircraft\",\"region\":\"Delhi\"}"));
    ScopedEnvironmentVariable endpointSetting(
        "AEROSIM_OLLAMA_ENDPOINT",
        QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
    ScenarioEngine engine;

    QVERIFY(engine.generateScenario(
        QStringLiteral("Create 2 aircraft at Hindon Airbase")));
    const QString firstId = engine.scenario().entities().at(0).id();
    const QString secondId = engine.scenario().entities().at(1).id();
    QVERIFY(engine.generateScenarioWithAI(
        QStringLiteral("Add 3 more aircraft at Delhi")));
    QTRY_VERIFY_WITH_TIMEOUT(!engine.aiRequestPending(), 6000);

    QVERIFY(engine.statusMessage().startsWith(
        QStringLiteral("OLLAMA GENERATED JSON; COMMAND EXECUTED:")));
    QCOMPARE(engine.entityCount(), 5);
    QCOMPARE(engine.scenario().entities().at(0).id(), firstId);
    QCOMPARE(engine.scenario().entities().at(1).id(), secondId);
    QCOMPARE(engine.scenario().entities().at(4).id(), QStringLiteral("A05"));
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
                socket->flush(); });
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
                socket->flush(); });
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

void AirbaseScenarioTests::ollamaFallbackCanBeDisabled()
{
    ScopedEnvironmentVariable endpointSetting(
        "AEROSIM_OLLAMA_ENDPOINT", QByteArrayLiteral("http://127.0.0.1:1"));
    ScopedEnvironmentVariable fallbackSetting(
        "AEROSIM_DISABLE_DETERMINISTIC_FALLBACK", QByteArrayLiteral("1"));
    ScenarioEngine engine;

    const bool accepted = engine.generateScenarioWithAI(
        QStringLiteral("Add 2 aircraft at Hindon Airbase"));
    if (accepted)
        QTRY_VERIFY_WITH_TIMEOUT(!engine.aiRequestPending(), 6000);
    QVERIFY(accepted);
    QVERIFY(engine.statusMessage().contains(
        QStringLiteral("DETERMINISTIC FALLBACK DISABLED")));
    QCOMPARE(engine.entityCount(), 0);
}

void AirbaseScenarioTests::ollamaTimeoutCleansUpReply()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::AnyIPv4));
    connect(&server, &QTcpServer::newConnection, &server, [&server]()
            {
                QTcpSocket *socket = server.nextPendingConnection();
                socket->setParent(&server); });

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

void AirbaseScenarioTests::geoJsonExportsValidRouteAndMetadata()
{
    Scenario scenario;
    scenario.setName(QStringLiteral("GeoJSON test mission"));
    scenario.setDescription(QStringLiteral("Exporter regression test"));
    scenario.setRegion(QStringLiteral("North India"));

    QVector<Waypoint> route = {
        {QStringLiteral("Origin"), 28.0, 77.0, 1000.0},
        {QStringLiteral("Destination"), 29.0, 78.0, 2000.0}};
    Entity aircraft(QStringLiteral("A01"), QStringLiteral("Test aircraft"),
                    QStringLiteral("aircraft"), 28.0, 77.0, 1000.0,
                    250.0, 90.0, QStringLiteral("ACTIVE"),
                    QStringLiteral("TEST01"), 0.0, QStringLiteral("test"),
                    route, false);
    scenario.addEntity(aircraft);

    QString error;
    const QByteArray json = GeoJsonExporter::toJson(scenario, &error);
    QVERIFY2(!json.isEmpty(), qPrintable(error));
    QVERIFY(error.isEmpty());

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    QVERIFY(document.isObject());

    const QJsonObject root = document.object();
    QCOMPARE(root.value(QStringLiteral("type")).toString(),
             QStringLiteral("FeatureCollection"));
    QCOMPARE(root.value(QStringLiteral("name")).toString(),
             QStringLiteral("GeoJSON test mission"));
    QCOMPARE(root.value(QStringLiteral("description")).toString(),
             QStringLiteral("Exporter regression test"));
    QCOMPARE(root.value(QStringLiteral("region")).toString(),
             QStringLiteral("North India"));
    const QJsonObject exportProperties =
        root.value(QStringLiteral("properties")).toObject();
    QCOMPARE(exportProperties.value(QStringLiteral("commandCount")).toInt(), 0);
    QVERIFY(exportProperties.value(QStringLiteral("exportTimestamp")).isString());
    QVERIFY(exportProperties.value(QStringLiteral("commandHistory")).toArray().isEmpty());

    const QJsonArray features = root.value(QStringLiteral("features")).toArray();
    QCOMPARE(features.size(), 2);
    const QJsonObject pointFeature = features.at(0).toObject();
    QCOMPARE(pointFeature.value(QStringLiteral("geometry")).toObject()
                 .value(QStringLiteral("type")).toString(),
             QStringLiteral("Point"));
    const QJsonObject feature = features.at(1).toObject();
    QCOMPARE(feature.value(QStringLiteral("type")).toString(), QStringLiteral("Feature"));

    const QJsonObject geometry = feature.value(QStringLiteral("geometry")).toObject();
    QCOMPARE(geometry.value(QStringLiteral("type")).toString(), QStringLiteral("LineString"));
    const QJsonArray coordinates = geometry.value(QStringLiteral("coordinates")).toArray();
    QCOMPARE(coordinates.size(), 2);

    const QJsonArray first = coordinates.at(0).toArray();
    QCOMPARE(first.size(), 3);
    QCOMPARE(first.at(0).toDouble(), 77.0);   // longitude first
    QCOMPARE(first.at(1).toDouble(), 28.0);   // latitude second
    QCOMPARE(first.at(2).toDouble(), 1000.0); // altitude third

    const QJsonArray second = coordinates.at(1).toArray();
    QCOMPARE(second.at(0).toDouble(), 78.0);
    QCOMPARE(second.at(1).toDouble(), 29.0);
    QCOMPARE(second.at(2).toDouble(), 2000.0);

    const QJsonObject properties = feature.value(QStringLiteral("properties")).toObject();
    QCOMPARE(properties.value(QStringLiteral("entityId")).toString(), QStringLiteral("A01"));
    QCOMPARE(properties.value(QStringLiteral("name")).toString(), QStringLiteral("Test aircraft"));
    QCOMPARE(properties.value(QStringLiteral("type")).toString(), QStringLiteral("aircraft"));
    QCOMPARE(properties.value(QStringLiteral("callsign")).toString(), QStringLiteral("TEST01"));

    const QJsonArray waypoints = properties.value(QStringLiteral("waypoints")).toArray();
    QCOMPARE(waypoints.size(), 2);
    QCOMPARE(waypoints.at(1).toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("Destination"));
}

void AirbaseScenarioTests::geoJsonExportsFiveAircraftPositions()
{
    Scenario scenario;
    scenario.setName(QStringLiteral("Five aircraft"));
    for (int index = 0; index < 5; ++index) {
        scenario.addEntity(Entity(
            QStringLiteral("A%1").arg(index + 1),
            QStringLiteral("Aircraft %1").arg(index + 1),
            QStringLiteral("aircraft"),
            12.0 + index * 0.1, 77.0 + index * 0.1, 8000.0,
            250.0, 90.0, QStringLiteral("ACTIVE")));
    }

    QString error;
    int featureCount = 0;
    const QByteArray json = GeoJsonExporter::toJson(
        scenario, QVector<AirbaseRecord>{}, &error, &featureCount);
    QVERIFY2(!json.isEmpty(), qPrintable(error));
    QVERIFY(error.isEmpty());
    QCOMPARE(featureCount, 5);

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    const QJsonArray features =
        document.object().value(QStringLiteral("features")).toArray();
    QCOMPARE(features.size(), 5);
    const QJsonObject first = features.first().toObject();
    QCOMPARE(first.value(QStringLiteral("geometry")).toObject()
                 .value(QStringLiteral("type")).toString(),
             QStringLiteral("Point"));
    const QJsonArray coordinates = first.value(QStringLiteral("geometry")).toObject()
                                       .value(QStringLiteral("coordinates")).toArray();
    QCOMPARE(coordinates.at(0).toDouble(), 77.0);
    QCOMPARE(coordinates.at(1).toDouble(), 12.0);
    QCOMPARE(coordinates.at(2).toDouble(), 8000.0);
}

void AirbaseScenarioTests::geoJsonExportsAirbaseFormationAndRoute()
{
    Scenario scenario;
    scenario.setName(QStringLiteral("Combined export"));
    Entity aircraft(
        QStringLiteral("A01"), QStringLiteral("Flight One"),
        QStringLiteral("aircraft"), 28.0, 77.0, 9000.0,
        250.0, 90.0, QStringLiteral("ACTIVE"), QStringLiteral("TEST01"),
        0.0, QStringLiteral("test"),
        QVector<Waypoint>{
            {QStringLiteral("Origin"), 28.0, 77.0, 9000.0},
            {QStringLiteral("Destination"), 29.0, 78.0, 9000.0}},
        false);
    aircraft.setFormation(QStringLiteral("F01"), QStringLiteral("V"),
                          QStringLiteral("A01"), 0, {}, 2.0, 90.0,
                          QStringLiteral("ACTIVE"));
    scenario.addEntity(aircraft);
    Entity follower(
        QStringLiteral("A02"), QStringLiteral("Flight Two"),
        QStringLiteral("aircraft"), 28.1, 77.1, 9000.0,
        250.0, 90.0, QStringLiteral("ACTIVE"), QStringLiteral("TEST02"),
        0.0, QStringLiteral("test"),
        QVector<Waypoint>{{QStringLiteral("Formation slot"), 28.1, 77.1, 9000.0}},
        false);
    follower.setFormation(QStringLiteral("F01"), QStringLiteral("V"),
                          QStringLiteral("A01"), 1, {}, 2.0, 90.0,
                          QStringLiteral("ACTIVE"));
    scenario.addEntity(follower);

    AirbaseRecord airbase;
    airbase.id = QStringLiteral("BASE01");
    airbase.name = QStringLiteral("Test Airbase");
    airbase.coordinate = QGeoCoordinate(30.0, 80.0);
    airbase.coordinateAvailable = true;
    airbase.region = QStringLiteral("North India");
    airbase.category = QStringLiteral("military");

    QString error;
    int featureCount = 0;
    const QByteArray json = GeoJsonExporter::toJson(
        scenario, QVector<AirbaseRecord>{airbase}, &error, &featureCount);
    QVERIFY2(!json.isEmpty(), qPrintable(error));
    QCOMPARE(featureCount, 4);

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    const QJsonArray features =
        document.object().value(QStringLiteral("features")).toArray();
    QCOMPARE(features.size(), 4);

    const QJsonObject entityProperties =
        features.at(0).toObject().value(QStringLiteral("properties")).toObject();
    QCOMPARE(entityProperties.value(QStringLiteral("formationId")).toString(),
             QStringLiteral("F01"));
    QCOMPARE(entityProperties.value(QStringLiteral("formationType")).toString(),
             QStringLiteral("V"));

    const QJsonObject route = features.at(1).toObject();
    QCOMPARE(route.value(QStringLiteral("geometry")).toObject()
                 .value(QStringLiteral("type")).toString(),
             QStringLiteral("LineString"));
    const QJsonArray routeCoordinates =
        route.value(QStringLiteral("geometry")).toObject()
            .value(QStringLiteral("coordinates")).toArray();
    QCOMPARE(routeCoordinates.at(0).toArray().at(0).toDouble(), 77.0);
    QCOMPARE(routeCoordinates.at(0).toArray().at(1).toDouble(), 28.0);

    const QJsonObject airbaseFeature = features.at(3).toObject();
    const QJsonObject airbaseProperties =
        airbaseFeature.value(QStringLiteral("properties")).toObject();
    QCOMPARE(airbaseProperties.value(QStringLiteral("type")).toString(),
             QStringLiteral("airbase"));
    QCOMPARE(airbaseProperties.value(QStringLiteral("airbaseId")).toString(),
             QStringLiteral("BASE01"));
    const QJsonArray airbaseCoordinates =
        airbaseFeature.value(QStringLiteral("geometry")).toObject()
            .value(QStringLiteral("coordinates")).toArray();
    QCOMPARE(airbaseCoordinates.at(0).toDouble(), 80.0);
    QCOMPARE(airbaseCoordinates.at(1).toDouble(), 30.0);
}

void AirbaseScenarioTests::geoJsonRejectsInvalidCoordinates()
{
    Scenario scenario;
    scenario.setName(QStringLiteral("Invalid coordinate test"));
    scenario.addEntity(Entity(
        QStringLiteral("A01"), QStringLiteral("Bad route"), QStringLiteral("aircraft"),
        28.0, 77.0, 1000.0, 250.0, 90.0, QStringLiteral("ACTIVE"),
        QStringLiteral("TEST01"), 0.0, QStringLiteral("test"),
        QVector<Waypoint>{
            {QStringLiteral("Origin"), 28.0, 77.0, 1000.0},
            {QStringLiteral("Invalid"), 95.0, 78.0, 2000.0}},
        false));

    QString error;
    const QByteArray json = GeoJsonExporter::toJson(scenario, &error);
    QVERIFY(json.isEmpty());
    QVERIFY(error.contains(QStringLiteral("invalid waypoint"), Qt::CaseInsensitive));
}

void AirbaseScenarioTests::geoJsonSkipsSingleWaypointRoute()
{
    Scenario scenario;
    scenario.setName(QStringLiteral("Single waypoint test"));
    scenario.addEntity(Entity(
        QStringLiteral("A01"), QStringLiteral("Short route"), QStringLiteral("aircraft"),
        28.0, 77.0, 1000.0, 250.0, 90.0, QStringLiteral("ACTIVE"),
        QStringLiteral("TEST01"), 0.0, QStringLiteral("test"),
        QVector<Waypoint>{{QStringLiteral("Only"), 28.0, 77.0, 1000.0}}, false));

    QString error;
    const QByteArray json = GeoJsonExporter::toJson(scenario, &error);
    QVERIFY2(!json.isEmpty(), qPrintable(error));
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    const QJsonArray features =
        document.object().value(QStringLiteral("features")).toArray();
    QCOMPARE(features.size(), 1);
    QCOMPARE(features.first().toObject().value(QStringLiteral("geometry"))
                 .toObject().value(QStringLiteral("type")).toString(),
             QStringLiteral("Point"));
}

void AirbaseScenarioTests::geoJsonExportsScenarioWithoutRoutes()
{
    Scenario scenario(QStringLiteral("No routes"), {}, {});
    scenario.addEntity(Entity(
        QStringLiteral("A01"), QStringLiteral("No route"),
        QStringLiteral("aircraft"), 28.0, 77.0, 1000.0,
        250.0, 90.0, QStringLiteral("ACTIVE")));

    QString error;
    const QByteArray json = GeoJsonExporter::toJson(scenario, &error);
    QVERIFY2(!json.isEmpty(), qPrintable(error));
    QVERIFY(error.isEmpty());
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    const QJsonArray features =
        document.object().value(QStringLiteral("features")).toArray();
    QCOMPARE(features.size(), 1);
    QCOMPARE(features.first().toObject().value(QStringLiteral("geometry"))
                 .toObject().value(QStringLiteral("type")).toString(),
             QStringLiteral("Point"));
}

void AirbaseScenarioTests::geoJsonExportsEmptyScenario()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString filePath = directory.filePath(QStringLiteral("empty.geojson"));

    Scenario scenario;
    QString error;
    GeoJsonExporter::ExportSummary summary;
    QVERIFY(!GeoJsonExporter::writeFile(scenario, {}, filePath, &summary, &error));
    QVERIFY(error.contains(QStringLiteral("No exportable")));
    QVERIFY(!QFileInfo::exists(filePath));
}

void AirbaseScenarioTests::geoJsonWritesAndOverwritesFileUrl()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString requestedPath = directory.filePath(QStringLiteral("export"));
    const QString expectedPath = requestedPath + QStringLiteral(".geojson");
    const QString fileUrl = QUrl::fromLocalFile(requestedPath).toString();

    Scenario firstScenario(QStringLiteral("First export"), {}, {});
    firstScenario.addEntity(Entity(
        QStringLiteral("A01"), QStringLiteral("Aircraft One"),
        QStringLiteral("aircraft"), 12.0, 77.0, 8000.0,
        250.0, 90.0, QStringLiteral("ACTIVE")));
    GeoJsonExporter::ExportSummary firstSummary;
    QString error;
    QVERIFY2(GeoJsonExporter::writeFile(firstScenario, {}, fileUrl,
                                        &firstSummary, &error),
             qPrintable(error));
    QCOMPARE(firstSummary.absolutePath, QDir::cleanPath(expectedPath));
    QCOMPARE(firstSummary.featureCount, 1);
    QVERIFY(firstSummary.fileSizeBytes > 0);

    Scenario secondScenario(QStringLiteral("Second export"), {}, {});
    secondScenario.addEntity(Entity(
        QStringLiteral("A02"), QStringLiteral("Aircraft Two"),
        QStringLiteral("aircraft"), 13.0, 78.0, 9000.0,
        260.0, 180.0, QStringLiteral("ACTIVE")));
    secondScenario.addEntity(Entity(
        QStringLiteral("A03"), QStringLiteral("Aircraft Three"),
        QStringLiteral("aircraft"), 14.0, 79.0, 10000.0,
        270.0, 270.0, QStringLiteral("ACTIVE")));
    GeoJsonExporter::ExportSummary secondSummary;
    QVERIFY2(GeoJsonExporter::writeFile(secondScenario, {}, expectedPath,
                                        &secondSummary, &error),
             qPrintable(error));
    QCOMPARE(secondSummary.absolutePath, QDir::cleanPath(expectedPath));
    QCOMPARE(secondSummary.featureCount, 2);

    QFile file(expectedPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray json = file.readAll();
    QVERIFY(!json.isEmpty());
    QCOMPARE(QFileInfo(expectedPath).size(), secondSummary.fileSizeBytes);
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    QCOMPARE(document.object().value(QStringLiteral("name")).toString(),
             QStringLiteral("Second export"));
    QCOMPARE(document.object().value(QStringLiteral("features")).toArray().size(), 2);
}

void AirbaseScenarioTests::geoJsonWritesRelativePathAsAbsolutePath()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString previousDirectory = QDir::currentPath();
    QVERIFY(QDir::setCurrent(directory.path()));

    Scenario scenario(QStringLiteral("Relative export"), {}, {});
    scenario.addEntity(Entity(
        QStringLiteral("A01"), QStringLiteral("Aircraft One"),
        QStringLiteral("aircraft"), 12.0, 77.0, 8000.0,
        250.0, 90.0, QStringLiteral("ACTIVE")));
    GeoJsonExporter::ExportSummary summary;
    QString error;
    const bool exported = GeoJsonExporter::writeFile(
        scenario, {}, QStringLiteral("relative.geojson"), &summary, &error);
    QVERIFY(QDir::setCurrent(previousDirectory));

    QVERIFY2(exported, qPrintable(error));
    QCOMPARE(summary.absolutePath,
             QDir::cleanPath(directory.filePath(QStringLiteral("relative.geojson"))));
    QVERIFY(QFileInfo(summary.absolutePath).exists());
}

void AirbaseScenarioTests::geoJsonWritesToDesktopAndDocuments()
{
    const QList<QStandardPaths::StandardLocation> locations = {
        QStandardPaths::DesktopLocation,
        QStandardPaths::DocumentsLocation
    };
    Scenario scenario(QStringLiteral("Standard location export"), {}, {});
    scenario.addEntity(Entity(
        QStringLiteral("A01"), QStringLiteral("Aircraft One"),
        QStringLiteral("aircraft"), 12.0, 77.0, 8000.0,
        250.0, 90.0, QStringLiteral("ACTIVE")));

    int testedLocations = 0;
    for (QStandardPaths::StandardLocation location : locations) {
        const QString basePath = QStandardPaths::writableLocation(location);
        if (basePath.isEmpty() || !QFileInfo(basePath).isDir())
            continue;

        QTemporaryDir directory(
            QDir(basePath).filePath(QStringLiteral("AeroSimGeoJSON-XXXXXX")));
        QVERIFY2(directory.isValid(),
                 qPrintable(QStringLiteral("Cannot create test folder under %1")
                                .arg(basePath)));

        const QString filePath =
            directory.filePath(QStringLiteral("scenario.geojson"));
        GeoJsonExporter::ExportSummary summary;
        QString error;
        QVERIFY2(GeoJsonExporter::writeFile(
                     scenario, {}, filePath, &summary, &error),
                 qPrintable(error));
        QCOMPARE(summary.absolutePath, QDir::cleanPath(filePath));
        QCOMPARE(summary.featureCount, 1);
        QVERIFY(summary.fileSizeBytes > 0);
        QVERIFY(QFileInfo(summary.absolutePath).exists());
        ++testedLocations;
    }

    if (testedLocations == 0)
        QSKIP("Desktop and Documents locations are unavailable on this system");
}

void AirbaseScenarioTests::geoJsonRejectsUnavailablePath()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Scenario scenario(QStringLiteral("Path validation"), {}, {});
    scenario.addEntity(Entity(
        QStringLiteral("A01"), QStringLiteral("Aircraft One"),
        QStringLiteral("aircraft"), 12.0, 77.0, 8000.0,
        250.0, 90.0, QStringLiteral("ACTIVE")));

    GeoJsonExporter::ExportSummary summary;
    QString error;
    const QString missingDirectoryPath =
        directory.filePath(QStringLiteral("missing/scenario.geojson"));
    QVERIFY(!GeoJsonExporter::writeFile(scenario, {}, missingDirectoryPath,
                                        &summary, &error));
    QVERIFY(error.contains(QStringLiteral("Directory unavailable")));
    QVERIFY(!QFileInfo::exists(missingDirectoryPath));

    error.clear();
    QVERIFY(!GeoJsonExporter::writeFile(
        scenario, {}, QStringLiteral("https://example.invalid/scenario.geojson"),
        &summary, &error));
    QVERIFY(error.contains(QStringLiteral("Directory unavailable")));
}

void AirbaseScenarioTests::csvExportsAircraftAndAirbases()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Scenario scenario(QStringLiteral("CSV test"), {}, {});
    Entity first(QStringLiteral("A01"), QStringLiteral("First"),
                 QStringLiteral("aircraft"), 10.0, 70.0, 1000.0,
                 200.0, 10.0, QStringLiteral("ACTIVE"),
                 QStringLiteral("Élite,\"01\""));
    first.setFormation(QStringLiteral("FORM-1"), QStringLiteral("V"),
                       QStringLiteral("A01"), 0, {}, 2.0, 90.0,
                       QStringLiteral("ACTIVE"));
    scenario.addEntity(first);
    scenario.addEntity(Entity(
        QStringLiteral("A02"), QStringLiteral("Second"), QStringLiteral("aircraft"),
        11.0, 71.0, 2000.0, 210.0, 20.0, QStringLiteral("ACTIVE"),
        QStringLiteral("TEST02")));

    AircraftSimulationState firstState;
    firstState.id = QStringLiteral("A01");
    firstState.callsign = QStringLiteral("unused simulation callsign");
    firstState.latitude = 12.5;
    firstState.longitude = 77.5;
    firstState.altitude = 9000.0;
    firstState.heading = 95.0;
    firstState.speed = 260.0;
    firstState.status = QStringLiteral("Taxi, \"climb\"");
    AircraftSimulationState secondState;
    secondState.id = QStringLiteral("A02");
    secondState.latitude = 13.5;
    secondState.longitude = 78.5;
    secondState.altitude = 10000.0;
    secondState.heading = 180.0;
    secondState.speed = 270.0;
    secondState.status = QStringLiteral("ACTIVE");
    QHash<QString, AircraftSimulationState> states{
        {firstState.id, firstState}, {secondState.id, secondState}};

    AirbaseRecord airbase;
    airbase.id = QStringLiteral("BASE01");
    airbase.name = QStringLiteral("Test Base");
    airbase.coordinate = QGeoCoordinate(30.0, 80.0);
    airbase.coordinateAvailable = true;

    const QString requestedPath = directory.filePath(QStringLiteral("scenario"));
    CsvExporter::ExportSummary summary;
    QString error;
    QVERIFY2(CsvExporter::writeFile(scenario, {airbase}, states, requestedPath,
                                    &summary, &error),
             qPrintable(error));
    const QString expectedPath = requestedPath + QStringLiteral(".csv");
    QCOMPARE(summary.absolutePath, QDir::cleanPath(expectedPath));
    QCOMPARE(summary.rowCount, 3);
    QVERIFY(summary.fileSizeBytes > 0);

    QFile file(expectedPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray csv = file.readAll();
    QVERIFY(csv.contains(QString::fromUtf8("Élite").toUtf8()));
    const QStringList lines = QString::fromUtf8(csv).split(QLatin1Char('\n'));
    QCOMPARE(lines.at(0),
             QStringLiteral("ID,Callsign,EntityType,Latitude,Longitude,Altitude,Heading,Speed,Status,FormationID,RecordType,CommandSequence,Prompt,Timestamp,ProcessingStatus,ParsedOperation"));
    QCOMPARE(lines.at(1),
             QStringLiteral("A01,\"Élite,\"\"01\"\"\",aircraft,12.5,77.5,9000,95,260,\"Taxi, \"\"climb\"\"\",FORM-1,Aircraft,,,,,"));
    QCOMPARE(lines.at(2),
             QStringLiteral("A02,TEST02,aircraft,13.5,78.5,10000,180,270,ACTIVE,,Aircraft,,,,,"));
    QCOMPARE(lines.at(3), QStringLiteral("BASE01,,airbase,30,80,,,,,,Airbase,,,,,"));
    file.close();

    firstState.latitude = 15.0;
    states.insert(firstState.id, firstState);
    QVERIFY2(CsvExporter::writeFile(scenario, {airbase}, states, expectedPath,
                                    &summary, &error),
             qPrintable(error));
    QFile overwrittenFile(expectedPath);
    QVERIFY(overwrittenFile.open(QIODevice::ReadOnly));
    QVERIFY(overwrittenFile.readAll().contains("A01,\"Élite,\"\"01\"\"\",aircraft,15,77.5"));
}

void AirbaseScenarioTests::csvExportsEmptyScenario()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Scenario scenario;
    CsvExporter::ExportSummary summary;
    QString error;
    const QString filePath = directory.filePath(QStringLiteral("empty.csv"));
    QVERIFY(!CsvExporter::writeFile(scenario, {}, {}, filePath, &summary, &error));
    QVERIFY(error.contains(QStringLiteral("No exportable")));
    QVERIFY(!QFileInfo::exists(filePath));
}

void AirbaseScenarioTests::csvRejectsMissingStateAndUnavailablePath()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Scenario scenario(QStringLiteral("CSV errors"), {}, {});
    scenario.addEntity(Entity(
        QStringLiteral("A01"), QStringLiteral("Aircraft One"),
        QStringLiteral("aircraft"), 12.0, 77.0, 8000.0,
        250.0, 90.0, QStringLiteral("ACTIVE")));

    CsvExporter::ExportSummary summary;
    QString error;
    const QString outputPath = directory.filePath(QStringLiteral("missing-state.csv"));
    QVERIFY(!CsvExporter::writeFile(scenario, {}, {}, outputPath, &summary, &error));
    QVERIFY(error.contains(QStringLiteral("no current simulation state")));
    QVERIFY(!QFileInfo::exists(outputPath));

    AircraftSimulationState state;
    state.id = QStringLiteral("A01");
    state.latitude = 12.0;
    state.longitude = 77.0;
    state.altitude = 8000.0;
    QHash<QString, AircraftSimulationState> states{{state.id, state}};
    error.clear();
    const QString missingDirectoryPath =
        directory.filePath(QStringLiteral("missing/scenario.csv"));
    QVERIFY(!CsvExporter::writeFile(scenario, {}, states, missingDirectoryPath,
                                    &summary, &error));
    QVERIFY(error.contains(QStringLiteral("Directory unavailable")));
    QVERIFY(!QFileInfo::exists(missingDirectoryPath));
}

void AirbaseScenarioTests::commandHistoryCapturesOriginalPromptsAndFailures()
{
    ScenarioEngine engine;
    QSignalSpy historySpy(&engine, &ScenarioEngine::commandHistoryChanged);
    const QString validPrompt =
        QStringLiteral("  Generate 2 aircraft at Hindon Airbase  ");
    QVERIFY2(engine.generateScenario(validPrompt), qPrintable(engine.statusMessage()));
    QVERIFY(!engine.generateScenario(QStringLiteral("abracadabra, qzx!")));
    const QString escapedPrompt =
        QString::fromUtf8("not a command, \"quoted\"\nsecond line");
    QVERIFY(!engine.generateScenario(escapedPrompt));
    QVERIFY(!engine.generateScenario(QStringLiteral(" \n\t ")));

    const QVector<CommandHistoryEntry> history = engine.commandHistory();
    QCOMPARE(history.size(), 3);
    QCOMPARE(engine.commandHistoryCount(), 3);
    QCOMPARE(historySpy.count(), 3);
    QCOMPARE(history.at(0).sequenceNumber, 1);
    QCOMPARE(history.at(0).prompt, validPrompt);
    QCOMPARE(history.at(0).processingStatus, QStringLiteral("succeeded"));
    QVERIFY(!history.at(0).parsedOperation.isEmpty());
    const QDateTime firstTimestamp =
        QDateTime::fromString(history.at(0).timestamp, Qt::ISODateWithMs);
    QVERIFY(firstTimestamp.isValid());
    QCOMPARE(history.at(1).sequenceNumber, 2);
    QCOMPARE(history.at(1).prompt, QStringLiteral("abracadabra, qzx!"));
    QCOMPARE(history.at(1).processingStatus, QStringLiteral("failed"));
    QVERIFY(history.at(1).parsedOperation.isEmpty());
    const QDateTime secondTimestamp =
        QDateTime::fromString(history.at(1).timestamp, Qt::ISODateWithMs);
    QVERIFY(secondTimestamp.isValid());
    QVERIFY(firstTimestamp <= secondTimestamp);
    QCOMPARE(history.at(2).sequenceNumber, 3);
    QCOMPARE(history.at(2).prompt, escapedPrompt);
    QCOMPARE(history.at(2).processingStatus, QStringLiteral("failed"));
    const QDateTime thirdTimestamp =
        QDateTime::fromString(history.at(2).timestamp, Qt::ISODateWithMs);
    QVERIFY(thirdTimestamp.isValid());
    QVERIFY(secondTimestamp <= thirdTimestamp);
}

void AirbaseScenarioTests::commandHistorySurvivesScenarioMutations()
{
    ScenarioEngine engine;
    const QStringList prompts = {
        QStringLiteral("Generate 2 aircraft at Hindon Airbase"),
        QStringLiteral("Move AERO001 to Pune Airbase"),
        QStringLiteral("Stop AERO001"),
        QStringLiteral("Create 3 aircraft in a V formation"),
        QStringLiteral("Disband the formation."),
        QStringLiteral("Remove all aircraft.")};
    for (const QString &prompt : prompts)
        QVERIFY2(engine.generateScenario(prompt), qPrintable(engine.statusMessage()));

    QCOMPARE(engine.entityCount(), 0);
    engine.resetSimulation();
    const QVector<CommandHistoryEntry> history = engine.commandHistory();
    QCOMPARE(history.size(), prompts.size());
    for (int index = 0; index < prompts.size(); ++index) {
        QCOMPARE(history.at(index).sequenceNumber, index + 1);
        QCOMPARE(history.at(index).prompt, prompts.at(index));
        QCOMPARE(history.at(index).processingStatus, QStringLiteral("succeeded"));
    }
}

void AirbaseScenarioTests::exportersIncludeMatchingCommandHistory()
{
    Scenario scenario(QStringLiteral("History export"), {}, {});
    Entity aircraft(
        QStringLiteral("A01"), QStringLiteral("Flight One"),
        QStringLiteral("aircraft"), 28.0, 77.0, 9000.0,
        250.0, 90.0, QStringLiteral("ACTIVE"), QStringLiteral("TEST01"),
        0.0, QStringLiteral("test"),
        QVector<Waypoint>{
            {QStringLiteral("Origin"), 28.0, 77.0, 9000.0},
            {QStringLiteral("Destination"), 29.0, 78.0, 9000.0}},
        false);
    aircraft.setFormation(QStringLiteral("FORM-1"), QStringLiteral("V"),
                          QStringLiteral("A01"), 0, {}, 2.0, 90.0,
                          QStringLiteral("ACTIVE"));
    scenario.addEntity(aircraft);

    AirbaseRecord airbase;
    airbase.id = QStringLiteral("BASE01");
    airbase.name = QStringLiteral("Test Base");
    airbase.coordinate = QGeoCoordinate(30.0, 80.0);
    airbase.coordinateAvailable = true;

    const QString originalPrompt =
        QString::fromUtf8("Add aircraft, \"Élite\"\nthen wait.");
    const QVector<CommandHistoryEntry> history = {
        {1, originalPrompt, QStringLiteral("2026-11-13T10:00:00.123Z"),
         QStringLiteral("succeeded"), QStringLiteral("ADD_ENTITIES")},
        {2, QStringLiteral("nonsense, ???"),
         QStringLiteral("2026-11-13T10:00:01.456Z"),
         QStringLiteral("failed"), QString()}};

    QString error;
    int featureCount = 0;
    const QByteArray json =
        GeoJsonExporter::toJson(scenario, {airbase}, history, &error, &featureCount);
    QVERIFY2(!json.isEmpty(), qPrintable(error));
    QCOMPARE(featureCount, 3);
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    const QJsonObject root = document.object();
    QCOMPARE(root.value(QStringLiteral("type")).toString(),
             QStringLiteral("FeatureCollection"));
    QCOMPARE(root.value(QStringLiteral("features")).toArray().size(), 3);
    const QJsonObject exportProperties =
        root.value(QStringLiteral("properties")).toObject();
    QCOMPARE(exportProperties.value(QStringLiteral("commandCount")).toInt(), 2);
    const QJsonArray commands =
        exportProperties.value(QStringLiteral("commandHistory")).toArray();
    QCOMPARE(commands.size(), 2);
    QCOMPARE(commands.at(0).toObject()
                 .value(QStringLiteral("sequenceNumber")).toInt(), 1);
    QCOMPARE(commands.at(0).toObject()
                 .value(QStringLiteral("prompt")).toString(), originalPrompt);
    QCOMPARE(commands.at(0).toObject()
                 .value(QStringLiteral("timestamp")).toString(),
             history.at(0).timestamp);
    QCOMPARE(commands.at(0).toObject()
                 .value(QStringLiteral("parsedOperation")).toString(),
             QStringLiteral("ADD_ENTITIES"));
    QCOMPARE(commands.at(1).toObject()
                 .value(QStringLiteral("processingStatus")).toString(),
             QStringLiteral("failed"));
    QVERIFY(!commands.at(1).toObject().contains(
        QStringLiteral("parsedOperation")));
    QCOMPARE(root.value(QStringLiteral("features")).toArray().at(0).toObject()
                 .value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("formationId")).toString(),
             QStringLiteral("FORM-1"));

    AircraftSimulationState state;
    state.id = QStringLiteral("A01");
    state.latitude = 28.25;
    state.longitude = 77.25;
    state.altitude = 9050.0;
    state.heading = 91.0;
    state.speed = 251.0;
    state.status = QStringLiteral("ACTIVE");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString csvPath = directory.filePath(QStringLiteral("history.csv"));
    CsvExporter::ExportSummary summary;
    QVERIFY2(CsvExporter::writeFile(
                 scenario, {airbase}, {{state.id, state}}, history,
                 csvPath, &summary, &error),
             qPrintable(error));
    QCOMPARE(summary.rowCount, 4);

    QFile csvFile(csvPath);
    QVERIFY(csvFile.open(QIODevice::ReadOnly));
    const QByteArray csvBytes = csvFile.readAll();
    const QString csv = QString::fromUtf8(csvBytes);
    const QString expectedHeader = QStringLiteral(
        "ID,Callsign,EntityType,Latitude,Longitude,Altitude,Heading,Speed,Status,FormationID,"
        "RecordType,CommandSequence,Prompt,Timestamp,ProcessingStatus,ParsedOperation");
    QVERIFY(csv.startsWith(expectedHeader + QLatin1Char('\n')));
    QVERIFY(csvBytes.contains(QString::fromUtf8("Élite").toUtf8()));
    const QString expectedCommandRow =
        QString(10, QLatin1Char(',')) +
        QStringLiteral("Command,1,\"Add aircraft, \"\"Élite\"\"\nthen wait.\","
                       "2026-11-13T10:00:00.123Z,succeeded,ADD_ENTITIES\n");
    QVERIFY(csv.contains(expectedCommandRow));
    const QString failedCommandRow =
        QString(10, QLatin1Char(',')) +
        QStringLiteral("Command,2,\"nonsense, ???\","
                       "2026-11-13T10:00:01.456Z,failed,\n");
    QVERIFY(csv.contains(failedCommandRow));

    Scenario commandOnlyScenario;
    const QString commandOnlyGeoPath =
        directory.filePath(QStringLiteral("commands-only.geojson"));
    GeoJsonExporter::ExportSummary geoSummary;
    QVERIFY2(GeoJsonExporter::writeFile(
                 commandOnlyScenario, {}, history, commandOnlyGeoPath,
                 &geoSummary, &error),
             qPrintable(error));
    QCOMPARE(geoSummary.featureCount, 0);
    QFile commandOnlyGeoFile(commandOnlyGeoPath);
    QVERIFY(commandOnlyGeoFile.open(QIODevice::ReadOnly));
    const QJsonDocument commandOnlyGeo =
        QJsonDocument::fromJson(commandOnlyGeoFile.readAll(), &parseError);
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    QCOMPARE(commandOnlyGeo.object().value(QStringLiteral("features"))
                 .toArray().size(), 0);
    QCOMPARE(commandOnlyGeo.object().value(QStringLiteral("properties"))
                 .toObject().value(QStringLiteral("commandCount")).toInt(), 2);

    const QString commandOnlyCsvPath =
        directory.filePath(QStringLiteral("commands-only.csv"));
    QVERIFY2(CsvExporter::writeFile(commandOnlyScenario, {}, {}, history,
                                    commandOnlyCsvPath, &summary, &error),
             qPrintable(error));
    QCOMPARE(summary.rowCount, 2);
}

void AirbaseScenarioTests::exportsCurrentSimulationStateToBothFormats()
{
    ScenarioEngine engine;
    QVERIFY2(engine.generateScenario(
                 QStringLiteral("Generate 2 aircraft at Hindon Airbase")),
             qPrintable(engine.statusMessage()));
    const QVariantMap initialState = engine.aircraftInformation(QStringLiteral("A01"));
    QVERIFY(!initialState.isEmpty());
    QVERIFY2(engine.generateScenario(QStringLiteral("Move AERO001 to Pune Airbase")),
             qPrintable(engine.statusMessage()));
    QVERIFY(engine.advanceSimulationForTest(1.0));

    const QVariantMap currentState = engine.aircraftInformation(QStringLiteral("A01"));
    QVERIFY(!currentState.isEmpty());
    QVERIFY(currentState.value(QStringLiteral("latitude")).toDouble() !=
                initialState.value(QStringLiteral("latitude")).toDouble() ||
            currentState.value(QStringLiteral("longitude")).toDouble() !=
                initialState.value(QStringLiteral("longitude")).toDouble());

    const QString outputDirectory =
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("export-test-artifacts"));
    QVERIFY(QDir().mkpath(outputDirectory));
    const QString geoJsonPath =
        QDir(outputDirectory).filePath(QStringLiteral("current-simulation.geojson"));
    const QString csvPath =
        QDir(outputDirectory).filePath(QStringLiteral("current-simulation.csv"));
    QVERIFY2(engine.exportScenarioGeoJson(geoJsonPath), qPrintable(engine.statusMessage()));
    QVERIFY(engine.statusMessage().contains(QFileInfo(geoJsonPath).absoluteFilePath()));
    QVERIFY(engine.statusMessage().contains(QStringLiteral("Features:")));
    QFile geoJsonFile(geoJsonPath);
    QVERIFY(geoJsonFile.open(QIODevice::ReadOnly));
    QJsonParseError parseError;
    const QJsonDocument geoJson =
        QJsonDocument::fromJson(geoJsonFile.readAll(), &parseError);
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    QCOMPARE(geoJson.object().value(QStringLiteral("type")).toString(),
             QStringLiteral("FeatureCollection"));
    const QVector<CommandHistoryEntry> history = engine.commandHistory();
    QCOMPARE(history.size(), 2);
    const QJsonObject exportProperties =
        geoJson.object().value(QStringLiteral("properties")).toObject();
    QCOMPARE(exportProperties.value(QStringLiteral("commandCount")).toInt(),
             history.size());
    const QJsonArray geoCommands =
        exportProperties.value(QStringLiteral("commandHistory")).toArray();
    QCOMPARE(geoCommands.size(), history.size());
    for (int index = 0; index < history.size(); ++index) {
        QCOMPARE(geoCommands.at(index).toObject()
                     .value(QStringLiteral("sequenceNumber")).toInt(),
                 history.at(index).sequenceNumber);
        QCOMPARE(geoCommands.at(index).toObject()
                     .value(QStringLiteral("prompt")).toString(),
                 history.at(index).prompt);
        QCOMPARE(geoCommands.at(index).toObject()
                     .value(QStringLiteral("timestamp")).toString(),
                 history.at(index).timestamp);
    }
    const QJsonArray features =
        geoJson.object().value(QStringLiteral("features")).toArray();
    bool foundAircraftPoint = false;
    for (const QJsonValue &featureValue : features) {
        const QJsonObject feature = featureValue.toObject();
        const QJsonObject properties =
            feature.value(QStringLiteral("properties")).toObject();
        if (properties.value(QStringLiteral("entityId")).toString() !=
                QStringLiteral("A01") ||
            feature.value(QStringLiteral("geometry")).toObject()
                    .value(QStringLiteral("type")).toString() !=
                QStringLiteral("Point"))
            continue;

        const QJsonArray coordinates =
            feature.value(QStringLiteral("geometry")).toObject()
                .value(QStringLiteral("coordinates")).toArray();
        QCOMPARE(coordinates.at(0).toDouble(),
                 currentState.value(QStringLiteral("longitude")).toDouble());
        QCOMPARE(coordinates.at(1).toDouble(),
                 currentState.value(QStringLiteral("latitude")).toDouble());
        QCOMPARE(coordinates.at(2).toDouble(),
                 currentState.value(QStringLiteral("altitude")).toDouble());
        foundAircraftPoint = true;
    }
    QVERIFY(foundAircraftPoint);

    QVERIFY2(engine.exportScenarioCsv(csvPath), qPrintable(engine.statusMessage()));
    QVERIFY(engine.statusMessage().contains(QFileInfo(csvPath).absoluteFilePath()));
    QVERIFY(engine.statusMessage().contains(QStringLiteral("Rows:")));
    QFile csvFile(csvPath);
    QVERIFY(csvFile.open(QIODevice::ReadOnly));
    const QStringList csvLines =
        QString::fromUtf8(csvFile.readAll()).split(QLatin1Char('\n'));
    QCOMPARE(csvLines.first(),
             QStringLiteral("ID,Callsign,EntityType,Latitude,Longitude,Altitude,Heading,Speed,Status,FormationID,RecordType,CommandSequence,Prompt,Timestamp,ProcessingStatus,ParsedOperation"));
    const QStringList aircraftFields = csvLines.at(1).split(QLatin1Char(','));
    QCOMPARE(aircraftFields.at(0), QStringLiteral("A01"));
    QCOMPARE(aircraftFields.at(3).toDouble(),
             currentState.value(QStringLiteral("latitude")).toDouble());
    QCOMPARE(aircraftFields.at(4).toDouble(),
             currentState.value(QStringLiteral("longitude")).toDouble());
    QCOMPARE(aircraftFields.at(5).toDouble(),
             currentState.value(QStringLiteral("altitude")).toDouble());
    QVector<QStringList> csvCommandRows;
    for (const QString &line : csvLines) {
        const QStringList fields = line.split(QLatin1Char(','));
        if (fields.size() == 16 &&
            fields.at(10) == QStringLiteral("Command"))
            csvCommandRows.append(fields);
    }
    QCOMPARE(csvCommandRows.size(), history.size());
    for (int index = 0; index < history.size(); ++index) {
        QCOMPARE(csvCommandRows.at(index).at(11).toInt(),
                 history.at(index).sequenceNumber);
        QCOMPARE(csvCommandRows.at(index).at(12), history.at(index).prompt);
        QCOMPARE(csvCommandRows.at(index).at(13), history.at(index).timestamp);
        QCOMPARE(csvCommandRows.at(index).at(14), history.at(index).processingStatus);
        QCOMPARE(csvCommandRows.at(index).at(15), history.at(index).parsedOperation);
        QCOMPARE(csvCommandRows.at(index).at(12),
                 geoCommands.at(index).toObject()
                     .value(QStringLiteral("prompt")).toString());
    }
    QVERIFY(QFileInfo(geoJsonPath).isAbsolute());
    QVERIFY(QFileInfo(csvPath).isAbsolute());
    QVERIFY(QFileInfo(geoJsonPath).size() > 0);
    QVERIFY(QFileInfo(csvPath).size() > 0);
}

QTEST_MAIN(AirbaseScenarioTests)
#include "airbase_tests.moc"
