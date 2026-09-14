#pragma once

#include "../data/dataresult.h"
#include "../data/models.h"

#include <QByteArray>
#include <QDate>
#include <QString>
#include <QVariant>
#include <QVector>

class SpreadsheetScheduleImporter
{
public:
    static DataResult<QVector<Course>> parseFile(const QString &filePath, int totalWeeks,
                                                 const QDate &semesterStartDate = {});
    static DataResult<QVector<Course>> parseCsv(const QByteArray &data, int totalWeeks,
                                                const QDate &semesterStartDate = {});

    static QString columnHelp();

private:
    static DataResult<QVector<Course>> parseRows(const QVector<QVector<QVariant>> &rows,
                                                 int totalWeeks,
                                                 const QDate &semesterStartDate);
    static DataResult<QVector<Course>> parseXlsx(const QString &filePath, int totalWeeks,
                                                 const QDate &semesterStartDate);
#ifdef Q_OS_WIN
    static DataResult<QVector<Course>> parseExcel(const QString &filePath, int totalWeeks,
                                                  const QDate &semesterStartDate);
#endif
};
