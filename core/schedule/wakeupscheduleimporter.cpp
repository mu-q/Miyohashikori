#include "wakeupscheduleimporter.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>

namespace {

DataResult<QVector<QJsonDocument>> splitDocuments(const QByteArray &input)
{
    QVector<QJsonDocument> documents;
    int start = -1;
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (int index = 0; index < input.size(); ++index) {
        const char ch = input.at(index);
        if (start < 0) {
            if (ch == '{' || ch == '[') {
                start = index;
                depth = 1;
            }
            continue;
        }
        if (inString) {
            if (escaped)
                escaped = false;
            else if (ch == '\\')
                escaped = true;
            else if (ch == '"')
                inString = false;
            continue;
        }
        if (ch == '"') {
            inString = true;
        } else if (ch == '{' || ch == '[') {
            ++depth;
        } else if (ch == '}' || ch == ']') {
            --depth;
            if (depth == 0) {
                QJsonParseError error;
                const QJsonDocument document = QJsonDocument::fromJson(input.mid(start, index - start + 1), &error);
                if (error.error != QJsonParseError::NoError)
                    return DataResult<QVector<QJsonDocument>>::fail(
                        QStringLiteral("WakeUp 备份 JSON 无效：%1").arg(error.errorString()));
                documents.append(document);
                start = -1;
            }
        }
    }
    if (start >= 0 || inString)
        return DataResult<QVector<QJsonDocument>>::fail(QStringLiteral("WakeUp 备份内容不完整"));
    return DataResult<QVector<QJsonDocument>>::ok(documents);
}

QString requiredString(const QJsonObject &object, const QString &name)
{
    return object.value(name).toString().trimmed();
}

} // namespace

DataResult<ImportedSchedule> WakeUpScheduleImporter::parseFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return DataResult<ImportedSchedule>::fail(
            QStringLiteral("无法读取 WakeUp 课表：%1").arg(file.errorString()));
    return parse(file.readAll());
}

DataResult<ImportedSchedule> WakeUpScheduleImporter::parse(const QByteArray &data)
{
    const auto parsed = splitDocuments(data);
    if (!parsed.success)
        return DataResult<ImportedSchedule>::fail(parsed.error);
    if (parsed.value.size() != 5 || !parsed.value.at(1).isArray()
        || !parsed.value.at(2).isObject() || !parsed.value.at(3).isArray()
        || !parsed.value.at(4).isArray()) {
        return DataResult<ImportedSchedule>::fail(
            QStringLiteral("不是受支持的 WakeUp 备份：应包含 5 段课表数据"));
    }

    const QJsonObject metadata = parsed.value.at(2).object();
    ImportedSchedule schedule;
    schedule.name = requiredString(metadata, QStringLiteral("tableName"));
    schedule.startDate = QDate::fromString(requiredString(metadata, QStringLiteral("startDate")),
                                           Qt::ISODate);
    schedule.totalWeeks = metadata.value(QStringLiteral("maxWeek")).toInt();
    if (schedule.name.isEmpty() || !schedule.startDate.isValid()
        || schedule.totalWeeks < 1 || schedule.totalWeeks > 40) {
        return DataResult<ImportedSchedule>::fail(QStringLiteral("WakeUp 备份的学期信息无效"));
    }

    QMap<int, QPair<QTime, QTime>> nodeTimes;
    for (const QJsonValue &value : parsed.value.at(1).array()) {
        const QJsonObject object = value.toObject();
        const int node = object.value(QStringLiteral("node")).toInt();
        const QTime start = QTime::fromString(requiredString(object, QStringLiteral("startTime")),
                                              QStringLiteral("H:mm"));
        const QTime end = QTime::fromString(requiredString(object, QStringLiteral("endTime")),
                                            QStringLiteral("H:mm"));
        if (node > 0 && start.isValid() && end.isValid())
            nodeTimes.insert(node, qMakePair(start, end));
    }

    QHash<int, QString> courseNames;
    for (const QJsonValue &value : parsed.value.at(3).array()) {
        const QJsonObject object = value.toObject();
        courseNames.insert(object.value(QStringLiteral("id")).toInt(),
                           requiredString(object, QStringLiteral("courseName")));
    }

    for (const QJsonValue &value : parsed.value.at(4).array()) {
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("ownTime")).toBool(false))
            continue;
        Course course;
        const int courseId = object.value(QStringLiteral("id")).toInt();
        const int startNode = object.value(QStringLiteral("startNode")).toInt();
        const int step = object.value(QStringLiteral("step")).toInt(1);
        const int endNode = startNode + qMax(1, step) - 1;
        course.name = courseNames.value(courseId).trimmed();
        course.teacher = requiredString(object, QStringLiteral("teacher"));
        course.room = requiredString(object, QStringLiteral("room"));
        course.weekday = object.value(QStringLiteral("day")).toInt();
        course.startWeek = object.value(QStringLiteral("startWeek")).toInt();
        course.endWeek = object.value(QStringLiteral("endWeek")).toInt();
        course.weekPattern = static_cast<CourseWeekPattern>(object.value(QStringLiteral("type")).toInt());
        if (!nodeTimes.contains(startNode) || !nodeTimes.contains(endNode)) {
            return DataResult<ImportedSchedule>::fail(
                QStringLiteral("课程“%1”的第 %2—%3 节没有对应时间")
                    .arg(course.name.isEmpty() ? QStringLiteral("未知课程") : course.name)
                    .arg(startNode).arg(endNode));
        }
        course.startTime = nodeTimes.value(startNode).first;
        course.endTime = nodeTimes.value(endNode).second;
        if (course.name.isEmpty() || course.weekday < 1 || course.weekday > 7
            || course.startWeek < 1 || course.endWeek < course.startWeek
            || course.endWeek > schedule.totalWeeks
            || static_cast<int>(course.weekPattern) < 0
            || static_cast<int>(course.weekPattern) > 2
            || course.startTime >= course.endTime) {
            return DataResult<ImportedSchedule>::fail(QStringLiteral("WakeUp 备份中存在无效课程记录"));
        }
        schedule.courses.append(course);
    }
    if (schedule.courses.isEmpty())
        return DataResult<ImportedSchedule>::fail(QStringLiteral("WakeUp 备份中没有可导入的课程"));
    return DataResult<ImportedSchedule>::ok(schedule);
}
