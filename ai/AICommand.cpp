#include "AICommand.h"

#include "../core/FormationGenerator.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStringList>
#include <QtMath>

namespace
{
    bool requiredString(const QJsonObject &object, const QString &name,
                        QString &value, QString *errorMessage)
    {
        const QJsonValue jsonValue = object.value(name);
        if (!jsonValue.isString() || jsonValue.toString().trimmed().isEmpty())
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("MODEL COMMAND FIELD '%1' MUST BE A NON-EMPTY STRING")
                                    .arg(name);
            return false;
        }
        value = jsonValue.toString().trimmed();
        return true;
    }

    bool requiredDestination(const QJsonObject &object, QString &value,
                             QString *errorMessage)
    {
        for (const QString &field : {QStringLiteral("destination"),
                                     QStringLiteral("location"),
                                     QStringLiteral("region")})
            if (object.contains(field))
                return requiredString(object, field, value, errorMessage);
        if (errorMessage)
            *errorMessage =
                QStringLiteral("MODEL COMMAND FIELD 'destination' MUST BE A NON-EMPTY STRING");
        return false;
    }

    bool validEntityId(const QString &id)
    {
        return QRegularExpression(QStringLiteral("^[A-Za-z]+\\d+$")).match(id).hasMatch();
    }

    bool validOperation(const QString &operation)
    {
        return operation == QStringLiteral("ADD_ENTITIES") ||
               operation == QStringLiteral("MOVE_ENTITY") ||
               operation == QStringLiteral("SET_ROUTE") ||
               operation == QStringLiteral("STOP_AIRCRAFT") ||
               operation == QStringLiteral("STOP_ALL_AIRCRAFT") ||
               operation == QStringLiteral("PAUSE_AIRCRAFT") ||
               operation == QStringLiteral("RESUME_AIRCRAFT") ||
               operation == QStringLiteral("START_AIRCRAFT") ||
               operation == QStringLiteral("REMOVE_AIRCRAFT") ||
               operation == QStringLiteral("REMOVE_LAST_AIRCRAFT") ||
               operation == QStringLiteral("REMOVE_ALL_AIRCRAFT") ||
               operation == QStringLiteral("SCHEDULE_TAKEOFF") ||
               operation == QStringLiteral("CANCEL_EVENT") ||
               operation == QStringLiteral("SHOW_AIRBASES") ||
               operation == QStringLiteral("CREATE_FORMATION") ||
               operation == QStringLiteral("MOVE_FORMATION") ||
               operation == QStringLiteral("CREATE_ROUTE") ||
               operation == QStringLiteral("DISPLAY_ROUTE") ||
               operation == QStringLiteral("ASSIGN_ROUTE") ||
               operation == QStringLiteral("MOVE_ALONG_ROUTE") ||
               operation == QStringLiteral("STOP_FORMATION") ||
               operation == QStringLiteral("RESUME_FORMATION") ||
               operation == QStringLiteral("DISBAND_FORMATION") ||
               operation == QStringLiteral("QUERY_SCENARIO");
    }

    bool parseFormationSelection(const QJsonObject &object,
                                 ScenarioRequest &request,
                                 QString *errorMessage)
    {
        if (object.contains(QStringLiteral("formation_type")))
        {
            QString type;
            if (!requiredString(object, QStringLiteral("formation_type"), type,
                                errorMessage))
                return false;
            request.formationType = FormationGenerator::canonicalType(type);
            if (request.formationType.isEmpty())
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("MODEL FORMATION TYPE IS NOT SUPPORTED");
                return false;
            }
            request.formationTypeSpecified = true;
        }
        if (object.contains(QStringLiteral("all_formations")))
        {
            const QJsonValue allFormations = object.value(QStringLiteral("all_formations"));
            if (!allFormations.isBool())
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("MODEL ALL_FORMATIONS MUST BE A BOOLEAN");
                return false;
            }
            request.allFormations = allFormations.toBool();
        }
        return true;
    }

    bool isRegionLocation(const QString &locationName)
    {
        static const QStringList regionsAndCities = {
            QStringLiteral("India"), QStringLiteral("North India"),
            QStringLiteral("South India"), QStringLiteral("East India"),
            QStringLiteral("West India"), QStringLiteral("Delhi"),
            QStringLiteral("Bengaluru"), QStringLiteral("Mumbai"),
            QStringLiteral("Bombay"), QStringLiteral("Pune"),
            QStringLiteral("Goa"), QStringLiteral("Chennai"),
            QStringLiteral("Hyderabad"), QStringLiteral("Kolkata")};
        return regionsAndCities.contains(locationName.trimmed(), Qt::CaseInsensitive);
    }
}

