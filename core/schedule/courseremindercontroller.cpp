#include "courseremindercontroller.h"

#include "../data/schedulerepository.h"

#include <QTimer>
#include <QTimeZone>

CourseReminderController::CourseReminderController(ScheduleRepository *repository, QObject *parent)
    : QObject(parent), repository_(repository), timer_(new QTimer(this))
{
    timer_->setInterval(30000);
    connect(timer_, &QTimer::timeout, this, [this] { checkAt(QDateTime::currentDateTime()); });
}

void CourseReminderController::start()
{
    checkAt(QDateTime::currentDateTime());
    timer_->start();
}

void CourseReminderController::checkAt(const QDateTime &localNow)
{
    if (!repository_ || !localNow.isValid())
        return;
    const auto semesterResult = repository_->activeSemester();
    if (!semesterResult.success) {
        emit reminderError(semesterResult.error);
        return;
    }
    if (!semesterResult.value.has_value())
        return;
    const Semester semester = *semesterResult.value;
    const int week = weekForDate(semester, localNow.date());
    if (week < 1 || week > semester.totalWeeks)
        return;
    const auto coursesResult = repository_->listCourses(semester.id);
    if (!coursesResult.success) {
        emit reminderError(coursesResult.error);
        return;
    }
    for (const Course &course : coursesResult.value) {
        if (course.weekday != localNow.date().dayOfWeek() || !occursInWeek(course, week))
            continue;
        const QDateTime startsAt(localNow.date(), course.startTime, localNow.timeZone());
        const qint64 secondsUntil = localNow.secsTo(startsAt);
        for (const int leadMinutes : {30, 20}) {
            // 30 秒轮询、60 秒触发窗：避免边界抖动，并且不会在睡眠恢复后补播过期提醒。
            if (secondsUntil > leadMinutes * 60 || secondsUntil <= (leadMinutes - 1) * 60)
                continue;
            const auto marked = repository_->markReminderSent(course.id, localNow.date(), leadMinutes);
            if (!marked.success) {
                emit reminderError(marked.error);
                continue;
            }
            if (!marked.value)
                continue;
            CourseOccurrence occurrence;
            occurrence.course = course;
            occurrence.semester = semester;
            occurrence.date = localNow.date();
            occurrence.startsAt = startsAt;
            occurrence.endsAt = QDateTime(localNow.date(), course.endTime, localNow.timeZone());
            occurrence.week = week;
            const QString place = course.room.trimmed().isEmpty()
                                      ? QString()
                                      : QStringLiteral("，地点是%1").arg(course.room.trimmed());
            const QString display = QStringLiteral("九郎先生，距离“%1”上课还有 %2 分钟%3。该准备出发了。")
                                        .arg(course.name).arg(leadMinutes).arg(place);
            const QString speech = QStringLiteral("九郎さん、%1の授業まであと%2分です。そろそろ準備してください。")
                                       .arg(course.name).arg(leadMinutes);
            emit reminderDue(occurrence, leadMinutes, display, speech);
        }
    }
}

bool CourseReminderController::occursInWeek(const Course &course, int week)
{
    if (week < course.startWeek || week > course.endWeek)
        return false;
    if (course.weekPattern == CourseWeekPattern::OddWeeks)
        return week % 2 == 1;
    if (course.weekPattern == CourseWeekPattern::EvenWeeks)
        return week % 2 == 0;
    return true;
}

int CourseReminderController::weekForDate(const Semester &semester, const QDate &date)
{
    if (!semester.startDate.isValid() || !date.isValid())
        return 0;
    const qint64 days = semester.startDate.daysTo(date);
    return days < 0 ? 0 : static_cast<int>(days / 7) + 1;
}
