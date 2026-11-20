#include "AIParser.h"

#include "../data/AirbaseRegistry.h"
#include "../core/FormationGenerator.h"

#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>
#include <QStringList>
#include <cmath>

namespace
{
    QString normalizeText(const QString &text)
    {
        QString normalized = text.trimmed();
        normalized.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
        return normalized.toLower();
    }

    QString canonicalTypeForText(const QString &text)
    {
        const QString normalized = normalizeText(text);

        if (normalized.contains(QStringLiteral("radar")))
            return QStringLiteral("radar_site");
        if (normalized.contains(QStringLiteral("ground station")) || normalized.contains(QStringLiteral("ground_station")))
            return QStringLiteral("ground_station");
        if (normalized.contains(QStringLiteral("shipborne")) || normalized.contains(QStringLiteral("ship")))
            return QStringLiteral("ship");
        if (normalized.contains(QStringLiteral("aircraft")) || normalized.contains(QStringLiteral("fighter")) ||
            normalized.contains(QStringLiteral("plane")) || normalized.contains(QStringLiteral("jet")))
            return QStringLiteral("aircraft");
        return {};
    }

    QString regionFromText(const QString &text)
    {
        const QString normalized = normalizeText(text);

        if (normalized.contains(QStringLiteral("north india")))
            return QStringLiteral("North India");
        if (normalized.contains(QStringLiteral("south india")))
            return QStringLiteral("South India");
        if (normalized.contains(QStringLiteral("east india")))
            return QStringLiteral("East India");
        if (normalized.contains(QStringLiteral("west india")))
            return QStringLiteral("West India");
        if (normalized.contains(QStringLiteral("delhi")))
            return QStringLiteral("Delhi");
        if (normalized.contains(QStringLiteral("bengaluru")) || normalized.contains(QStringLiteral("bangalore")))
            return QStringLiteral("Bengaluru");
        if (normalized.contains(QStringLiteral("mumbai")))
            return QStringLiteral("Mumbai");
        if (normalized.contains(QStringLiteral("bombay")))
            return QStringLiteral("Mumbai");
        if (normalized.contains(QStringLiteral("pune")))
            return QStringLiteral("Pune");
        if (normalized.contains(QStringLiteral("goa")))
            return QStringLiteral("Goa");
        if (normalized.contains(QStringLiteral("chennai")))
            return QStringLiteral("Chennai");
        if (normalized.contains(QStringLiteral("hyderabad")))
            return QStringLiteral("Hyderabad");
        if (normalized.contains(QStringLiteral("kolkata")))
            return QStringLiteral("Kolkata");
        if (normalized.contains(QStringLiteral("india")))
            return QStringLiteral("India");
        return QStringLiteral("Unknown Region");
    }

    bool isVerifiedAirbaseDestination(const AirbaseRegistry *registry,
                                     const QString &destination)
    {
        if (!registry)
            return false;

        const QString query = normalizeText(destination);
        const AirbaseRecord *match = nullptr;
        for (const AirbaseRecord &record : registry->records())
        {
            bool matchesRecord = normalizeText(record.name) == query ||
                                 normalizeText(record.id) == query;
            for (const QString &alias : record.aliases)
                matchesRecord = matchesRecord || normalizeText(alias) == query;
            if (!matchesRecord)
                continue;
            if (match)
                return false;
            match = &record;
        }

        return match && match->coordinateAvailable && match->coordinate.isValid() &&
               match->verificationStatus == QStringLiteral("verified_public_reference");
    }

    QString locationNameFromText(const QString &text)
    {
        const QString normalized = normalizeText(text);

        if (normalized.contains(QStringLiteral("near delhi")))
            return QStringLiteral("Delhi");
        if (normalized.contains(QStringLiteral("near bengaluru")) || normalized.contains(QStringLiteral("near bangalore")))
            return QStringLiteral("Bengaluru");
        if (normalized.contains(QStringLiteral("near mumbai")))
            return QStringLiteral("Mumbai");
        if (normalized.contains(QStringLiteral("near bombay")))
            return QStringLiteral("Mumbai");
        if (normalized.contains(QStringLiteral("near chennai")))
            return QStringLiteral("Chennai");
        if (normalized.contains(QStringLiteral("near hyderabad")))
            return QStringLiteral("Hyderabad");
        if (normalized.contains(QStringLiteral("near kolkata")))
            return QStringLiteral("Kolkata");

        const QString region = regionFromText(text);
        return region == QStringLiteral("Unknown Region") ? QString() : region;
    }

    bool extractEntitySpec(const QString &text, QString *type, int *count)
    {
        const QString normalized = normalizeText(text);
        const QRegularExpression matchPattern(
            QStringLiteral(R"((-?\d+|one|two|three|four|five|six|seven|eight|nine|ten)\s*(?:more\s+)?(?:aircraft|aircrafts|fighter|fighters|plane|planes|jet|jets|ship|ships|shipborne|shipborne\s+entities|radar\s+site|radar\s+sites|radar|ground\s+station|ground\s+stations))"));
        const QRegularExpressionMatch match = matchPattern.match(normalized);
        if (!match.hasMatch())
            return false;

        bool countOk = false;
        int parsedCount = match.captured(1).toInt(&countOk);
        if (!countOk)
        {
            static const QStringList numberWords = {
                QStringLiteral("one"), QStringLiteral("two"), QStringLiteral("three"),
                QStringLiteral("four"), QStringLiteral("five"), QStringLiteral("six"),
                QStringLiteral("seven"), QStringLiteral("eight"), QStringLiteral("nine"),
                QStringLiteral("ten")};
            const int wordIndex = numberWords.indexOf(match.captured(1));
            countOk = wordIndex >= 0;
            parsedCount = wordIndex + 1;
        }
        const QString entityText = match.captured(0).mid(
            match.captured(0).indexOf(QChar(' ')) + 1);

        *count = parsedCount;
        *type = canonicalTypeForText(entityText);
        return countOk && parsedCount > 0 && !type->isEmpty();
    }

