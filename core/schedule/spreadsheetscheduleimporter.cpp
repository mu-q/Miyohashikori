#include "spreadsheetscheduleimporter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QRegularExpression>
#include <QStringConverter>
#include <QXmlStreamReader>
#include <QtGui/private/qzipreader_p.h>
#include <QtMath>

#include <algorithm>

#ifdef Q_OS_WIN
#include <QAxObject>
#endif

namespace {

using Rows = QVector<QVector<QVariant>>;

struct ColumnMapping
{
    int name = -1;
    int weekday = -1;
    int startTime = -1;
    int endTime = -1;
    int timeRange = -1;
    int startWeek = -1;
    int endWeek = -1;
    int weekRange = -1;
    int pattern = -1;
    int teacher = -1;
    int room = -1;
    int firstDate = -1;
    int lastDate = -1;
    int date = -1;

    int semanticScore() const
    {
        if (name < 0)
            return 0;
        int score = 1;
        score += weekday >= 0 || firstDate >= 0 || date >= 0 ? 1 : 0;
        score += (startTime >= 0 && endTime >= 0) || timeRange >= 0 ? 1 : 0;
        score += (startWeek >= 0 && endWeek >= 0) || weekRange >= 0
                     || firstDate >= 0 || date >= 0 ? 1 : 0;
        return score;
    }
};

QString normalizedHeader(QString text)
{
    text = text.trimmed().toLower();
    text.remove(QRegularExpression(QStringLiteral("[\\s_\\-（）()]+")));
    return text;
}

int findColumn(const QHash<QString, int> &columns, const QStringList &aliases)
{
    int bestColumn = -1;
    int bestScore = 0;
    for (const QString &alias : aliases) {
        const QString normalizedAlias = normalizedHeader(alias);
        for (auto it = columns.cbegin(); it != columns.cend(); ++it) {
            const QString &header = it.key();
            int score = 0;
            if (header == normalizedAlias)
                score = 100;
            else if (normalizedAlias.size() >= 2 && header.contains(normalizedAlias))
                score = 80 - qMin(20, header.size() - normalizedAlias.size());
            else if (header.size() >= 2 && normalizedAlias.contains(header))
                score = 65 - qMin(20, normalizedAlias.size() - header.size());
            if (score > bestScore) {
                bestScore = score;
                bestColumn = it.value();
            }
        }
    }
    return bestColumn;
}

ColumnMapping matchColumns(const QHash<QString, int> &columns)
{
    ColumnMapping result;
    result.name = findColumn(columns, {QStringLiteral("课程名称"), QStringLiteral("课程名"),
        QStringLiteral("科目名称"), QStringLiteral("科目"), QStringLiteral("教学班名称"),
        QStringLiteral("课程活动"), QStringLiteral("subject"), QStringLiteral("course"),
        QStringLiteral("title"), QStringLiteral("name")});
    result.weekday = findColumn(columns, {QStringLiteral("星期"), QStringLiteral("星期几"),
        QStringLiteral("周几"), QStringLiteral("上课星期"), QStringLiteral("上课日"),
        QStringLiteral("weekday"), QStringLiteral("dayofweek"), QStringLiteral("classday"),
        QStringLiteral("day")});
    result.startTime = findColumn(columns, {QStringLiteral("开始时间"), QStringLiteral("上课开始时间"),
        QStringLiteral("起始时间"), QStringLiteral("开始日期时间"), QStringLiteral("起始日期时间"),
        QStringLiteral("上课钟点"), QStringLiteral("starttime"), QStringLiteral("startdatetime"),
        QStringLiteral("begintime")});
    result.endTime = findColumn(columns, {QStringLiteral("结束时间"), QStringLiteral("下课时间"),
        QStringLiteral("终止时间"), QStringLiteral("结束日期时间"), QStringLiteral("终止日期时间"),
        QStringLiteral("endtime"), QStringLiteral("enddatetime"), QStringLiteral("finishtime")});
    result.timeRange = findColumn(columns, {QStringLiteral("时间段"), QStringLiteral("上课时间"),
        QStringLiteral("课程时间"), QStringLiteral("时段"), QStringLiteral("classtime"),
        QStringLiteral("timerange")});
    result.startWeek = findColumn(columns, {QStringLiteral("开始周"), QStringLiteral("起始周"),
        QStringLiteral("首周"), QStringLiteral("startweek"), QStringLiteral("beginweek")});
    result.endWeek = findColumn(columns, {QStringLiteral("结束周"), QStringLiteral("终止周"),
        QStringLiteral("末周"), QStringLiteral("endweek"), QStringLiteral("lastweek")});
    result.weekRange = findColumn(columns, {QStringLiteral("周次"), QStringLiteral("教学周"),
        QStringLiteral("上课周次"), QStringLiteral("周范围"), QStringLiteral("开课周次"),
        QStringLiteral("weeks"), QStringLiteral("weekrange"), QStringLiteral("schedule")});
    result.pattern = findColumn(columns, {QStringLiteral("单双周"), QStringLiteral("重复规则"),
        QStringLiteral("周类型"), QStringLiteral("重复"), QStringLiteral("weekpattern"),
        QStringLiteral("parity"), QStringLiteral("type")});
    result.teacher = findColumn(columns, {QStringLiteral("教师"), QStringLiteral("老师"),
        QStringLiteral("任课教师"), QStringLiteral("授课教师"), QStringLiteral("教师姓名"),
        QStringLiteral("teacher"), QStringLiteral("instructor"), QStringLiteral("lecturer")});
    result.room = findColumn(columns, {QStringLiteral("教室"), QStringLiteral("地点"),
        QStringLiteral("上课地点"), QStringLiteral("教学地点"), QStringLiteral("场地"),
        QStringLiteral("room"), QStringLiteral("location"), QStringLiteral("classroom"),
        QStringLiteral("venue")});
    result.firstDate = findColumn(columns, {QStringLiteral("首次日期"), QStringLiteral("首次上课"),
        QStringLiteral("开始日期"), QStringLiteral("firstdate"), QStringLiteral("startdate")});
    result.lastDate = findColumn(columns, {QStringLiteral("最后日期"), QStringLiteral("最后上课"),
        QStringLiteral("结束日期"), QStringLiteral("lastdate"), QStringLiteral("enddate")});
    result.date = findColumn(columns, {QStringLiteral("上课日期"), QStringLiteral("课程日期"),
        QStringLiteral("日期"), QStringLiteral("classdate"), QStringLiteral("date")});
    return result;
}

QString valueText(const QVector<QVariant> &row, int column)
{
    return column >= 0 && column < row.size() ? row.at(column).toString().trimmed() : QString();
}

bool isBlankRow(const QVector<QVariant> &row)
{
    for (const QVariant &value : row) {
        if (!value.toString().trimmed().isEmpty())
            return false;
    }
    return true;
}

int parseInteger(const QVariant &value, bool *ok)
{
    if (value.metaType().id() != QMetaType::QString) {
        const double number = value.toDouble(ok);
        if (*ok && qAbs(number - qRound(number)) < 0.000001)
            return qRound(number);
        *ok = false;
        return 0;
    }
    QString text = value.toString().trimmed();
    text.remove(QRegularExpression(QStringLiteral("[^0-9+-].*$")));
    return text.toInt(ok);
}

QTime parseTime(const QVariant &value)
{
    if (value.canConvert<QDateTime>()) {
        const QDateTime dateTime = value.toDateTime();
        if (dateTime.isValid())
            return dateTime.time();
    }
    bool numberOk = false;
    const double number = value.toDouble(&numberOk);
    if (numberOk && number >= 0.0 && number < 1.0) {
        const int seconds = qRound(number * 86400.0) % 86400;
        return QTime(0, 0).addSecs(seconds);
    }
    const QString text = value.toString().trimmed();
    for (const QString &format : {QStringLiteral("H:mm"), QStringLiteral("H:mm:ss"),
                                  QStringLiteral("h:mm AP")}) {
        const QTime time = QTime::fromString(text, format);
        if (time.isValid())
            return time;
    }
    return {};
}

QDateTime parseDateTime(const QVariant &value)
{
    if (value.canConvert<QDateTime>()) {
        const QDateTime dateTime = value.toDateTime();
        if (dateTime.isValid())
            return dateTime;
    }
    bool numberOk = false;
    const double serial = value.toDouble(&numberOk);
    if (numberOk && serial >= 1.0) {
        const int wholeDays = qFloor(serial);
        const int seconds = qRound((serial - wholeDays) * 86400.0) % 86400;
        return QDateTime(QDate(1899, 12, 30).addDays(wholeDays), QTime(0, 0).addSecs(seconds));
    }
    const QString text = value.toString().trimmed();
    for (const QString &format : {QStringLiteral("yyyy-MM-dd HH:mm:ss"),
                                  QStringLiteral("yyyy-MM-dd H:mm"),
                                  QStringLiteral("yyyy/M/d H:mm"),
                                  QStringLiteral("yyyy/M/d")}) {
        const QDateTime dateTime = QDateTime::fromString(text, format);
        if (dateTime.isValid())
            return dateTime;
    }
    const QDate isoDate = QDate::fromString(text, Qt::ISODate);
    return isoDate.isValid() ? QDateTime(isoDate, QTime(0, 0)) : QDateTime();
}

bool parseTimeRange(const QString &text, QTime *startTime, QTime *endTime)
{
    static const QRegularExpression expression(
        QStringLiteral("(\\d{1,2}:\\d{2})(?::\\d{2})?\\s*[-~～—–至到]\\s*"
                       "(\\d{1,2}:\\d{2})(?::\\d{2})?"));
    const QRegularExpressionMatch match = expression.match(text);
    if (!match.hasMatch())
        return false;
    *startTime = QTime::fromString(match.captured(1), QStringLiteral("H:mm"));
    *endTime = QTime::fromString(match.captured(2), QStringLiteral("H:mm"));
    return startTime->isValid() && endTime->isValid();
}

bool parseWeekRange(QString text, int *startWeek, int *endWeek,
                    CourseWeekPattern *pattern)
{
    const QString normalized = normalizedHeader(text);
    if (normalized.contains(QStringLiteral("单")) || normalized.contains(QStringLiteral("奇数"))
        || normalized.contains(QStringLiteral("odd")))
        *pattern = CourseWeekPattern::OddWeeks;
    else if (normalized.contains(QStringLiteral("双")) || normalized.contains(QStringLiteral("偶数"))
             || normalized.contains(QStringLiteral("even")))
        *pattern = CourseWeekPattern::EvenWeeks;

    QVector<int> numbers;
    QRegularExpressionMatchIterator matches = QRegularExpression(QStringLiteral("\\d+")).globalMatch(text);
    while (matches.hasNext())
        numbers.append(matches.next().captured().toInt());
    if (numbers.isEmpty())
        return false;
    if (numbers.size() > 2) {
        const int step = numbers.at(1) - numbers.first();
        if (step != 1 && step != 2)
            return false;
        for (int index = 2; index < numbers.size(); ++index) {
            if (numbers.at(index) - numbers.at(index - 1) != step)
                return false;
        }
        if (step == 2)
            *pattern = numbers.first() % 2 == 0 ? CourseWeekPattern::EvenWeeks
                                                : CourseWeekPattern::OddWeeks;
    }
    *startWeek = numbers.first();
    *endWeek = numbers.last();
    return true;
}

int weekForDate(const QDate &date, const QDate &semesterStartDate)
{
    if (!date.isValid() || !semesterStartDate.isValid() || date < semesterStartDate)
        return 0;
    return semesterStartDate.daysTo(date) / 7 + 1;
}

int parseWeekday(QString text)
{
    text = normalizedHeader(text);
    bool ok = false;
    const int number = text.toInt(&ok);
    if (ok && number >= 1 && number <= 7)
        return number;
    static const QVector<QStringList> names = {
        {QStringLiteral("周一"), QStringLiteral("星期一"), QStringLiteral("礼拜一"), QStringLiteral("monday"), QStringLiteral("mon")},
        {QStringLiteral("周二"), QStringLiteral("星期二"), QStringLiteral("礼拜二"), QStringLiteral("tuesday"), QStringLiteral("tue")},
        {QStringLiteral("周三"), QStringLiteral("星期三"), QStringLiteral("礼拜三"), QStringLiteral("wednesday"), QStringLiteral("wed")},
        {QStringLiteral("周四"), QStringLiteral("星期四"), QStringLiteral("礼拜四"), QStringLiteral("thursday"), QStringLiteral("thu")},
        {QStringLiteral("周五"), QStringLiteral("星期五"), QStringLiteral("礼拜五"), QStringLiteral("friday"), QStringLiteral("fri")},
        {QStringLiteral("周六"), QStringLiteral("星期六"), QStringLiteral("礼拜六"), QStringLiteral("saturday"), QStringLiteral("sat")},
        {QStringLiteral("周日"), QStringLiteral("周天"), QStringLiteral("星期日"), QStringLiteral("星期天"), QStringLiteral("sunday"), QStringLiteral("sun")}
    };
    for (int day = 0; day < names.size(); ++day) {
        for (const QString &name : names.at(day)) {
            if (text == normalizedHeader(name))
                return day + 1;
        }
    }
    return 0;
}

CourseWeekPattern parsePattern(QString text, bool *ok)
{
    text = normalizedHeader(text);
    if (text.isEmpty() || text == QStringLiteral("每周") || text == QStringLiteral("全周")
        || text == QStringLiteral("everyweek") || text == QStringLiteral("0")) {
        *ok = true;
        return CourseWeekPattern::EveryWeek;
    }
    if (text == QStringLiteral("单周") || text == QStringLiteral("奇数周")
        || text == QStringLiteral("odd") || text == QStringLiteral("oddweeks")
        || text == QStringLiteral("1")) {
        *ok = true;
        return CourseWeekPattern::OddWeeks;
    }
    if (text == QStringLiteral("双周") || text == QStringLiteral("偶数周")
        || text == QStringLiteral("even") || text == QStringLiteral("evenweeks")
        || text == QStringLiteral("2")) {
        *ok = true;
        return CourseWeekPattern::EvenWeeks;
    }
    *ok = false;
    return CourseWeekPattern::EveryWeek;
}

QChar detectDelimiter(const QString &text)
{
    const QString firstLine = text.section(QLatin1Char('\n'), 0, 0);
    const QVector<QChar> candidates = {QLatin1Char(','), QLatin1Char('\t'), QLatin1Char(';')};
    QChar best = QLatin1Char(',');
    int bestCount = -1;
    for (const QChar candidate : candidates) {
        int count = 0;
        bool quoted = false;
        for (const QChar ch : firstLine) {
            if (ch == QLatin1Char('"')) quoted = !quoted;
            else if (!quoted && ch == candidate) ++count;
        }
        if (count > bestCount) { best = candidate; bestCount = count; }
    }
    return best;
}

Rows parseDelimited(const QString &text, QChar delimiter, QString *error)
{
    Rows rows;
    QVector<QVariant> row;
    QString field;
    bool quoted = false;
    for (qsizetype index = 0; index < text.size(); ++index) {
        const QChar ch = text.at(index);
        if (quoted) {
            if (ch == QLatin1Char('"')) {
                if (index + 1 < text.size() && text.at(index + 1) == QLatin1Char('"')) {
                    field.append(QLatin1Char('"'));
                    ++index;
                } else {
                    quoted = false;
                }
            } else {
                field.append(ch);
            }
        } else if (ch == QLatin1Char('"') && field.isEmpty()) {
            quoted = true;
        } else if (ch == delimiter) {
            row.append(field); field.clear();
        } else if (ch == QLatin1Char('\n')) {
            row.append(field); field.clear();
            if (!row.isEmpty()) rows.append(row);
            row.clear();
        } else if (ch != QLatin1Char('\r')) {
            field.append(ch);
        }
    }
    if (quoted) {
        *error = QStringLiteral("CSV 中存在未闭合的引号");
        return {};
    }
    if (!field.isEmpty() || !row.isEmpty()) {
        row.append(field);
        rows.append(row);
    }
    return rows;
}

#ifdef Q_OS_WIN
Rows rowsFromExcelValue(const QVariant &value)
{
    Rows rows;
    const QVariantList outer = value.toList();
    if (outer.isEmpty()) {
        rows.append({value});
        return rows;
    }
    for (const QVariant &rowValue : outer) {
        const QVariantList cells = rowValue.toList();
        QVector<QVariant> row;
        if (cells.isEmpty()) row.append(rowValue);
        else for (const QVariant &cell : cells) row.append(cell);
        rows.append(row);
    }
    return rows;
}
#endif

int columnFromCellReference(const QString &reference)
{
    int column = 0;
    for (const QChar character : reference) {
        if (!character.isLetter())
            break;
        column = column * 26 + character.toUpper().unicode() - QLatin1Char('A').unicode() + 1;
    }
    return column - 1;
}

QStringList readSharedStrings(const QByteArray &xmlData)
{
    QStringList strings;
    QXmlStreamReader xml(xmlData);
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != QLatin1String("si"))
            continue;
        QString value;
        while (!(xml.isEndElement() && xml.name() == QLatin1String("si")) && !xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QLatin1String("t"))
                value.append(xml.readElementText());
        }
        strings.append(value);
    }
    return strings;
}

