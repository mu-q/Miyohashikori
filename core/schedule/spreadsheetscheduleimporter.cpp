#include "spreadsheetscheduleimporter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QRegularExpression>
#include <QStringConverter>
#include <QtMath>

#ifdef Q_OS_WIN
#include <QAxObject>
#endif

namespace {

using Rows = QVector<QVector<QVariant>>;

QString normalizedHeader(QString text)
{
    text = text.trimmed().toLower();
    text.remove(QRegularExpression(QStringLiteral("[\\s_\\-（）()]+")));
    return text;
}

int findColumn(const QHash<QString, int> &columns, const QStringList &aliases)
{
    for (const QString &alias : aliases) {
        const auto it = columns.constFind(normalizedHeader(alias));
        if (it != columns.cend())
            return it.value();
    }
    return -1;
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

} // namespace

DataResult<QVector<Course>> SpreadsheetScheduleImporter::parseFile(const QString &filePath,
                                                                    int totalWeeks)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QStringLiteral("csv") || suffix == QStringLiteral("tsv")) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly))
            return DataResult<QVector<Course>>::fail(QStringLiteral("无法读取表格：%1").arg(file.errorString()));
        return parseCsv(file.readAll(), totalWeeks);
    }
#ifdef Q_OS_WIN
    if (suffix == QStringLiteral("xlsx") || suffix == QStringLiteral("xls"))
        return parseExcel(filePath, totalWeeks);
#endif
    return DataResult<QVector<Course>>::fail(QStringLiteral("仅支持 .xlsx、.xls、.csv 和 .tsv 文件"));
}

DataResult<QVector<Course>> SpreadsheetScheduleImporter::parseCsv(const QByteArray &data,
                                                                   int totalWeeks)
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
    return parseRows(rows, totalWeeks);
}

QString SpreadsheetScheduleImporter::columnHelp()
{
    return QStringLiteral("首行列名：课程名称、星期、开始时间、结束时间、开始周、结束周；"
                          "可选：单双周、教师、教室。星期可写周一或 1，单双周留空表示每周。");
}

DataResult<QVector<Course>> SpreadsheetScheduleImporter::parseRows(const Rows &rows,
                                                                    int totalWeeks)
{
    if (totalWeeks < 1 || totalWeeks > 40)
        return DataResult<QVector<Course>>::fail(QStringLiteral("当前学期周数无效"));
    if (rows.isEmpty())
        return DataResult<QVector<Course>>::fail(QStringLiteral("表格为空"));
    int headerRow = -1;
    QHash<QString, int> columns;
    for (int rowIndex = 0; rowIndex < qMin(rows.size(), 10); ++rowIndex) {
        columns.clear();
        for (int column = 0; column < rows.at(rowIndex).size(); ++column)
            columns.insert(normalizedHeader(rows.at(rowIndex).at(column).toString()), column);
        if (findColumn(columns, {QStringLiteral("课程名称"), QStringLiteral("课程名"), QStringLiteral("course"), QStringLiteral("name")}) >= 0) {
            headerRow = rowIndex;
            break;
        }
    }
    if (headerRow < 0)
        return DataResult<QVector<Course>>::fail(QStringLiteral("前 10 行中未找到“课程名称”表头"));

    const int nameColumn = findColumn(columns, {QStringLiteral("课程名称"), QStringLiteral("课程名"), QStringLiteral("course"), QStringLiteral("name")});
    const int dayColumn = findColumn(columns, {QStringLiteral("星期"), QStringLiteral("星期几"), QStringLiteral("weekday"), QStringLiteral("day")});
    const int startTimeColumn = findColumn(columns, {QStringLiteral("开始时间"), QStringLiteral("上课时间"), QStringLiteral("starttime"), QStringLiteral("start")});
    const int endTimeColumn = findColumn(columns, {QStringLiteral("结束时间"), QStringLiteral("下课时间"), QStringLiteral("endtime"), QStringLiteral("end")});
    const int startWeekColumn = findColumn(columns, {QStringLiteral("开始周"), QStringLiteral("起始周"), QStringLiteral("startweek")});
    const int endWeekColumn = findColumn(columns, {QStringLiteral("结束周"), QStringLiteral("终止周"), QStringLiteral("endweek")});
    const int patternColumn = findColumn(columns, {QStringLiteral("单双周"), QStringLiteral("重复"), QStringLiteral("周类型"), QStringLiteral("weekpattern"), QStringLiteral("type")});
    const int teacherColumn = findColumn(columns, {QStringLiteral("教师"), QStringLiteral("老师"), QStringLiteral("teacher")});
    const int roomColumn = findColumn(columns, {QStringLiteral("教室"), QStringLiteral("地点"), QStringLiteral("room"), QStringLiteral("location")});
    if (dayColumn < 0 || startTimeColumn < 0 || endTimeColumn < 0
        || startWeekColumn < 0 || endWeekColumn < 0) {
        return DataResult<QVector<Course>>::fail(QStringLiteral("缺少必需列。%1").arg(columnHelp()));
    }

    QVector<Course> courses;
    for (int rowIndex = headerRow + 1; rowIndex < rows.size(); ++rowIndex) {
        const QVector<QVariant> &row = rows.at(rowIndex);
        if (isBlankRow(row))
            continue;
        Course course;
        course.name = valueText(row, nameColumn);
        course.teacher = valueText(row, teacherColumn);
        course.room = valueText(row, roomColumn);
        course.weekday = parseWeekday(valueText(row, dayColumn));
        course.startTime = startTimeColumn < row.size() ? parseTime(row.at(startTimeColumn)) : QTime();
        course.endTime = endTimeColumn < row.size() ? parseTime(row.at(endTimeColumn)) : QTime();
        bool startWeekOk = false;
        bool endWeekOk = false;
        course.startWeek = startWeekColumn < row.size() ? parseInteger(row.at(startWeekColumn), &startWeekOk) : 0;
        course.endWeek = endWeekColumn < row.size() ? parseInteger(row.at(endWeekColumn), &endWeekOk) : 0;
        bool patternOk = false;
        course.weekPattern = parsePattern(valueText(row, patternColumn), &patternOk);
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
                                                                     int totalWeeks)
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
            result = parseRows(rowsFromExcelValue(range->property("Value2")), totalWeeks);
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