    bool parseRouteLocations(const QString &normalized, QStringList *locations)
    {
        const QRegularExpression pattern(
            QStringLiteral(R"(^\s*(create|generate|show|display)\s+(?:me\s+)?(?:a\s+)?(?:flight\s+)?route\s+(?:from\s+|connecting\s+)(.+?)[.!]?$)"));
        const QRegularExpressionMatch match = pattern.match(normalized);
        if (!match.hasMatch())
            return false;
        QString routeText = match.captured(2).trimmed();
        routeText.replace(QRegularExpression(QStringLiteral("\\s+and\\s+")),
                          QStringLiteral(" to "));
        routeText.replace(QRegularExpression(QStringLiteral("\\s*,\\s*")),
                          QStringLiteral(" to "));
        const QStringList parts =
            routeText.split(QRegularExpression(QStringLiteral("\\s+to\\s+")),
                            Qt::SkipEmptyParts);
        QStringList result;
        for (const QString &part : parts) {
            const QString location = part.trimmed();
            if (location.isEmpty() ||
                (!result.isEmpty() &&
                 result.last().compare(location, Qt::CaseInsensitive) == 0))
                continue;
            result.push_back(location);
        }
        if (result.size() < 2)
            return false;
        *locations = result;
        return true;
    }
}

AIParser::AIParser(const AirbaseRegistry *airbaseRegistry)
    : m_airbaseRegistry(airbaseRegistry)
{
}

