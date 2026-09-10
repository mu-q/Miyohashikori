#include "schedulerepository.h"

#include "databasemanager.h"
#include "sqlhelpers.h"

#include <QSqlError>
#include <QSqlQuery>

namespace {

QString unavailableError()
{
    return QStringLiteral("课程表数据库尚未初始化");
}

Semester semesterFromQuery(const QSqlQuery &query)
{
    Semester semester;
    semester.id = query.value(QStringLiteral("id")).toLongLong();
    semester.name = query.value(QStringLiteral("name")).toString();
    semester.startDate = QDate::fromString(query.value(QStringLiteral("start_date")).toString(),
                                           Qt::ISODate);
    semester.totalWeeks = query.value(QStringLiteral("total_weeks")).toInt();
    semester.active = query.value(QStringLiteral("is_active")).toBool();
    semester.createdAt = SqlHelpers::fromUtcText(query.value(QStringLiteral("created_at")).toString());
    semester.updatedAt = SqlHelpers::fromUtcText(query.value(QStringLiteral("updated_at")).toString());
    return semester;
}

Course courseFromQuery(const QSqlQuery &query)
{
    Course course;
    course.id = query.value(QStringLiteral("id")).toLongLong();
    course.semesterId = query.value(QStringLiteral("semester_id")).toLongLong();
    course.name = query.value(QStringLiteral("name")).toString();
    course.teacher = query.value(QStringLiteral("teacher")).toString();
    course.room = query.value(QStringLiteral("room")).toString();
    course.weekday = query.value(QStringLiteral("weekday")).toInt();
    course.startTime = QTime::fromString(query.value(QStringLiteral("start_time")).toString(),
                                        QStringLiteral("HH:mm"));
    course.endTime = QTime::fromString(query.value(QStringLiteral("end_time")).toString(),
                                      QStringLiteral("HH:mm"));
    course.startWeek = query.value(QStringLiteral("start_week")).toInt();
    course.endWeek = query.value(QStringLiteral("end_week")).toInt();
    course.weekPattern = static_cast<CourseWeekPattern>(query.value(QStringLiteral("week_pattern")).toInt());
    course.createdAt = SqlHelpers::fromUtcText(query.value(QStringLiteral("created_at")).toString());
    course.updatedAt = SqlHelpers::fromUtcText(query.value(QStringLiteral("updated_at")).toString());
    return course;
}

bool execActiveUpdate(QSqlDatabase database, qint64 id, QString *error)
{
    QSqlQuery clear(database);
    if (!clear.exec(QStringLiteral("UPDATE semesters SET is_active = 0 WHERE is_active = 1"))) {
        *error = SqlHelpers::queryError(QStringLiteral("切换学期失败"), clear);
        return false;
    }
    QSqlQuery activate(database);
    activate.prepare(QStringLiteral("UPDATE semesters SET is_active = 1, updated_at = :now WHERE id = :id"));
    activate.bindValue(QStringLiteral(":now"), SqlHelpers::utcNowText());
    activate.bindValue(QStringLiteral(":id"), id);
    if (!activate.exec() || activate.numRowsAffected() != 1) {
        *error = activate.lastError().isValid()
                     ? SqlHelpers::queryError(QStringLiteral("切换学期失败"), activate)
                     : QStringLiteral("要启用的学期不存在");
        return false;
    }
    return true;
}

} // namespace

ScheduleRepository::ScheduleRepository(DatabaseManager *databaseManager)
    : databaseManager_(databaseManager)
{
}

