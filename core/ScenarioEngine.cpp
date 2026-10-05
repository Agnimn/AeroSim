#include "ScenarioEngine.h"

#include <QPair>
#include <utility>

namespace
{
    const std::vector<QPair<double, double>> northIndiaPoints = {
        {28.6139, 77.2090}, {30.7333, 76.7794}, {26.9124, 75.7873}, {27.1767, 78.0081}, {31.6340, 74.8723}, {30.3165, 78.0322}, {29.9457, 78.1642}, {25.4358, 81.8463}};
    const std::vector<QPair<double, double>> southIndiaPoints = {
        {12.9716, 77.5946}, {13.0827, 80.2707}, {17.3850, 78.4867}, {11.0168, 76.9558}, {15.3173, 75.7139}, {9.9312, 76.2673}};
    const std::vector<QPair<double, double>> eastIndiaPoints = {
        {22.5726, 88.3639}, {20.2961, 85.8245}, {26.1445, 91.7362}, {23.3441, 85.3096}, {25.5941, 85.1376}};
    const std::vector<QPair<double, double>> westIndiaPoints = {
        {19.0760, 72.8777}, {23.0225, 72.5714}, {21.1702, 72.8311}, {15.4909, 73.8278}, {18.5204, 73.8567}};
    const std::vector<QPair<double, double>> defaultPoints = {{22.5, 79.0}};
}

ScenarioEngine::ScenarioEngine(QObject *parent)
    : QObject(parent)
{
}

EntityModel *ScenarioEngine::entityModel() { return &m_entityModel; }
QString ScenarioEngine::scenarioName() const { return m_scenario.name(); }
QString ScenarioEngine::region() const { return m_scenario.region(); }
QString ScenarioEngine::entityType() const { return m_entityType; }
int ScenarioEngine::entityCount() const { return static_cast<int>(m_scenario.entityCount()); }
QString ScenarioEngine::statusMessage() const { return m_statusMessage; }
bool ScenarioEngine::scenarioGenerated() const { return m_scenarioGenerated; }
double ScenarioEngine::centerLatitude() const { return m_centerLatitude; }
double ScenarioEngine::centerLongitude() const { return m_centerLongitude; }
const Scenario &ScenarioEngine::scenario() const { return m_scenario; }
QString ScenarioEngine::selectedEntityId() const { return m_selectedEntityId; }
QString ScenarioEngine::selectedEntityName() const { return m_selectedEntityName; }

bool ScenarioEngine::selectAircraft(const QString &entityId)
{
    for (const Entity &entity : m_scenario.entities())
    {
        if (entity.id() != entityId || entity.type() != QStringLiteral("aircraft"))
            continue;

        if (m_selectedEntityId != entity.id())
        {
            m_selectedEntityId = entity.id();
            m_selectedEntityName = entity.name();
            emit selectedEntityChanged();
        }
        return true;
    }
    return false;
}

void ScenarioEngine::clearSelectedAircraft()
{
    if (m_selectedEntityId.isEmpty())
        return;
    m_selectedEntityId.clear();
    m_selectedEntityName.clear();
    emit selectedEntityChanged();
}

bool ScenarioEngine::generateScenario(const QString &prompt)
{
    ScenarioRequest request;
    QString errorMessage;
    if (!m_parser.parsePrompt(prompt, request, &errorMessage))
    {
        setStatusMessage(errorMessage);
        return false;
    }

    Scenario generated(scenarioNameForRegion(request.region), prompt.trimmed(), request.region);
    const auto &points = pointsForRegion(request.region);
    const QString prefix = request.entityType.toUpper();
    for (int index = 0; index < request.entityCount; ++index)
    {
        const auto &point = points.at(static_cast<std::size_t>(index) % points.size());
        const QString suffix = QStringLiteral("%1").arg(index + 1, 2, 10, QLatin1Char('0'));
        generated.addEntity(Entity(prefix + QLatin1Char('_') + suffix,
                                   displayType(request.entityType) + QLatin1Char(' ') + suffix,
                                   request.entityType,
                                   point.first,
                                   point.second,
                                   request.altitude,
                                   request.speed,
                                   request.heading,
                                   QStringLiteral("active")));
    }

    m_scenario = std::move(generated);
    clearSelectedAircraft();
    m_entityType = displayType(request.entityType);
    regionCenter(request.region, m_centerLatitude, m_centerLongitude);
    m_entityModel.setEntities(m_scenario.entities());
    m_scenarioGenerated = true;
    emit scenarioChanged();
    setStatusMessage(QStringLiteral("GENERATED"));
    return true;
}

QString ScenarioEngine::displayType(const QString &type)
{
    if (type == QStringLiteral("ground_station"))
        return QStringLiteral("Ground Station");
    if (type == QStringLiteral("ship"))
        return QStringLiteral("Ship");
    return QStringLiteral("Aircraft");
}

QString ScenarioEngine::scenarioNameForRegion(const QString &region)
{
    return region == QStringLiteral("Unknown Region")
               ? QStringLiteral("Scenario")
               : region + QStringLiteral(" Scenario");
}

void ScenarioEngine::regionCenter(const QString &region, double &latitude, double &longitude)
{
    if (region == QStringLiteral("North India"))
    {
        latitude = 29.0;
        longitude = 79.0;
    }
    else if (region == QStringLiteral("South India"))
    {
        latitude = 13.5;
        longitude = 78.5;
    }
    else if (region == QStringLiteral("East India"))
    {
        latitude = 23.5;
        longitude = 87.5;
    }
    else if (region == QStringLiteral("West India"))
    {
        latitude = 21.0;
        longitude = 73.5;
    }
    else
    {
        latitude = 22.5;
        longitude = 79.0;
    }
}

const std::vector<QPair<double, double>> &ScenarioEngine::pointsForRegion(const QString &region)
{
    if (region == QStringLiteral("North India"))
        return northIndiaPoints;
    if (region == QStringLiteral("South India"))
        return southIndiaPoints;
    if (region == QStringLiteral("East India"))
        return eastIndiaPoints;
    if (region == QStringLiteral("West India"))
        return westIndiaPoints;
    return defaultPoints;
}

void ScenarioEngine::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message)
        return;
    m_statusMessage = message;
    emit statusMessageChanged();
}
