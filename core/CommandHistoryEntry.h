#pragma once

#include <QString>

struct CommandHistoryEntry
{
    int sequenceNumber = 0;
    QString prompt;
    QString timestamp;
    QString processingStatus;
    QString parsedOperation;
};