DataResult<Semester> ScheduleRepository::createSemester(const QString &name, const QDate &startDate,
                                                         int totalWeeks, bool makeActive)
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<Semester>::fail(unavailableError());
    const QString validation = validateSemester(name, startDate, totalWeeks);
    if (!validation.isEmpty())
        return DataResult<Semester>::fail(validation);

    QSqlDatabase database = databaseManager_->database();
    if (!database.transaction())
        return DataResult<Semester>::fail(QStringLiteral("无法开始创建学期事务：%1").arg(database.lastError().text()));
    if (makeActive) {
        QSqlQuery clear(database);
        if (!clear.exec(QStringLiteral("UPDATE semesters SET is_active = 0 WHERE is_active = 1"))) {
            database.rollback();
            return DataResult<Semester>::fail(SqlHelpers::queryError(QStringLiteral("创建学期失败"), clear));
        }
    }
    const QString now = SqlHelpers::utcNowText();
    QSqlQuery query(database);
    query.prepare(QStringLiteral("INSERT INTO semesters "
                                 "(name, start_date, total_weeks, is_active, created_at, updated_at) "
                                 "VALUES (:name, :start_date, :total_weeks, :active, :now, :now)"));
    query.bindValue(QStringLiteral(":name"), name.trimmed());
    query.bindValue(QStringLiteral(":start_date"), startDate.toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":total_weeks"), totalWeeks);
    query.bindValue(QStringLiteral(":active"), makeActive ? 1 : 0);
    query.bindValue(QStringLiteral(":now"), now);
    if (!query.exec()) {
        database.rollback();
        return DataResult<Semester>::fail(SqlHelpers::queryError(QStringLiteral("创建学期失败"), query));
    }
    const qint64 id = query.lastInsertId().toLongLong();
    if (!database.commit())
        return DataResult<Semester>::fail(QStringLiteral("提交学期失败：%1").arg(database.lastError().text()));

    Semester semester;
    semester.id = id;
    semester.name = name.trimmed();
    semester.startDate = startDate;
    semester.totalWeeks = totalWeeks;
    semester.active = makeActive;
    semester.createdAt = SqlHelpers::fromUtcText(now);
    semester.updatedAt = semester.createdAt;
    return DataResult<Semester>::ok(semester);
}

DataResult<bool> ScheduleRepository::updateSemester(const Semester &semester)
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<bool>::fail(unavailableError());
    const QString validation = validateSemester(semester.name, semester.startDate, semester.totalWeeks);
    if (!validation.isEmpty())
        return DataResult<bool>::fail(validation);
    QSqlQuery query(databaseManager_->database());
    query.prepare(QStringLiteral("UPDATE semesters SET name=:name, start_date=:start_date, "
                                 "total_weeks=:weeks, updated_at=:now WHERE id=:id"));
    query.bindValue(QStringLiteral(":name"), semester.name.trimmed());
    query.bindValue(QStringLiteral(":start_date"), semester.startDate.toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":weeks"), semester.totalWeeks);
    query.bindValue(QStringLiteral(":now"), SqlHelpers::utcNowText());
    query.bindValue(QStringLiteral(":id"), semester.id);
    if (!query.exec())
        return DataResult<bool>::fail(SqlHelpers::queryError(QStringLiteral("更新学期失败"), query));
    return DataResult<bool>::ok(query.numRowsAffected() > 0);
}

DataResult<bool> ScheduleRepository::setActiveSemester(qint64 id)
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<bool>::fail(unavailableError());
    QSqlDatabase database = databaseManager_->database();
    if (!database.transaction())
        return DataResult<bool>::fail(QStringLiteral("无法开始切换学期事务"));
    QString error;
    if (!execActiveUpdate(database, id, &error)) {
        database.rollback();
        return DataResult<bool>::fail(error);
    }
    if (!database.commit())
        return DataResult<bool>::fail(QStringLiteral("提交学期切换失败：%1").arg(database.lastError().text()));
    return DataResult<bool>::ok(true);
}

DataResult<bool> ScheduleRepository::removeSemester(qint64 id)
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<bool>::fail(unavailableError());
    QSqlDatabase database = databaseManager_->database();
    if (!database.transaction())
        return DataResult<bool>::fail(QStringLiteral("无法开始删除学期事务"));
    QSqlQuery activeQuery(database);
    activeQuery.prepare(QStringLiteral("SELECT is_active FROM semesters WHERE id=:id"));
    activeQuery.bindValue(QStringLiteral(":id"), id);
    if (!activeQuery.exec() || !activeQuery.next()) {
        database.rollback();
        return DataResult<bool>::ok(false);
    }
    const bool wasActive = activeQuery.value(0).toBool();
    activeQuery.finish();
    QSqlQuery query(database);
    query.prepare(QStringLiteral("DELETE FROM semesters WHERE id=:id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        database.rollback();
        return DataResult<bool>::fail(SqlHelpers::queryError(QStringLiteral("删除学期失败"), query));
    }
    if (wasActive) {
        QSqlQuery next(database);
        next.prepare(QStringLiteral("UPDATE semesters SET is_active=1, updated_at=:now "
                                    "WHERE id=(SELECT id FROM semesters ORDER BY start_date DESC,id DESC LIMIT 1)"));
        next.bindValue(QStringLiteral(":now"), SqlHelpers::utcNowText());
        if (!next.exec()) {
            database.rollback();
            return DataResult<bool>::fail(SqlHelpers::queryError(QStringLiteral("启用后续学期失败"), next));
        }
    }
    if (!database.commit())
        return DataResult<bool>::fail(QStringLiteral("提交删除学期失败：%1").arg(database.lastError().text()));
    return DataResult<bool>::ok(true);
}