bool AIParser::parsePrompt(const QString &prompt, ScenarioRequest &request,
                           QString *errorMessage,
                           const QString &selectedAirbaseId) const
{
    const QString normalized = normalizeText(prompt);
    if (normalized.isEmpty())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("PLEASE ENTER A SCENARIO");
        return false;
    }

    request = {};
    request.explicitOperation = true;
    const QRegularExpression removeAllAircraftPattern(
        QStringLiteral(R"(^\s*(?:remove|delete|clear)\s+all\s+aircraft\s*[.!]?$)"));
    if (removeAllAircraftPattern.match(normalized).hasMatch())
    {
        request.operation = ScenarioOperation::RemoveAllAircraft;
        if (errorMessage)
            errorMessage->clear();
        return true;
    }

    const QRegularExpression removeLastAircraftPattern(
        QStringLiteral(R"(^\s*(?:remove|delete)\s+(?:the\s+)?last\s+(\d+|one|two|three|four|five|six|seven|eight|nine|ten)\s+aircraft\s*[.!]?$)"));
    const QRegularExpressionMatch removeLastAircraftMatch =
        removeLastAircraftPattern.match(normalized);
    if (removeLastAircraftMatch.hasMatch())
    {
        request.operation = ScenarioOperation::RemoveLastAircraft;
        bool countOk = false;
        request.entityCount = removeLastAircraftMatch.captured(1).toInt(&countOk);
        if (!countOk)
        {
            static const QStringList numberWords = {
                QStringLiteral("one"), QStringLiteral("two"), QStringLiteral("three"),
                QStringLiteral("four"), QStringLiteral("five"), QStringLiteral("six"),
                QStringLiteral("seven"), QStringLiteral("eight"), QStringLiteral("nine"),
                QStringLiteral("ten")};
            const int wordIndex =
                numberWords.indexOf(removeLastAircraftMatch.captured(1));
            countOk = wordIndex >= 0;
            request.entityCount = wordIndex + 1;
        }
        if (!countOk || request.entityCount < 1 ||
            request.entityCount > MaximumScenarioEntityCount)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("REMOVAL COUNT MUST BE BETWEEN 1 AND %1")
                                    .arg(MaximumScenarioEntityCount);
            return false;
        }
        if (errorMessage)
            errorMessage->clear();
        return true;
    }

    const QRegularExpression removeAircraftPattern(
        QStringLiteral(R"(^\s*(?:remove|delete)\s+(?:aircraft\s+)?([a-z]+\d+)\s*[.!]?$)"));
    const QRegularExpressionMatch removeAircraftMatch =
        removeAircraftPattern.match(normalized);
    if (removeAircraftMatch.hasMatch())
    {
        request.operation = ScenarioOperation::RemoveAircraft;
        request.operationTargetId = removeAircraftMatch.captured(1).toUpper();
        if (errorMessage)
            errorMessage->clear();
        return true;
    }

    if (normalized == QStringLiteral("reset scenario") || normalized == QStringLiteral("reset"))
    {
        request.operation = ScenarioOperation::Reset;
        if (errorMessage)
            errorMessage->clear();
        return true;
    }
    if (normalized == QStringLiteral("save scenario") || normalized == QStringLiteral("save"))
    {
        request.operation = ScenarioOperation::Save;
        if (errorMessage)
            errorMessage->clear();
        return true;
    }
    if (normalized == QStringLiteral("load scenario") || normalized == QStringLiteral("load"))
    {
        request.operation = ScenarioOperation::Load;
        if (errorMessage)
            errorMessage->clear();
        return true;
    }

    const bool formationIntent =
        (normalized.startsWith(QStringLiteral("create ")) ||
         normalized.startsWith(QStringLiteral("generate ")) ||
         normalized.startsWith(QStringLiteral("make ")) ||
         normalized.startsWith(QStringLiteral("add "))) &&
        (normalized.contains(QStringLiteral("formation")) ||
         normalized.contains(QStringLiteral("group")) ||
         normalized.contains(QStringLiteral("together")) ||
         normalized.contains(QStringLiteral("patrol")));
    if (formationIntent)
    {
        QString detectedType;
        int detectedCount = 0;
        if (!extractEntitySpec(normalized, &detectedType, &detectedCount) ||
            detectedType != QStringLiteral("aircraft"))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("SPECIFY A VALID AIRCRAFT COUNT FOR THE FORMATION");
            return false;
        }
        if (detectedCount < 2 || detectedCount > MaximumScenarioEntityCount)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("FORMATION COUNT MUST BE BETWEEN 2 AND %1")
                                    .arg(MaximumScenarioEntityCount);
            return false;
        }

        request.operation = ScenarioOperation::CreateFormation;
        request.entityType = QStringLiteral("aircraft");
        request.entityCount = detectedCount;
        EntityRequest aircraftRequest;
        aircraftRequest.type = QStringLiteral("aircraft");
        aircraftRequest.count = detectedCount;
        aircraftRequest.altitude = 28000.0;
        aircraftRequest.speed = 450.0;
        request.entities.push_back(aircraftRequest);
        request.entityTypes.push_back(QStringLiteral("aircraft"));
        request.formationType = QStringLiteral("V");
        const QRegularExpression formationTypePattern(
            QStringLiteral(R"(\b(?:in|as)\s+(?:a\s+)?([a-z]+(?:\s+[a-z]+)?)\s+formation\b)"));
        const QRegularExpressionMatch typeMatch = formationTypePattern.match(normalized);
        if (typeMatch.hasMatch())
        {
            request.formationType =
                FormationGenerator::canonicalType(typeMatch.captured(1));
            if (request.formationType.isEmpty())
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("UNSUPPORTED FORMATION TYPE");
                return false;
            }
        }

        const QRegularExpression spacingPattern(
            QStringLiteral(R"(\bspacing\s+(?:of\s+|to\s+)?(\d+(?:\.\d+)?)\s*(?:nm|nautical miles?)?\b)"));
        const QRegularExpressionMatch spacingMatch = spacingPattern.match(normalized);
        if (spacingMatch.hasMatch())
            request.formationSpacing = spacingMatch.captured(1).toDouble();
        if (!std::isfinite(request.formationSpacing) ||
            request.formationSpacing < 0.1 || request.formationSpacing > 100.0)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("FORMATION SPACING MUST BE BETWEEN 0.1 AND 100 NM");
            return false;
        }

        const QRegularExpression headingPattern(
            QStringLiteral(R"(\bheading\s+(-?\d+(?:\.\d+)?)\s*(?:degrees?)?\b)"));
        const QRegularExpressionMatch headingMatch = headingPattern.match(normalized);
        if (headingMatch.hasMatch())
            request.formationHeading = headingMatch.captured(1).toDouble();
        else
            request.formationHeading = request.heading;

        QString spawnText = normalized;
        spawnText.remove(QRegularExpression(
            QStringLiteral(R"(\b(?:move|fly|head|travel)\s+(?:them|the formation)?\s+towards?\s+.+[.!]?$)")));
        request.region = regionFromText(spawnText);
        if (request.region == QStringLiteral("Unknown Region") ||
            request.region == QStringLiteral("India"))
            request.region = QStringLiteral("India");
        request.locationName = request.region;
        request.altitude = 28000.0;
        request.speed = 450.0;
        if (errorMessage)
            errorMessage->clear();
        return true;
    }

    if (normalized == QStringLiteral("show scenario") || normalized == QStringLiteral("query scenario") ||
        normalized == QStringLiteral("describe scenario"))
    {
        request.operation = ScenarioOperation::Query;
        if (errorMessage)
            errorMessage->clear();
        return true;
    }

    if (normalized == QStringLiteral("show scheduled take-offs") ||
        normalized == QStringLiteral("show scheduled takeoffs"))
    {
        request.operation = ScenarioOperation::ListScheduledTakeoffs;
        if (errorMessage)
            errorMessage->clear();
        return true;
    }

    QStringList routeLocations;
    if (parseRouteLocations(normalized, &routeLocations))
    {
        request.operation =
            normalized.startsWith(QStringLiteral("show")) ||
                    normalized.startsWith(QStringLiteral("display"))
                ? ScenarioOperation::DisplayRoute
                : ScenarioOperation::CreateRoute;
        request.routeWaypoints = routeLocations;
        request.routeName = routeLocations.join(QStringLiteral(" to "));
        if (errorMessage)
            errorMessage->clear();
        return true;
    }

    const QRegularExpression schedulePattern(
        QStringLiteral(R"((?:schedule|take\s+off)\s+([a-z]+\d+)\s+(?:to\s+take\s+off\s+)?(?:from\s+(.+?)\s+)?at\s+(\d{1,2}):(\d{2})\s*(am|pm)?(?:\s+and\s+fly\s+to\s+(.+))?$)"));
    const QRegularExpressionMatch scheduleMatch = schedulePattern.match(normalized);
    if (scheduleMatch.hasMatch())
    {
        bool hourOk = false;
        bool minuteOk = false;
        const int parsedHour = scheduleMatch.captured(3).toInt(&hourOk);
        const int parsedMinute = scheduleMatch.captured(4).toInt(&minuteOk);
        const QString meridiem = scheduleMatch.captured(5);
        int hour = parsedHour;
        if (!hourOk || !minuteOk || parsedMinute > 59 ||
            (!meridiem.isEmpty() && (parsedHour < 1 || parsedHour > 12)) ||
            (meridiem.isEmpty() && parsedHour > 23))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("SPECIFY A VALID TIME IN HH:MM OR HH:MM AM/PM FORMAT");
            return false;
        }
        if (meridiem == QStringLiteral("pm") && hour != 12)
            hour += 12;
        else if (meridiem == QStringLiteral("am") && hour == 12)
            hour = 0;

        request.operation = ScenarioOperation::ScheduleTakeoff;
        request.operationTargetId = scheduleMatch.captured(1).toUpper();
        request.scheduledSeconds = hour * 3600 + parsedMinute * 60;
        request.originAirbaseName = scheduleMatch.captured(2).trimmed();
        request.destinationName = scheduleMatch.captured(6).trimmed();
        request.region = regionFromText(request.destinationName);
        if (errorMessage)
            errorMessage->clear();
        return true;
    }

    const QRegularExpression cancelSchedulePattern(
        QStringLiteral(R"(cancel\s+the\s+scheduled\s+take[- ]?off\s+for\s+([a-z]+\d+))"));
    const QRegularExpressionMatch cancelScheduleMatch = cancelSchedulePattern.match(normalized);
    if (cancelScheduleMatch.hasMatch())
    {
        request.operation = ScenarioOperation::CancelScheduledTakeoff;
        request.operationTargetId = cancelScheduleMatch.captured(1).toUpper();
        if (errorMessage)
            errorMessage->clear();
        return true;
    }

    const QRegularExpression stopAllAircraftPattern(
        QStringLiteral(R"(^\s*stop\s+all\s+aircraft\s*[.!]?$)"));
    if (stopAllAircraftPattern.match(normalized).hasMatch())
    {
        request.operation = ScenarioOperation::StopAllAircraft;
        if (errorMessage)
            errorMessage->clear();
        return true;
    }

    const QRegularExpression aircraftActionPattern(
        QStringLiteral(R"(^\s*(stop|pause|resume|start)\s+(?:aircraft\s+)?([a-z]+\d+)\s*[.!]?$)"));
    const QRegularExpressionMatch aircraftActionMatch =
        aircraftActionPattern.match(normalized);
    if (aircraftActionMatch.hasMatch())
    {
        const QString action = aircraftActionMatch.captured(1);
        request.operation = action == QStringLiteral("stop")
                                ? ScenarioOperation::StopAircraft
                                : action == QStringLiteral("pause")
                                      ? ScenarioOperation::PauseAircraft
                                      : action == QStringLiteral("start")
                                            ? ScenarioOperation::StartAircraft
                                            : ScenarioOperation::ResumeAircraft;
        request.operationTargetId = aircraftActionMatch.captured(2).toUpper();
        if (errorMessage)
            errorMessage->clear();
        return true;
    }

    const QRegularExpression formationActionPattern(
        QStringLiteral(R"(^\s*(stop|pause|resume|start|disband)\s+(?:(all)\s+)?(?:(?:the)\s+)?(?:(v|inverted v|line|column|diamond|wedge|echelon left|echelon right|circular)\s+)?formations?\s*[.!]?$)"));
    const QRegularExpressionMatch formationActionMatch =
        formationActionPattern.match(normalized);
    if (formationActionMatch.hasMatch())
    {
        const QString action = formationActionMatch.captured(1);
        request.operation = action == QStringLiteral("stop") ||
                                    action == QStringLiteral("pause")
                                ? ScenarioOperation::StopFormation
                                : action == QStringLiteral("disband")
                                      ? ScenarioOperation::DisbandFormation
                                      : ScenarioOperation::ResumeFormation;
        request.allFormations =
            !formationActionMatch.captured(2).isEmpty();
        if (!formationActionMatch.captured(3).isEmpty())
        {
            request.formationType =
                FormationGenerator::canonicalType(formationActionMatch.captured(3));
            request.formationTypeSpecified = true;
        }
        if (errorMessage)
            errorMessage->clear();
        return true;
    }

    const QRegularExpression formationMovementPattern(
        QStringLiteral(R"(^\s*move\s+(?:(all)\s+)?(?:(?:the)\s+)?(?:(v|inverted v|line|column|diamond|wedge|echelon left|echelon right|circular)\s+)?formations?\s+(?:to|towards?)\s+(.+?)[.!]?$)"));
    const QRegularExpressionMatch formationMovementMatch =
        formationMovementPattern.match(normalized);
    if (formationMovementMatch.hasMatch())
    {
        request.operation = ScenarioOperation::MoveFormation;
        request.allFormations =
            !formationMovementMatch.captured(1).isEmpty();
        if (!formationMovementMatch.captured(2).isEmpty())
        {
            request.formationType =
                FormationGenerator::canonicalType(formationMovementMatch.captured(2));
            request.formationTypeSpecified = true;
        }
        request.operationLocation = formationMovementMatch.captured(3).trimmed();
        request.region = regionFromText(request.operationLocation);
        if (request.operationLocation.isEmpty() ||
            (request.region == QStringLiteral("Unknown Region") &&
             !isVerifiedAirbaseDestination(m_airbaseRegistry,
                                          request.operationLocation)))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("SPECIFY A SUPPORTED DESTINATION");
            return false;
        }
        if (errorMessage)
            errorMessage->clear();
        return true;
    }

    const QRegularExpression aircraftRoutePattern(
        QStringLiteral(R"(^\s*(assign|move)\s+(?:the\s+)?route\s+from\s+(.+?)\s+to\s+aircraft\s+([a-z]+\d+)\s*$)"));
    const QRegularExpressionMatch aircraftRouteMatch =
        aircraftRoutePattern.match(normalized);
    if (aircraftRouteMatch.hasMatch())
    {
        request.operation =
            aircraftRouteMatch.captured(1) == QStringLiteral("assign")
                ? ScenarioOperation::AssignRoute
                : ScenarioOperation::MoveAlongRoute;
        request.routeAircraftId = aircraftRouteMatch.captured(3).toUpper();
        request.routeWaypoints =
            aircraftRouteMatch.captured(2).split(
                QRegularExpression(QStringLiteral("\\s+to\\s+")),
                Qt::SkipEmptyParts);
        if (request.routeWaypoints.size() < 2)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("SPECIFY AT LEAST AN ORIGIN AND DESTINATION");
            return false;
        }
        request.routeName = request.routeWaypoints.join(QStringLiteral(" to "));
        if (errorMessage)
            errorMessage->clear();
        return true;
    }
    const QRegularExpression alternateAircraftRoutePattern(
        QStringLiteral(R"(^\s*(assign|move)\s+(?:the\s+)?(.+?)\s+route\s+to\s+aircraft\s+([a-z]+\d+)\s*$)"));
    const QRegularExpressionMatch alternateAircraftRouteMatch =
        alternateAircraftRoutePattern.match(normalized);
    if (alternateAircraftRouteMatch.hasMatch())
    {
        request.operation =
            alternateAircraftRouteMatch.captured(1) == QStringLiteral("assign")
                ? ScenarioOperation::AssignRoute
                : ScenarioOperation::MoveAlongRoute;
        request.routeAircraftId = alternateAircraftRouteMatch.captured(3).toUpper();
        request.routeWaypoints =
            alternateAircraftRouteMatch.captured(2).split(
                QRegularExpression(QStringLiteral("\\s+to\\s+")),
                Qt::SkipEmptyParts);
        if (request.routeWaypoints.size() < 2)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("SPECIFY AT LEAST AN ORIGIN AND DESTINATION");
            return false;
        }
        request.routeName = request.routeWaypoints.join(QStringLiteral(" to "));
        if (errorMessage)
            errorMessage->clear();
        return true;
    }
    const QRegularExpression moveAlongRoutePattern(
        QStringLiteral(R"(^\s*move\s+(?:aircraft\s+)?([a-z]+\d+)\s+along\s+(?:the\s+)?route\s+from\s+(.+?)\s*$)"));
    const QRegularExpressionMatch moveAlongRouteMatch =
        moveAlongRoutePattern.match(normalized);
    if (moveAlongRouteMatch.hasMatch())
    {
        request.operation = ScenarioOperation::MoveAlongRoute;
        request.routeAircraftId = moveAlongRouteMatch.captured(1).toUpper();
        request.routeWaypoints =
            moveAlongRouteMatch.captured(2).split(
                QRegularExpression(QStringLiteral("\\s+to\\s+")),
                Qt::SkipEmptyParts);
        if (request.routeWaypoints.size() < 2)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("SPECIFY AT LEAST AN ORIGIN AND DESTINATION");
            return false;
        }
        request.routeName = request.routeWaypoints.join(QStringLiteral(" to "));
        if (errorMessage)
            errorMessage->clear();
        return true;
    }

    const QRegularExpression movementPattern(
        QStringLiteral(R"(^\s*(?:move|send|route)\s+(?:aircraft\s+)?([a-z]+\d+)(?:\s+from\s+(?:its|it's|the)\s+(?:current\s+)?(?:position|location))?\s+(?:to|towards?)\s+(.+?)[.!]?$)"));
    const QRegularExpressionMatch movementMatch = movementPattern.match(normalized);
    const QRegularExpression destinationChangePattern(
        QStringLiteral(R"(^\s*change\s+(?:route\s+for\s+)?(?:aircraft\s+)?([a-z]+\d+)(?:'s)?(?:\s+(?:destination|route))?\s+to\s+(.+?)[.!]?$)"));
    const QRegularExpressionMatch destinationChangeMatch =
        destinationChangePattern.match(normalized);
    if (movementMatch.hasMatch() || destinationChangeMatch.hasMatch())
    {
        const QRegularExpressionMatch &targetMatch =
            movementMatch.hasMatch() ? movementMatch : destinationChangeMatch;
        request.operation = movementMatch.hasMatch()
                                ? ScenarioOperation::Move
                                : ScenarioOperation::ChangeRoute;
        request.operationTargetId = targetMatch.captured(1).toUpper();
        request.operationLocation = targetMatch.captured(2).trimmed();
        request.region = regionFromText(request.operationLocation);
        if (request.operationLocation.isEmpty() ||
            (request.region == QStringLiteral("Unknown Region") &&
             !isVerifiedAirbaseDestination(m_airbaseRegistry,
                                          request.operationLocation)))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("SPECIFY A SUPPORTED DESTINATION");
            return false;
        }
        if (errorMessage)
            errorMessage->clear();
        return true;
    }

    if (normalized.startsWith(QStringLiteral("move ")) ||
        normalized.startsWith(QStringLiteral("send ")) ||
        normalized.startsWith(QStringLiteral("route ")) ||
        normalized.startsWith(QStringLiteral("change ")) ||
        normalized.startsWith(QStringLiteral("stop ")) ||
        normalized.startsWith(QStringLiteral("pause ")) ||
        normalized.startsWith(QStringLiteral("resume ")) ||
        normalized.startsWith(QStringLiteral("start ")))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("USE AN AIRCRAFT ID AND A SUPPORTED DESTINATION OR ACTION");
        return false;
    }

    if (normalized.startsWith(QStringLiteral("remove ")) ||
        normalized.startsWith(QStringLiteral("delete ")) ||
        normalized.startsWith(QStringLiteral("clear ")) ||
        normalized.startsWith(QStringLiteral("disband ")))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("SPECIFY AIRCRAFT, AN AIRCRAFT ID, OR A FORMATION");
        return false;
    }
    const bool isAddOperation = normalized.startsWith(QStringLiteral("add "));
    const bool isCreateOperation = normalized.startsWith(QStringLiteral("create "));
    const bool isGenerateOperation = normalized.startsWith(QStringLiteral("generate "));
    const bool isMakeOperation = normalized.startsWith(QStringLiteral("make "));
    const bool isReplaceOperation = normalized.startsWith(QStringLiteral("replace "));
    request.operation = isReplaceOperation ? ScenarioOperation::Replace
                                           : ScenarioOperation::Add;
    request.explicitOperation = isAddOperation || isCreateOperation ||
                                isGenerateOperation || isMakeOperation || isReplaceOperation;
    if (normalized.startsWith(QStringLiteral("show airbases")))
    {
        request.airbaseRegion = regionFromText(normalized);
        if (request.airbaseRegion == QStringLiteral("Unknown Region"))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("SPECIFY A CONFIGURED REGION FOR AIRBASES");
            return false;
        }
        request.airbaseDisplayOnly = true;
        if (errorMessage)
            errorMessage->clear();
        return true;
    }

    const bool targetsEachAirbase =
        QRegularExpression(QStringLiteral("\\bat each (?:configured )?airbase in\\b"))
            .match(normalized)
            .hasMatch();
    const bool targetsSelectedAirbase = normalized.contains(QStringLiteral("at the selected airbase"));
    if (targetsEachAirbase)
    {
        request.airbaseRegion = regionFromText(normalized);
        request.airbaseRegionTarget = request.airbaseRegion != QStringLiteral("Unknown Region");
        if (!request.airbaseRegionTarget)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("SPECIFY A CONFIGURED REGION FOR AIRBASES");
            return false;
        }
    }
    else if (targetsSelectedAirbase)
    {
        if (!m_airbaseRegistry || selectedAirbaseId.isEmpty())
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("SELECT AN AIRBASE BEFORE GENERATING ENTITIES");
            return false;
        }
        const AirbaseRecord *record = m_airbaseRegistry->byId(selectedAirbaseId);
        if (!record || !record->coordinateAvailable ||
            record->verificationStatus != QStringLiteral("verified_public_reference"))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("THE SELECTED AIRBASE IS NOT VERIFIED");
            return false;
        }
        request.airbaseId = record->id;
        request.airbaseName = record->name;
        request.airbaseRegion = record->region;
    }
    else
    {
        const QRegularExpression atPattern(QStringLiteral("\\bat\\s+(.+)$"));
        const QRegularExpressionMatch atMatch = atPattern.match(normalized);
        if (atMatch.hasMatch() && m_airbaseRegistry)
        {
            const QString airbaseQuery = atMatch.captured(1).trimmed();
            const QVector<const AirbaseRecord *> matches = m_airbaseRegistry->matchingText(airbaseQuery);
            if (matches.size() > 1)
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("AIRBASE NAME IS AMBIGUOUS: %1").arg(airbaseQuery);
                return false;
            }
            if (matches.size() == 1)
            {
                if (!matches.first()->coordinateAvailable ||
                    matches.first()->verificationStatus != QStringLiteral("verified_public_reference"))
                {
                    if (errorMessage)
                        *errorMessage = QStringLiteral("AIRBASE LOCATION IS NOT VERIFIED: %1")
                                            .arg(airbaseQuery);
                    return false;
                }
                request.airbaseId = matches.first()->id;
                request.airbaseName = matches.first()->name;
                request.airbaseRegion = matches.first()->region;
            }
            else
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("AIRBASE IS NOT CONFIGURED: %1").arg(airbaseQuery);
                return false;
            }
        }
    }
    request.region = regionFromText(normalized);
    request.locationName = locationNameFromText(normalized);
    request.hasCustomLocation = !request.locationName.isEmpty() && request.locationName != request.region;

    QString entityPrompt = normalized;
    QVector<QPair<int, int>> routeClauseRanges;
    const QRegularExpression routeFromPattern(
        QStringLiteral(R"(\b(?:route|send|direct|assign|make|have)\s+(?:one|1|another|the other|first|second)(?:\s+(?:aircraft|plane))?\s+from\s+[a-z]+(?:\s+[a-z]+)?\s+to\s+([a-z]+)\b)"));
    QRegularExpressionMatchIterator routeFromMatches = routeFromPattern.globalMatch(normalized);
    while (routeFromMatches.hasNext())
    {
        const QRegularExpressionMatch routeMatch = routeFromMatches.next();
        const QString destination = regionFromText(routeMatch.captured(1));
        if (destination == QStringLiteral("Unknown Region") ||
            destination == QStringLiteral("India"))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("SPECIFY A SUPPORTED ROUTE DESTINATION");
            return false;
        }
        request.routeDestinations.push_back(destination);
        routeClauseRanges.push_back({routeMatch.capturedStart(), routeMatch.capturedLength()});
    }

    const QRegularExpression routePattern(
        QStringLiteral(R"(\b(?:send|route|direct|assign|make|have|with)\s+(?:one|1|another|the other|first|second)(?:\s+(?:aircraft|plane))?(?:\s+(?:fly|flying|head|headed|travel|travelling|routed))?\s+(?:to|towards?)\s+([a-z]+)\b)"));
    QRegularExpressionMatchIterator routeMatches = routePattern.globalMatch(normalized);
    while (routeMatches.hasNext())
    {
        const QRegularExpressionMatch routeMatch = routeMatches.next();
        const QString destination = regionFromText(routeMatch.captured(1));
        if (destination == QStringLiteral("Unknown Region") ||
            destination == QStringLiteral("India") ||
            destination == QStringLiteral("North India") ||
            destination == QStringLiteral("South India") ||
            destination == QStringLiteral("East India") ||
            destination == QStringLiteral("West India"))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("SPECIFY A SUPPORTED ROUTE DESTINATION");
            return false;
        }
        request.routeDestinations.push_back(destination);
        routeClauseRanges.push_back({routeMatch.capturedStart(), routeMatch.capturedLength()});
    }
    for (auto iterator = routeClauseRanges.crbegin(); iterator != routeClauseRanges.crend(); ++iterator)
        entityPrompt.remove(iterator->first, iterator->second);

    request.altitude = 28000.0;
    request.speed = 450.0;
    if (normalized.contains(QStringLiteral("northbound")) ||
        normalized.contains(QStringLiteral("heading north")) ||
        normalized.contains(QStringLiteral("traveling north")))
        request.heading = 0.0;
    else if (normalized.contains(QStringLiteral("eastbound")) ||
             normalized.contains(QStringLiteral("heading east")) ||
             normalized.contains(QStringLiteral("traveling east")))
        request.heading = 90.0;
    else if (normalized.contains(QStringLiteral("southbound")) ||
             normalized.contains(QStringLiteral("heading south")) ||
             normalized.contains(QStringLiteral("traveling south")))
        request.heading = 180.0;
    else if (normalized.contains(QStringLiteral("westbound")) ||
             normalized.contains(QStringLiteral("heading west")) ||
             normalized.contains(QStringLiteral("traveling west")))
        request.heading = 270.0;

    const QRegularExpression altitudePattern(
        QStringLiteral(R"((\d{1,2}(?:,\d{3})?|\d{4,5})\s*(?:ft|feet|foot)\b)"));
    const QRegularExpressionMatch altitudeMatch = altitudePattern.match(normalized);
    if (altitudeMatch.hasMatch())
        request.altitude = altitudeMatch.captured(1).remove(QChar(',')).toDouble();

    const QRegularExpression speedPattern(
        QStringLiteral(R"((\d{2,3})\s*(?:kt|kts|knots)\b)"));
    const QRegularExpressionMatch speedMatch = speedPattern.match(normalized);
    if (speedMatch.hasMatch())
        request.speed = speedMatch.captured(1).toDouble();

    if (normalized.contains(QStringLiteral("crossing")))
        request.routePattern = QStringLiteral("crossing");
    else if (normalized.contains(QStringLiteral("parallel")))
        request.routePattern = QStringLiteral("parallel");
    else if (normalized.contains(QStringLiteral("holding")) ||
             normalized.contains(QStringLiteral("circular")) ||
             normalized.contains(QStringLiteral("circle")))
        request.routePattern = QStringLiteral("holding");
    else if (normalized.contains(QStringLiteral("arrival")))
        request.routePattern = QStringLiteral("arrival");
    else if (normalized.contains(QStringLiteral("departure")))
        request.routePattern = QStringLiteral("departure");
    else if (normalized.contains(QStringLiteral("straight")))
        request.routePattern = QStringLiteral("straight");
    request.entityTypes.clear();
    request.entities.clear();
    request.entityType.clear();

    if (normalized.contains(QStringLiteral("500 km")) || normalized.contains(QStringLiteral("500km")))
    {
        request.radiusKm = 500.0;
        request.hasRadius = true;
    }
    else if (QRegularExpression(QStringLiteral("(\\d+)\\s*km")).match(normalized).hasMatch())
    {
        const QRegularExpression radiusPattern(QStringLiteral("(\\d+)\\s*km"));
        const auto match = radiusPattern.match(normalized);
        request.radiusKm = match.hasMatch() ? match.captured(1).toDouble() : 0.0;
        request.hasRadius = request.radiusKm > 0.0;
    }

    const QRegularExpression entityPattern(
        QStringLiteral(R"((-?\d+|one|two|three|four|five|six|seven|eight|nine|ten)\s*(?:more\s+)?(?:aircraft|aircrafts|fighter|fighters|plane|planes|jet|jets|ship|ships|shipborne|shipborne\s+entities|radar\s+site|radar\s+sites|radar|ground\s+station|ground\s+stations))"));
    QRegularExpressionMatchIterator matches = entityPattern.globalMatch(entityPrompt);

    int totalEntityCount = 0;
    if (matches.hasNext())
    {
        while (matches.hasNext())
        {
            const QRegularExpressionMatch match = matches.next();
            const QString captured = match.captured(0);
            bool countOk = false;
            int count = match.captured(1).toInt(&countOk);
            if (!countOk)
            {
                static const QStringList numberWords = {
                    QStringLiteral("one"), QStringLiteral("two"), QStringLiteral("three"),
                    QStringLiteral("four"), QStringLiteral("five"), QStringLiteral("six"),
                    QStringLiteral("seven"), QStringLiteral("eight"), QStringLiteral("nine"),
                    QStringLiteral("ten")};
                const int wordIndex = numberWords.indexOf(match.captured(1));
                countOk = wordIndex >= 0;
                count = wordIndex + 1;
            }
            if (!countOk || count <= 0)
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("AIRCRAFT COUNT MUST BE A POSITIVE INTEGER");
                return false;
            }
            if (count > MaximumScenarioEntityCount ||
                totalEntityCount > MaximumScenarioEntityCount - count)
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("SCENARIO ENTITY COUNT MUST NOT EXCEED %1")
                                        .arg(MaximumScenarioEntityCount);
                return false;
            }
            const QString typeText = captured.mid(captured.indexOf(QChar(' ')) + 1);
            const QString type = canonicalTypeForText(typeText);
            if (type.isEmpty())
                continue;

            EntityRequest item;
            item.type = type;
            item.count = count;
            item.altitude = type == QStringLiteral("aircraft") ? request.altitude : 10000.0;
            item.speed = type == QStringLiteral("aircraft") ? request.speed : type == QStringLiteral("ship") ? 300.0
                                                                                                             : 0.0;
            item.heading = type == QStringLiteral("aircraft") ? request.heading : 90.0;
            request.entities.push_back(item);
            request.entityTypes << type;
            totalEntityCount += count;
        }
    }

    if (request.entities.isEmpty())
    {
        QString detectedType;
        int detectedCount = 1;
        if (!extractEntitySpec(normalized, &detectedType, &detectedCount))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("SPECIFY AIRCRAFT, SHIP, RADAR SITE, OR GROUND STATION");
            return false;
        }

        EntityRequest item;
        item.type = detectedType;
        item.count = detectedCount;
        item.altitude = detectedType == QStringLiteral("aircraft") ? request.altitude : 10000.0;
        item.speed = detectedType == QStringLiteral("aircraft") ? request.speed : detectedType == QStringLiteral("ship") ? 300.0
                                                                                                                         : 0.0;
        item.heading = detectedType == QStringLiteral("aircraft") ? request.heading : 90.0;
        request.entities.push_back(item);
        request.entityTypes << detectedType;
        totalEntityCount = detectedCount;
    }

    request.entityType = request.entityTypes.isEmpty() ? QString() : request.entityTypes.first();
    if (request.entityType.isEmpty())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("SPECIFY AIRCRAFT, SHIP, RADAR SITE, OR GROUND STATION");
        return false;
    }

    request.entityCount = request.entities.first().count;
    if (request.entities.size() == 1)
    {
        request.entityCount = request.entities.first().count;
    }
    if (request.routeDestinations.size() > request.entityCount)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("ROUTED AIRCRAFT COUNT CANNOT EXCEED SCENARIO COUNT");
        return false;
    }

    if (request.region == QStringLiteral("Unknown Region") && request.locationName.isEmpty() &&
        request.airbaseId.isEmpty() && !request.airbaseRegionTarget)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("SUPPORTED REGIONS: NORTH INDIA, SOUTH INDIA, EAST INDIA, WEST INDIA, DELHI, BENGALURU");
        return false;
    }

    if (request.region == QStringLiteral("Unknown Region") && !request.locationName.isEmpty())
        request.region = request.locationName;

    if (!request.locationName.isEmpty() && request.locationName != request.region)
        request.hasCustomLocation = true;

    if (errorMessage)
        errorMessage->clear();
    return true;
}