Rows readWorksheet(const QByteArray &xmlData, const QStringList &sharedStrings, QString *error)
{
    Rows rows;
    QXmlStreamReader xml(xmlData);
    QVector<QVariant> row;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("row")) {
            row.clear();
        } else if (xml.isStartElement() && xml.name() == QLatin1String("c")) {
            const QString reference = xml.attributes().value(QLatin1String("r")).toString();
            const QString type = xml.attributes().value(QLatin1String("t")).toString();
            const int column = columnFromCellReference(reference);
            QString rawValue;
            QString inlineText;
            while (!(xml.isEndElement() && xml.name() == QLatin1String("c")) && !xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement() && xml.name() == QLatin1String("v"))
                    rawValue = xml.readElementText();
                else if (xml.isStartElement() && xml.name() == QLatin1String("t"))
                    inlineText.append(xml.readElementText());
            }
            QVariant value;
            if (type == QLatin1String("s")) {
                bool ok = false;
                const int index = rawValue.toInt(&ok);
                value = ok && index >= 0 && index < sharedStrings.size()
                            ? QVariant(sharedStrings.at(index)) : QVariant(rawValue);
            } else if (type == QLatin1String("inlineStr")) {
                value = inlineText;
            } else if (type == QLatin1String("b")) {
                value = rawValue == QLatin1String("1");
            } else if (type == QLatin1String("d") || type == QLatin1String("str")) {
                value = rawValue;
            } else {
                bool ok = false;
                const double number = rawValue.toDouble(&ok);
                value = ok ? QVariant(number) : QVariant(rawValue);
            }
            if (column >= 0) {
                if (row.size() <= column)
                    row.resize(column + 1);
                row[column] = value;
            }
        } else if (xml.isEndElement() && xml.name() == QLatin1String("row")) {
            rows.append(row);
        }
    }
    if (xml.hasError())
        *error = QStringLiteral("工作表 XML 损坏：%1").arg(xml.errorString());
    return rows;
}

} // namespace