DataResult<QVector<Semester>> ScheduleRepository::listSemesters() const
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<QVector<Semester>>::fail(unavailableError());
    QSqlQuery query(databaseManager_->database());
    if (!query.exec(QStringLiteral("SELECT id,name,start_date,total_weeks,is_active,created_at,updated_at "
                                   "FROM semesters ORDER BY is_active DESC,start_date DESC,id DESC")))
        return DataResult<QVector<Semester>>::fail(SqlHelpers::queryError(QStringLiteral("读取学期失败"), query));
    QVector<Semester> result;
    while (query.next())
        result.append(semesterFromQuery(query));
    return DataResult<QVector<Semester>>::ok(result);
}

DataResult<std::optional<Semester>> ScheduleRepository::activeSemester() const
{
    const auto semesters = listSemesters();
    if (!semesters.success)
        return DataResult<std::optional<Semester>>::fail(semesters.error);
    for (const Semester &semester : semesters.value) {
        if (semester.active)
            return DataResult<std::optional<Semester>>::ok(semester);
    }
    return DataResult<std::optional<Semester>>::ok(std::nullopt);
}

DataResult<Course> ScheduleRepository::createCourse(const Course &course)
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<Course>::fail(unavailableError());
    const QString validation = validateCourse(course);
    if (!validation.isEmpty())
        return DataResult<Course>::fail(validation);
    const QString now = SqlHelpers::utcNowText();
    QSqlQuery query(databaseManager_->database());
    query.prepare(QStringLiteral("INSERT INTO courses "
        "(semester_id,name,teacher,room,weekday,start_time,end_time,start_week,end_week,week_pattern,created_at,updated_at) "
        "VALUES (:semester,:name,:teacher,:room,:weekday,:start,:end,:start_week,:end_week,:pattern,:now,:now)"));
    query.bindValue(QStringLiteral(":semester"), course.semesterId);
    query.bindValue(QStringLiteral(":name"), course.name.trimmed());
    query.bindValue(QStringLiteral(":teacher"), SqlHelpers::nonNullText(course.teacher));
    query.bindValue(QStringLiteral(":room"), SqlHelpers::nonNullText(course.room));
    query.bindValue(QStringLiteral(":weekday"), course.weekday);
    query.bindValue(QStringLiteral(":start"), course.startTime.toString(QStringLiteral("HH:mm")));
    query.bindValue(QStringLiteral(":end"), course.endTime.toString(QStringLiteral("HH:mm")));
    query.bindValue(QStringLiteral(":start_week"), course.startWeek);
    query.bindValue(QStringLiteral(":end_week"), course.endWeek);
    query.bindValue(QStringLiteral(":pattern"), static_cast<int>(course.weekPattern));
    query.bindValue(QStringLiteral(":now"), now);
    if (!query.exec())
        return DataResult<Course>::fail(SqlHelpers::queryError(QStringLiteral("添加课程失败"), query));
    Course created = course;
    created.id = query.lastInsertId().toLongLong();
    created.name = course.name.trimmed();
    created.createdAt = SqlHelpers::fromUtcText(now);
    created.updatedAt = created.createdAt;
    return DataResult<Course>::ok(created);
}

DataResult<bool> ScheduleRepository::updateCourse(const Course &course)
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<bool>::fail(unavailableError());
    const QString validation = validateCourse(course);
    if (!validation.isEmpty())
        return DataResult<bool>::fail(validation);
    QSqlQuery query(databaseManager_->database());
    query.prepare(QStringLiteral("UPDATE courses SET name=:name,teacher=:teacher,room=:room,weekday=:weekday,"
        "start_time=:start,end_time=:end,start_week=:start_week,end_week=:end_week,week_pattern=:pattern,"
        "updated_at=:now WHERE id=:id AND semester_id=:semester"));
    query.bindValue(QStringLiteral(":name"), course.name.trimmed());
    query.bindValue(QStringLiteral(":teacher"), SqlHelpers::nonNullText(course.teacher));
    query.bindValue(QStringLiteral(":room"), SqlHelpers::nonNullText(course.room));
    query.bindValue(QStringLiteral(":weekday"), course.weekday);
    query.bindValue(QStringLiteral(":start"), course.startTime.toString(QStringLiteral("HH:mm")));
    query.bindValue(QStringLiteral(":end"), course.endTime.toString(QStringLiteral("HH:mm")));
    query.bindValue(QStringLiteral(":start_week"), course.startWeek);
    query.bindValue(QStringLiteral(":end_week"), course.endWeek);
    query.bindValue(QStringLiteral(":pattern"), static_cast<int>(course.weekPattern));
    query.bindValue(QStringLiteral(":now"), SqlHelpers::utcNowText());
    query.bindValue(QStringLiteral(":id"), course.id);
    query.bindValue(QStringLiteral(":semester"), course.semesterId);
    if (!query.exec())
        return DataResult<bool>::fail(SqlHelpers::queryError(QStringLiteral("更新课程失败"), query));
    return DataResult<bool>::ok(query.numRowsAffected() > 0);
}

