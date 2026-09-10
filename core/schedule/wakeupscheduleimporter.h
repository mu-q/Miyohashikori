#pragma once

#include "../data/dataresult.h"
#include "../data/models.h"

#include <QByteArray>
#include <QString>

class WakeUpScheduleImporter
{
public:
    static DataResult<ImportedSchedule> parseFile(const QString &filePath);
    static DataResult<ImportedSchedule> parse(const QByteArray &data);
};
