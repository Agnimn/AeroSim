#include "AIParser.h"

#include <QRegularExpression>

bool AIParser::parsePrompt(const QString &prompt, ScenarioRequest &request,
                           QString *errorMessage) const
{
    const QString normalized = prompt.trimmed().toLower();
    if (normalized.isEmpty())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("PLEASE ENTER A SCENARIO");
        return false;
    }

    if (normalized.contains(QStringLiteral("ground station")) || normalized.contains(QStringLiteral("ground_station")))
    {
        request.entityType = QStringLiteral("ground_station");
    }
    else if (normalized.contains(QStringLiteral("ship")))
    {
        request.entityType = QStringLiteral("ship");
    }
    else if (normalized.contains(QStringLiteral("aircraft")) || normalized.contains(QStringLiteral("fighter")) || normalized.contains(QStringLiteral("plane")) || normalized.contains(QStringLiteral("jet")))
    {
        request.entityType = QStringLiteral("aircraft");
    }
    else
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("SPECIFY AIRCRAFT, SHIP, OR GROUND STATION");
        return false;
    }

    if (normalized.contains(QStringLiteral("north india")))
        request.region = QStringLiteral("North India");
    else if (normalized.contains(QStringLiteral("south india")))
        request.region = QStringLiteral("South India");
    else if (normalized.contains(QStringLiteral("east india")))
        request.region = QStringLiteral("East India");
    else if (normalized.contains(QStringLiteral("west india")))
        request.region = QStringLiteral("West India");
    else
        request.region = QStringLiteral("Unknown Region");

    const QRegularExpression numberPattern(QStringLiteral("\\b(\\d+)\\b"));
    const QRegularExpressionMatch numberMatch = numberPattern.match(normalized);
    request.entityCount = numberMatch.hasMatch() ? numberMatch.captured(1).toInt() : 1;
    request.entityCount = qBound(1, request.entityCount, 20);

    if (errorMessage)
        errorMessage->clear();
    return true;
}