DataResult<QVector<Course>> SpreadsheetScheduleImporter::parseFile(const QString &filePath,
                                                                    int totalWeeks,
                                                                    const QDate &semesterStartDate)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QStringLiteral("csv") || suffix == QStringLiteral("tsv")) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly))
            return DataResult<QVector<Course>>::fail(QStringLiteral("无法读取表格：%1").arg(file.errorString()));
        return parseCsv(file.readAll(), totalWeeks, semesterStartDate);
    }
    if (suffix == QStringLiteral("xlsx"))
        return parseXlsx(filePath, totalWeeks, semesterStartDate);
#ifdef Q_OS_WIN
    if (suffix == QStringLiteral("xls"))
        return parseExcel(filePath, totalWeeks, semesterStartDate);
#endif
    return DataResult<QVector<Course>>::fail(QStringLiteral("仅支持 .xlsx、.xls、.csv 和 .tsv 文件"));
}

DataResult<QVector<Course>> SpreadsheetScheduleImporter::parseXlsx(
    const QString &filePath, int totalWeeks, const QDate &semesterStartDate)
{
    QZipReader archive(filePath);
    if (!archive.exists() || !archive.isReadable())
        return DataResult<QVector<Course>>::fail(QStringLiteral("无法读取 XLSX 工作簿"));

    const QStringList sharedStrings = readSharedStrings(
        archive.fileData(QStringLiteral("xl/sharedStrings.xml")));
    QList<QZipReader::FileInfo> worksheets;
    for (const QZipReader::FileInfo &entry : archive.fileInfoList()) {
        if (entry.isFile && entry.filePath.startsWith(QStringLiteral("xl/worksheets/"))
            && entry.filePath.endsWith(QStringLiteral(".xml")))
            worksheets.append(entry);
    }
    std::sort(worksheets.begin(), worksheets.end(), [](const auto &left, const auto &right) {
        return left.filePath < right.filePath;
    });
    if (worksheets.isEmpty())
        return DataResult<QVector<Course>>::fail(QStringLiteral("XLSX 中没有工作表"));

    QString lastError = QStringLiteral("没有找到可识别的课程明细表");
    for (const QZipReader::FileInfo &worksheet : worksheets) {
        QString xmlError;
        const Rows rows = readWorksheet(archive.fileData(worksheet.filePath), sharedStrings,
                                        &xmlError);
        if (!xmlError.isEmpty()) {
            lastError = xmlError;
            continue;
        }
        const auto parsed = parseRows(rows, totalWeeks, semesterStartDate);
        if (parsed.success)
            return parsed;
        lastError = parsed.error;
    }
    return DataResult<QVector<Course>>::fail(lastError);
}