bool AICommand::fromJson(const QJsonObject &object, ScenarioRequest &request,
                         QString *errorMessage)
{
    request = {};
    const QString operation = object.value(QStringLiteral("operation")).toString().trimmed();
    if (!validOperation(operation))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("UNSUPPORTED MODEL OPERATION");
        return false;
    }

    request.explicitOperation = true;
    if (operation == QStringLiteral("QUERY_SCENARIO"))
    {
        request.operation = ScenarioOperation::Query;
        return true;
    }

    if (operation == QStringLiteral("SHOW_AIRBASES"))
    {
        request.operation = ScenarioOperation::Query;
        request.airbaseDisplayOnly = true;
        if (!requiredString(object, QStringLiteral("region"), request.airbaseRegion, errorMessage))
            return false;
        return true;
    }

    if (operation == QStringLiteral("REMOVE_ALL_AIRCRAFT"))
    {
        request.operation = ScenarioOperation::RemoveAllAircraft;
        return true;
    }
    if (operation == QStringLiteral("REMOVE_LAST_AIRCRAFT"))
    {
        request.operation = ScenarioOperation::RemoveLastAircraft;
        const QString countField = object.contains(QStringLiteral("count"))
                                       ? QStringLiteral("count")
                                       : QStringLiteral("last_n");
        const QJsonValue count = object.value(countField);
        if (!count.isDouble() || count.toDouble() != qFloor(count.toDouble()))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("MODEL REMOVAL COUNT MUST BE AN INTEGER");
            return false;
        }
        request.entityCount = count.toInt();
        if (request.entityCount < 1 ||
            request.entityCount > MaximumScenarioEntityCount)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("MODEL REMOVAL COUNT MUST BE BETWEEN 1 AND %1")
                                    .arg(MaximumScenarioEntityCount);
            return false;
        }
        return true;
    }
    if (operation == QStringLiteral("REMOVE_AIRCRAFT"))
    {
        request.operation = ScenarioOperation::RemoveAircraft;
        if (!requiredString(object, QStringLiteral("entity_id"),
                            request.operationTargetId, errorMessage) ||
            !validEntityId(request.operationTargetId))
        {
            if (errorMessage && errorMessage->isEmpty())
                *errorMessage = QStringLiteral("MODEL ENTITY_ID IS INVALID");
            return false;
        }
        request.operationTargetId = request.operationTargetId.toUpper();
        return true;
    }
    if (operation == QStringLiteral("STOP_ALL_AIRCRAFT"))
    {
        request.operation = ScenarioOperation::StopAllAircraft;
        return true;
    }

    if (operation == QStringLiteral("MOVE_FORMATION") ||
        operation == QStringLiteral("STOP_FORMATION") ||
        operation == QStringLiteral("RESUME_FORMATION") ||
        operation == QStringLiteral("DISBAND_FORMATION"))
    {
        request.operation =
            operation == QStringLiteral("MOVE_FORMATION")
                ? ScenarioOperation::MoveFormation
                : operation == QStringLiteral("STOP_FORMATION")
                      ? ScenarioOperation::StopFormation
                      : operation == QStringLiteral("RESUME_FORMATION")
                            ? ScenarioOperation::ResumeFormation
                            : ScenarioOperation::DisbandFormation;
        if (!parseFormationSelection(object, request, errorMessage))
            return false;
        return request.operation != ScenarioOperation::MoveFormation ||
               requiredDestination(object, request.operationLocation, errorMessage);
    }

    if (operation == QStringLiteral("ADD_ENTITIES") &&
        !object.contains(QStringLiteral("formation_type")))
    {
        request.operation = ScenarioOperation::Add;
        request.entityType = object.value(QStringLiteral("entity_type")).toString(QStringLiteral("aircraft")).trimmed().toLower();
        if (request.entityType != QStringLiteral("aircraft"))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("ONLY AIRCRAFT ENTITIES ARE SUPPORTED");
            return false;
        }
        const QJsonValue countValue = object.value(QStringLiteral("count"));
        if (!countValue.isDouble() || countValue.toDouble() != qFloor(countValue.toDouble()))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("MODEL COUNT MUST BE AN INTEGER");
            return false;
        }
        request.entityCount = countValue.toInt();
        if (request.entityCount < 1 || request.entityCount > MaximumScenarioEntityCount)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("MODEL COUNT MUST BE BETWEEN 1 AND %1")
                                    .arg(MaximumScenarioEntityCount);
            return false;
        }
        if (object.contains(QStringLiteral("region")))
        {
            if (!requiredString(object, QStringLiteral("region"),
                                request.locationName, errorMessage))
                return false;
        }
        else if (object.contains(QStringLiteral("location")))
        {
            if (!requiredString(object, QStringLiteral("location"),
                                request.locationName, errorMessage))
                return false;
        }
        else if (!requiredString(object, QStringLiteral("destination"),
                                 request.locationName, errorMessage))
        {
            return false;
        }
        if (object.contains(QStringLiteral("route_destinations")))
        {
            const QJsonValue routeDestinationsValue =
                object.value(QStringLiteral("route_destinations"));
            if (!routeDestinationsValue.isArray())
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("MODEL ROUTE DESTINATIONS MUST BE AN ARRAY");
                return false;
            }
            for (const QJsonValue &destination : routeDestinationsValue.toArray())
            {
                if (!destination.isString() || destination.toString().trimmed().isEmpty())
                {
                    if (errorMessage)
                        *errorMessage = QStringLiteral("MODEL ROUTE DESTINATIONS MUST BE NON-EMPTY STRINGS");
                    return false;
                }
                request.routeDestinations.push_back(destination.toString().trimmed());
            }
        }
        else if (object.contains(QStringLiteral("route_destination")))
        {
            QString destination;
            if (!requiredString(object, QStringLiteral("route_destination"),
                                destination, errorMessage))
                return false;
            request.routeDestinations.push_back(destination);
        }
        if (request.routeDestinations.size() > request.entityCount)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("MODEL ROUTE COUNT EXCEEDS MODEL ENTITY COUNT");
            return false;
        }
        return true;
    }

    if (operation == QStringLiteral("CREATE_FORMATION") ||
        (operation == QStringLiteral("ADD_ENTITIES") &&
         object.contains(QStringLiteral("formation_type"))))
    {
        request.operation = ScenarioOperation::CreateFormation;
        request.entityType =
            object.value(QStringLiteral("entity_type")).toString(QStringLiteral("aircraft"))
                .trimmed()
                .toLower();
        if (request.entityType != QStringLiteral("aircraft"))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("ONLY AIRCRAFT FORMATIONS ARE SUPPORTED");
            return false;
        }
        const QJsonValue countValue = object.value(QStringLiteral("count"));
        if (!countValue.isDouble() || countValue.toDouble() != qFloor(countValue.toDouble()))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("MODEL COUNT MUST BE AN INTEGER");
            return false;
        }
        request.entityCount = countValue.toInt();
        if (request.entityCount < 2 || request.entityCount > MaximumScenarioEntityCount)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("MODEL FORMATION COUNT MUST BE BETWEEN 2 AND %1")
                                    .arg(MaximumScenarioEntityCount);
            return false;
        }
        QString formationType;
        if (!requiredString(object, QStringLiteral("formation_type"), formationType,
                            errorMessage))
            return false;
        request.formationType = FormationGenerator::canonicalType(formationType);
        if (request.formationType.isEmpty())
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("MODEL FORMATION TYPE IS NOT SUPPORTED");
            return false;
        }
        return true;
    }

    if (operation == QStringLiteral("CREATE_ROUTE") ||
        operation == QStringLiteral("DISPLAY_ROUTE") ||
        operation == QStringLiteral("ASSIGN_ROUTE") ||
        operation == QStringLiteral("MOVE_ALONG_ROUTE"))
    {
        request.operation =
            operation == QStringLiteral("CREATE_ROUTE")
                ? ScenarioOperation::CreateRoute
                : operation == QStringLiteral("DISPLAY_ROUTE")
                      ? ScenarioOperation::DisplayRoute
                      : operation == QStringLiteral("ASSIGN_ROUTE")
                            ? ScenarioOperation::AssignRoute
                            : ScenarioOperation::MoveAlongRoute;
        if (request.operation == ScenarioOperation::AssignRoute ||
            request.operation == ScenarioOperation::MoveAlongRoute)
        {
            if (!requiredString(object, QStringLiteral("aircraft_id"),
                                request.routeAircraftId, errorMessage) &&
                !requiredString(object, QStringLiteral("entity_id"),
                                request.routeAircraftId, errorMessage))
                return false;
            request.routeAircraftId = request.routeAircraftId.toUpper();
        }
        request.routeName = object.value(QStringLiteral("route_name"))
                                .toString()
                                .trimmed();
        const QJsonValue waypointValue = object.contains(QStringLiteral("waypoints"))
                                             ? object.value(QStringLiteral("waypoints"))
                                             : object.value(QStringLiteral("route"));
        if (!waypointValue.isArray())
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("MODEL ROUTE WAYPOINTS MUST BE AN ARRAY");
            return false;
        }
        for (const QJsonValue &value : waypointValue.toArray())
        {
            const QString name = value.isString()
                                     ? value.toString().trimmed()
                                     : value.toObject().value(QStringLiteral("name"))
                                           .toString()
                                           .trimmed();
            if (name.isEmpty())
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("MODEL ROUTE WAYPOINT NAMES MUST NOT BE EMPTY");
                return false;
            }
            request.routeWaypoints.push_back(name);
        }
        if (request.routeWaypoints.size() < 2)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("MODEL ROUTE REQUIRES AT LEAST TWO WAYPOINTS");
            return false;
        }
        if (request.routeName.isEmpty())
            request.routeName = request.routeWaypoints.join(QStringLiteral(" to "));
        return true;
    }

    if (!requiredString(object, QStringLiteral("entity_id"), request.operationTargetId,
                        errorMessage) ||
        !validEntityId(request.operationTargetId))
    {
        if (errorMessage && errorMessage->isEmpty())
            *errorMessage = QStringLiteral("MODEL ENTITY_ID IS INVALID");
        return false;
    }
    request.operationTargetId = request.operationTargetId.toUpper();

    if (operation == QStringLiteral("MOVE_ENTITY") ||
        operation == QStringLiteral("SET_ROUTE"))
    {
        request.operation = operation == QStringLiteral("MOVE_ENTITY")
                                ? ScenarioOperation::Move
                                : ScenarioOperation::ChangeRoute;
        return requiredDestination(object, request.operationLocation, errorMessage);
    }

    if (operation == QStringLiteral("STOP_AIRCRAFT") ||
        operation == QStringLiteral("PAUSE_AIRCRAFT") ||
        operation == QStringLiteral("RESUME_AIRCRAFT") ||
        operation == QStringLiteral("START_AIRCRAFT"))
    {
        request.operation = operation == QStringLiteral("STOP_AIRCRAFT")
                                ? ScenarioOperation::StopAircraft
                                : operation == QStringLiteral("PAUSE_AIRCRAFT")
                                      ? ScenarioOperation::PauseAircraft
                                      : operation == QStringLiteral("START_AIRCRAFT")
                                            ? ScenarioOperation::StartAircraft
                                            : ScenarioOperation::ResumeAircraft;
        return true;
    }

    if (operation == QStringLiteral("CANCEL_EVENT"))
    {
        request.operation = ScenarioOperation::CancelScheduledTakeoff;
        return true;
    }

    request.operation = ScenarioOperation::ScheduleTakeoff;
    const QString time = object.value(QStringLiteral("time")).toString().trimmed();
    const QRegularExpression timePattern(QStringLiteral("^(\\d{1,2}):(\\d{2})$"));
    const QRegularExpressionMatch timeMatch = timePattern.match(time);
    if (!timeMatch.hasMatch())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("MODEL TIME MUST USE HH:MM");
        return false;
    }
    const int hour = timeMatch.captured(1).toInt();
    const int minute = timeMatch.captured(2).toInt();
    if (hour > 23 || minute > 59)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("MODEL TIME IS INVALID");
        return false;
    }
    request.scheduledSeconds = hour * 3600 + minute * 60;
    request.originAirbaseName = object.value(QStringLiteral("origin")).toString().trimmed();
    request.destinationName = object.value(QStringLiteral("destination")).toString().trimmed();
    return true;
}

    bool AICommand::fromResponse(const QString &response, ScenarioRequest &request,
                                 QString *errorMessage)
    {
        QString jsonText = response.trimmed();
        qDebug().noquote() << "[AI COMMAND] response bytes=" << response.toUtf8().size()
                           << "fenced="
                           << (jsonText.startsWith(QStringLiteral("```")));
        const QRegularExpression fencedPattern(
            QStringLiteral("^```(?:json)?\\s*([\\s\\S]*?)\\s*```$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch fencedMatch = fencedPattern.match(jsonText);
        if (fencedMatch.hasMatch())
            jsonText = fencedMatch.captured(1).trimmed();

        QJsonParseError parseError;
        const QJsonDocument document =
            QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("MODEL RESPONSE CONTAINS INVALID JSON: %1")
                                    .arg(parseError.errorString());
            qWarning().noquote() << "[AI COMMAND] invalid JSON:"
                                 << parseError.errorString();
            return false;
        }
        if (!document.isObject())
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("MODEL RESPONSE MUST BE ONE JSON OBJECT");
            qWarning().noquote() << "[AI COMMAND] JSON parse failed:"
                                 << (errorMessage ? *errorMessage : QString());
            return false;
        }
        QJsonObject commandObject = document.object();
        if (!commandObject.contains(QStringLiteral("operation")) &&
            commandObject.size() == 1)
        {
            const auto operation = commandObject.constBegin();
            if (validOperation(operation.key()) && operation.value().isObject())
            {
                const QString operationName = operation.key();
                commandObject = operation.value().toObject();
                commandObject.insert(QStringLiteral("operation"), operationName);
                qWarning().noquote() << "[AI COMMAND] normalized operation-key wrapper:"
                                     << operationName;
            }
        }
        const bool valid = fromJson(commandObject, request, errorMessage);
        qDebug().noquote() << "[AI COMMAND] validation result=" << valid
                           << "operation=" << document.object().value(QStringLiteral("operation")).toString()
                           << "error=" << (errorMessage ? *errorMessage : QString());
        return valid;
    }

    bool AICommand::toPrompt(const ScenarioRequest &request, QString *prompt,
                             QString *errorMessage)
    {
        if (!prompt)
            return false;
        switch (request.operation)
        {
        case ScenarioOperation::Add:
            *prompt = QStringLiteral("Add %1 aircraft %2 %3")
                          .arg(request.entityCount)
                          .arg(isRegionLocation(request.locationName)
                                   ? QStringLiteral("in")
                                   : QStringLiteral("at"))
                          .arg(request.locationName);
            for (int index = 0; index < request.routeDestinations.size(); ++index)
                *prompt += index == 0
                               ? QStringLiteral(" and send one to %1")
                                     .arg(request.routeDestinations.at(index))
                               : QStringLiteral(" and make another fly toward %1")
                                     .arg(request.routeDestinations.at(index));
            return true;
        case ScenarioOperation::CreateFormation:
            *prompt = QStringLiteral("Create %1 aircraft in a %2 formation")
                          .arg(request.entityCount)
                          .arg(request.formationType);
            return true;
        case ScenarioOperation::MoveFormation:
            *prompt = request.allFormations
                          ? QStringLiteral("Move all %1formations towards %2")
                                .arg(request.formationTypeSpecified
                                         ? request.formationType + QLatin1Char(' ')
                                         : QString(),
                                     request.operationLocation)
                          : QStringLiteral("Move the %1formation towards %2")
                                .arg(request.formationTypeSpecified
                                         ? request.formationType + QLatin1Char(' ')
                                         : QString(),
                                     request.operationLocation);
            return true;
        case ScenarioOperation::RemoveAircraft:
            *prompt = QStringLiteral("Remove aircraft %1").arg(request.operationTargetId);
            return true;
        case ScenarioOperation::RemoveLastAircraft:
            *prompt = QStringLiteral("Remove last %1 aircraft").arg(request.entityCount);
            return true;
        case ScenarioOperation::RemoveAllAircraft:
            *prompt = QStringLiteral("Remove all aircraft");
            return true;
        case ScenarioOperation::StopAllAircraft:
            *prompt = QStringLiteral("Stop all aircraft");
            return true;
        case ScenarioOperation::StopFormation:
            *prompt = request.allFormations
                          ? QStringLiteral("Stop all %1formations")
                                .arg(request.formationTypeSpecified
                                         ? request.formationType + QLatin1Char(' ')
                                         : QString())
                          : QStringLiteral("Stop the %1formation")
                                .arg(request.formationTypeSpecified
                                         ? request.formationType + QLatin1Char(' ')
                                         : QString());
            return true;
        case ScenarioOperation::ResumeFormation:
            *prompt = request.allFormations
                          ? QStringLiteral("Resume all %1formations")
                                .arg(request.formationTypeSpecified
                                         ? request.formationType + QLatin1Char(' ')
                                         : QString())
                          : QStringLiteral("Resume the %1formation")
                                .arg(request.formationTypeSpecified
                                         ? request.formationType + QLatin1Char(' ')
                                         : QString());
            return true;
        case ScenarioOperation::DisbandFormation:
            *prompt = request.allFormations
                          ? QStringLiteral("Disband all %1formations")
                                .arg(request.formationTypeSpecified
                                         ? request.formationType + QLatin1Char(' ')
                                         : QString())
                          : QStringLiteral("Disband the %1formation")
                                .arg(request.formationTypeSpecified
                                         ? request.formationType + QLatin1Char(' ')
                                         : QString());
            return true;
        case ScenarioOperation::Move:
            *prompt = QStringLiteral("Move %1 to %2")
                          .arg(request.operationTargetId, request.operationLocation);
            return true;
        case ScenarioOperation::ChangeRoute:
            *prompt = QStringLiteral("Change route for %1 to %2")
                          .arg(request.operationTargetId, request.operationLocation);
            return true;
        case ScenarioOperation::CreateRoute:
        case ScenarioOperation::DisplayRoute:
        case ScenarioOperation::AssignRoute:
        case ScenarioOperation::MoveAlongRoute:
            *prompt = QStringLiteral("%1 route from %2")
                          .arg(request.operation == ScenarioOperation::CreateRoute
                                   ? QStringLiteral("Create a")
                                   : request.operation == ScenarioOperation::DisplayRoute
                                         ? QStringLiteral("Show a")
                                         : request.operation == ScenarioOperation::AssignRoute
                                               ? QStringLiteral("Assign a")
                                               : QStringLiteral("Move along a"),
                               request.routeWaypoints.join(QStringLiteral(" to ")));
            if (!request.routeAircraftId.isEmpty())
                *prompt += QStringLiteral(" to aircraft %1").arg(request.routeAircraftId);
            return true;
        case ScenarioOperation::StopAircraft:
            *prompt = QStringLiteral("Stop %1").arg(request.operationTargetId);
            return true;
        case ScenarioOperation::PauseAircraft:
            *prompt = QStringLiteral("Pause %1").arg(request.operationTargetId);
            return true;
        case ScenarioOperation::ResumeAircraft:
            *prompt = QStringLiteral("Resume %1").arg(request.operationTargetId);
            return true;
        case ScenarioOperation::StartAircraft:
            *prompt = QStringLiteral("Start %1").arg(request.operationTargetId);
            return true;
        case ScenarioOperation::ScheduleTakeoff:
            *prompt = QStringLiteral("Schedule %1 to take off%2 at %3%4")
                          .arg(request.operationTargetId,
                               request.originAirbaseName.isEmpty()
                                   ? QString()
                                   : QStringLiteral(" from ") + request.originAirbaseName,
                               QStringLiteral("%1:%2")
                                   .arg(request.scheduledSeconds / 3600, 2, 10, QLatin1Char('0'))
                                   .arg((request.scheduledSeconds / 60) % 60, 2, 10, QLatin1Char('0')),
                               request.destinationName.isEmpty()
                                   ? QString()
                                   : QStringLiteral(" and fly to ") + request.destinationName);
            return true;
        case ScenarioOperation::CancelScheduledTakeoff:
            *prompt = QStringLiteral("Cancel the scheduled take-off for %1")
                          .arg(request.operationTargetId);
            return true;
        case ScenarioOperation::Query:
            *prompt = request.airbaseDisplayOnly
                          ? QStringLiteral("Show airbases in %1").arg(request.airbaseRegion)
                          : QStringLiteral("Query scenario");
            return true;
        default:
            if (errorMessage)
                *errorMessage = QStringLiteral("MODEL OPERATION CANNOT BE EXECUTED");
            return false;
        }
    }