DataResult<bool> ScheduleRepository::removeCourse(qint64 id)
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<bool>::fail(unavailableError());
    QSqlQuery query(databaseManager_->database());
    query.prepare(QStringLiteral("DELETE FROM courses WHERE id=:id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec())
        return DataResult<bool>::fail(SqlHelpers::queryError(QStringLiteral("删除课程失败"), query));
    return DataResult<bool>::ok(query.numRowsAffected() > 0);
}

DataResult<QVector<Course>> ScheduleRepository::listCourses(qint64 semesterId) const
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<QVector<Course>>::fail(unavailableError());
    QSqlQuery query(databaseManager_->database());
    query.prepare(QStringLiteral("SELECT id,semester_id,name,teacher,room,weekday,start_time,end_time,"
                                 "start_week,end_week,week_pattern,created_at,updated_at FROM courses "
                                 "WHERE semester_id=:semester ORDER BY weekday,start_time,name,id"));
    query.bindValue(QStringLiteral(":semester"), semesterId);
    if (!query.exec())
        return DataResult<QVector<Course>>::fail(SqlHelpers::queryError(QStringLiteral("读取课程失败"), query));
    QVector<Course> courses;
    while (query.next())
        courses.append(courseFromQuery(query));
    return DataResult<QVector<Course>>::ok(courses);
}

DataResult<Semester> ScheduleRepository::importSchedule(const ImportedSchedule &schedule)
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<Semester>::fail(unavailableError());
    const QString semesterValidation = validateSemester(schedule.name, schedule.startDate, schedule.totalWeeks);
    if (!semesterValidation.isEmpty())
        return DataResult<Semester>::fail(semesterValidation);
    for (const Course &course : schedule.courses) {
        Course candidate = course;
        candidate.semesterId = 1;
        const QString courseValidation = validateCourse(candidate);
        if (!courseValidation.isEmpty())
            return DataResult<Semester>::fail(courseValidation);
    }

    QSqlDatabase database = databaseManager_->database();
    if (!database.transaction())
        return DataResult<Semester>::fail(QStringLiteral("无法开始导入事务：%1").arg(database.lastError().text()));
    QSqlQuery clear(database);
    if (!clear.exec(QStringLiteral("UPDATE semesters SET is_active=0 WHERE is_active=1"))) {
        database.rollback();
        return DataResult<Semester>::fail(SqlHelpers::queryError(QStringLiteral("导入课表失败"), clear));
    }
    const QString now = SqlHelpers::utcNowText();
    QSqlQuery semesterQuery(database);
    semesterQuery.prepare(QStringLiteral("INSERT INTO semesters(name,start_date,total_weeks,is_active,created_at,updated_at) "
                                         "VALUES(:name,:date,:weeks,1,:now,:now)"));
    semesterQuery.bindValue(QStringLiteral(":name"), schedule.name.trimmed());
    semesterQuery.bindValue(QStringLiteral(":date"), schedule.startDate.toString(Qt::ISODate));
    semesterQuery.bindValue(QStringLiteral(":weeks"), schedule.totalWeeks);
    semesterQuery.bindValue(QStringLiteral(":now"), now);
    if (!semesterQuery.exec()) {
        database.rollback();
        return DataResult<Semester>::fail(SqlHelpers::queryError(QStringLiteral("导入学期失败"), semesterQuery));
    }
    const qint64 semesterId = semesterQuery.lastInsertId().toLongLong();
    QSqlQuery courseQuery(database);
    courseQuery.prepare(QStringLiteral("INSERT INTO courses "
        "(semester_id,name,teacher,room,weekday,start_time,end_time,start_week,end_week,week_pattern,created_at,updated_at) "
        "VALUES(:semester,:name,:teacher,:room,:weekday,:start,:end,:start_week,:end_week,:pattern,:now,:now)"));
    for (const Course &course : schedule.courses) {
        courseQuery.bindValue(QStringLiteral(":semester"), semesterId);
        courseQuery.bindValue(QStringLiteral(":name"), course.name.trimmed());
        courseQuery.bindValue(QStringLiteral(":teacher"), SqlHelpers::nonNullText(course.teacher));
        courseQuery.bindValue(QStringLiteral(":room"), SqlHelpers::nonNullText(course.room));
        courseQuery.bindValue(QStringLiteral(":weekday"), course.weekday);
        courseQuery.bindValue(QStringLiteral(":start"), course.startTime.toString(QStringLiteral("HH:mm")));
        courseQuery.bindValue(QStringLiteral(":end"), course.endTime.toString(QStringLiteral("HH:mm")));
        courseQuery.bindValue(QStringLiteral(":start_week"), course.startWeek);
        courseQuery.bindValue(QStringLiteral(":end_week"), course.endWeek);
        courseQuery.bindValue(QStringLiteral(":pattern"), static_cast<int>(course.weekPattern));
        courseQuery.bindValue(QStringLiteral(":now"), now);
        if (!courseQuery.exec()) {
            database.rollback();
            return DataResult<Semester>::fail(SqlHelpers::queryError(QStringLiteral("导入课程失败"), courseQuery));
        }
    }
    if (!database.commit())
        return DataResult<Semester>::fail(QStringLiteral("提交课表导入失败：%1").arg(database.lastError().text()));
    Semester semester;
    semester.id = semesterId;
    semester.name = schedule.name.trimmed();
    semester.startDate = schedule.startDate;
    semester.totalWeeks = schedule.totalWeeks;
    semester.active = true;
    semester.createdAt = SqlHelpers::fromUtcText(now);
    semester.updatedAt = semester.createdAt;
    return DataResult<Semester>::ok(semester);
}