DataResult<QVector<Course>> SpreadsheetScheduleImporter::parseCsv(const QByteArray &data,
                                                                   int totalWeeks,
                                                                   const QDate &semesterStartDate)
{
    QStringDecoder utf8(QStringDecoder::Utf8);
    QString text = utf8.decode(data);
    if (utf8.hasError())
        text = QString::fromLocal8Bit(data);
    if (!text.isEmpty() && text.front() == QChar::ByteOrderMark)
        text.remove(0, 1);
    QString error;
    const Rows rows = parseDelimited(text, detectDelimiter(text), &error);
    if (!error.isEmpty())
        return DataResult<QVector<Course>>::fail(error);
    return parseRows(rows, totalWeeks, semesterStartDate);
}

QString SpreadsheetScheduleImporter::columnHelp()
{
    return QStringLiteral("会自动匹配常见的中英文列名和列顺序。课程名称必须存在；"
                          "星期、时间和周次可分别使用独立列，也可写成时间段、周次范围，"
                          "还可由首次/最后上课日期推导。教师和教室可选。");
}

DataResult<QVector<Course>> SpreadsheetScheduleImporter::parseRows(const Rows &rows,
                                                                    int totalWeeks,
                                                                    const QDate &semesterStartDate)
{
    if (totalWeeks < 1 || totalWeeks > 40)
        return DataResult<QVector<Course>>::fail(QStringLiteral("当前学期周数无效"));
    if (rows.isEmpty())
        return DataResult<QVector<Course>>::fail(QStringLiteral("表格为空"));
    int headerRow = -1;
    int bestScore = 0;
    ColumnMapping mapping;
    for (int rowIndex = 0; rowIndex < qMin(rows.size(), 30); ++rowIndex) {
        QHash<QString, int> columns;
        for (int column = 0; column < rows.at(rowIndex).size(); ++column)
            columns.insert(normalizedHeader(rows.at(rowIndex).at(column).toString()), column);
        const ColumnMapping candidate = matchColumns(columns);
        const int score = candidate.semanticScore();
        if (score > bestScore) {
            bestScore = score;
            headerRow = rowIndex;
            mapping = candidate;
        }
    }
    if (headerRow < 0 || bestScore < 4)
        return DataResult<QVector<Course>>::fail(
            QStringLiteral("无法识别包含课程名称、星期/日期、时间和周次的明细表。%1")
                .arg(columnHelp()));

    QVector<Course> courses;
    for (int rowIndex = headerRow + 1; rowIndex < rows.size(); ++rowIndex) {
        const QVector<QVariant> &row = rows.at(rowIndex);
        if (isBlankRow(row))
            continue;
        Course course;
        course.name = valueText(row, mapping.name);
        course.teacher = valueText(row, mapping.teacher);
        course.room = valueText(row, mapping.room);
        const QDateTime firstDate = mapping.firstDate >= 0 && mapping.firstDate < row.size()
                                        ? parseDateTime(row.at(mapping.firstDate)) : QDateTime();
        const QDateTime lastDate = mapping.lastDate >= 0 && mapping.lastDate < row.size()
                                       ? parseDateTime(row.at(mapping.lastDate)) : QDateTime();
        const QDateTime singleDate = mapping.date >= 0 && mapping.date < row.size()
                                         ? parseDateTime(row.at(mapping.date)) : QDateTime();
        course.weekday = parseWeekday(valueText(row, mapping.weekday));
        if (course.weekday == 0) {
            const QDate inferredDate = firstDate.isValid() ? firstDate.date() : singleDate.date();
            if (inferredDate.isValid())
                course.weekday = inferredDate.dayOfWeek();
        }
        course.startTime = mapping.startTime >= 0 && mapping.startTime < row.size()
                               ? parseTime(row.at(mapping.startTime)) : QTime();
        course.endTime = mapping.endTime >= 0 && mapping.endTime < row.size()
                             ? parseTime(row.at(mapping.endTime)) : QTime();
        if (!course.startTime.isValid() && mapping.startTime >= 0
            && mapping.startTime < row.size())
            course.startTime = parseDateTime(row.at(mapping.startTime)).time();
        if (!course.endTime.isValid() && mapping.endTime >= 0
            && mapping.endTime < row.size())
            course.endTime = parseDateTime(row.at(mapping.endTime)).time();
        if ((!course.startTime.isValid() || !course.endTime.isValid())
            && mapping.timeRange >= 0 && mapping.timeRange < row.size()) {
            parseTimeRange(valueText(row, mapping.timeRange), &course.startTime, &course.endTime);
        }
        bool startWeekOk = false;
        bool endWeekOk = false;
        course.startWeek = mapping.startWeek >= 0 && mapping.startWeek < row.size()
                               ? parseInteger(row.at(mapping.startWeek), &startWeekOk) : 0;
        course.endWeek = mapping.endWeek >= 0 && mapping.endWeek < row.size()
                             ? parseInteger(row.at(mapping.endWeek), &endWeekOk) : 0;
        bool patternOk = false;
        course.weekPattern = parsePattern(valueText(row, mapping.pattern), &patternOk);
        if (mapping.weekRange >= 0 && mapping.weekRange < row.size()) {
            int rangeStart = 0;
            int rangeEnd = 0;
            CourseWeekPattern rangePattern = course.weekPattern;
            if (parseWeekRange(valueText(row, mapping.weekRange), &rangeStart, &rangeEnd,
                               &rangePattern)) {
                if (!startWeekOk) { course.startWeek = rangeStart; startWeekOk = true; }
                if (!endWeekOk) { course.endWeek = rangeEnd; endWeekOk = true; }
                if (mapping.pattern < 0 || valueText(row, mapping.pattern).isEmpty()) {
                    course.weekPattern = rangePattern;
                    patternOk = true;
                }
            }
        }
        if (!startWeekOk) {
            course.startWeek = weekForDate(
                firstDate.isValid() ? firstDate.date() : singleDate.date(), semesterStartDate);
            startWeekOk = course.startWeek > 0;
        }
        if (!endWeekOk) {
            const QDate endDate = lastDate.isValid() ? lastDate.date()
                                                     : (singleDate.isValid() ? singleDate.date()
                                                                             : firstDate.date());
            course.endWeek = weekForDate(endDate, semesterStartDate);
            endWeekOk = course.endWeek > 0;
        }
        if (course.name.isEmpty() || course.weekday == 0 || !course.startTime.isValid()
            || !course.endTime.isValid() || course.startTime >= course.endTime
            || !startWeekOk || !endWeekOk || course.startWeek < 1
            || course.endWeek < course.startWeek || course.endWeek > totalWeeks || !patternOk) {
            return DataResult<QVector<Course>>::fail(
                QStringLiteral("第 %1 行课程数据无效，请检查名称、星期、时间、周次与单双周。")
                    .arg(rowIndex + 1));
        }
        courses.append(course);
    }
    if (courses.isEmpty())
        return DataResult<QVector<Course>>::fail(QStringLiteral("表格中没有课程数据"));
    return DataResult<QVector<Course>>::ok(courses);
}

