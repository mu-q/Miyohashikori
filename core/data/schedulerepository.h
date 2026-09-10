#pragma once

#include "dataresult.h"
#include "models.h"

#include <QVector>

#include <optional>

class DatabaseManager;

class ScheduleRepository
{
public:
    explicit ScheduleRepository(DatabaseManager *databaseManager);

    DataResult<Semester> createSemester(const QString &name, const QDate &startDate,
                                        int totalWeeks, bool makeActive = true);
    DataResult<bool> updateSemester(const Semester &semester);
    DataResult<bool> setActiveSemester(qint64 id);
    DataResult<bool> removeSemester(qint64 id);
    DataResult<QVector<Semester>> listSemesters() const;
    DataResult<std::optional<Semester>> activeSemester() const;

    DataResult<Course> createCourse(const Course &course);
    DataResult<bool> updateCourse(const Course &course);
    DataResult<bool> removeCourse(qint64 id);
    DataResult<QVector<Course>> listCourses(qint64 semesterId) const;
    DataResult<Semester> importSchedule(const ImportedSchedule &schedule);

    DataResult<bool> markReminderSent(qint64 courseId, const QDate &date, int leadMinutes);

private:
    QString validateSemester(const QString &name, const QDate &startDate, int totalWeeks) const;
    QString validateCourse(const Course &course) const;

    DatabaseManager *databaseManager_ = nullptr;
};