DataResult<bool> ScheduleRepository::markReminderSent(qint64 courseId, const QDate &date, int leadMinutes)
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<bool>::fail(unavailableError());
    if (!date.isValid() || (leadMinutes != 20 && leadMinutes != 30))
        return DataResult<bool>::fail(QStringLiteral("课程提醒参数无效"));
    QSqlQuery query(databaseManager_->database());
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO course_reminders "
                                 "(course_id,occurrence_date,lead_minutes,reminded_at) "
                                 "VALUES(:course,:date,:lead,:now)"));
    query.bindValue(QStringLiteral(":course"), courseId);
    query.bindValue(QStringLiteral(":date"), date.toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":lead"), leadMinutes);
    query.bindValue(QStringLiteral(":now"), SqlHelpers::utcNowText());
    if (!query.exec())
        return DataResult<bool>::fail(SqlHelpers::queryError(QStringLiteral("记录课程提醒失败"), query));
    return DataResult<bool>::ok(query.numRowsAffected() > 0);
}

QString ScheduleRepository::validateSemester(const QString &name, const QDate &startDate, int totalWeeks) const
{
    if (name.trimmed().isEmpty())
        return QStringLiteral("学期名称不能为空");
    if (!startDate.isValid())
        return QStringLiteral("开学日期无效");
    if (totalWeeks < 1 || totalWeeks > 40)
        return QStringLiteral("学期周数必须在 1 到 40 之间");
    return {};
}

QString ScheduleRepository::validateCourse(const Course &course) const
{
    if (course.semesterId < 1)
        return QStringLiteral("课程没有所属学期");
    if (course.name.trimmed().isEmpty())
        return QStringLiteral("课程名称不能为空");
    if (course.weekday < 1 || course.weekday > 7)
        return QStringLiteral("课程星期无效");
    if (!course.startTime.isValid() || !course.endTime.isValid() || course.startTime >= course.endTime)
        return QStringLiteral("课程起止时间无效");
    if (course.startWeek < 1 || course.endWeek < course.startWeek || course.endWeek > 40)
        return QStringLiteral("课程周次范围无效");
    const int pattern = static_cast<int>(course.weekPattern);
    if (pattern < 0 || pattern > 2)
        return QStringLiteral("课程单双周类型无效");
    return {};
}