#ifdef Q_OS_WIN
DataResult<QVector<Course>> SpreadsheetScheduleImporter::parseExcel(const QString &filePath,
                                                                     int totalWeeks,
                                                                     const QDate &semesterStartDate)
{
    QAxObject excel(QStringLiteral("Excel.Application"));
    if (excel.isNull())
        return DataResult<QVector<Course>>::fail(
            QStringLiteral("无法启动 Microsoft Excel。请安装 Excel，或将文件另存为 UTF-8 CSV 后导入。"));
    excel.setProperty("Visible", false);
    excel.setProperty("DisplayAlerts", false);
    QAxObject *workbooks = excel.querySubObject("Workbooks");
    QAxObject *workbook = workbooks
                              ? workbooks->querySubObject("Open(const QString&)",
                                                          QDir::toNativeSeparators(filePath))
                              : nullptr;
    if (!workbook) {
        excel.dynamicCall("Quit()");
        return DataResult<QVector<Course>>::fail(QStringLiteral("Excel 无法打开这个工作簿"));
    }
    QAxObject *sheets = workbook->querySubObject("Worksheets");
    const int sheetCount = sheets ? sheets->property("Count").toInt() : 0;
    QString lastError = QStringLiteral("工作簿中没有可读取的工作表");
    DataResult<QVector<Course>> result = DataResult<QVector<Course>>::fail(lastError);
    for (int sheetIndex = 1; sheetIndex <= sheetCount; ++sheetIndex) {
        QAxObject *sheet = sheets->querySubObject("Item(int)", sheetIndex);
        QAxObject *range = sheet ? sheet->querySubObject("UsedRange") : nullptr;
        if (range) {
            result = parseRows(rowsFromExcelValue(range->property("Value2")), totalWeeks,
                               semesterStartDate);
            delete range;
            if (result.success) { delete sheet; break; }
            lastError = result.error;
        }
        delete sheet;
    }
    workbook->dynamicCall("Close(Boolean)", false);
    excel.dynamicCall("Quit()");
    delete sheets;
    delete workbook;
    delete workbooks;
    return result.success ? result : DataResult<QVector<Course>>::fail(lastError);
}
#endif
