#pragma once

#include "../data/models.h"

#include <QObject>

class QTimer;
class ScheduleRepository;

class CourseReminderController : public QObject
{
    Q_OBJECT
public:
    explicit CourseReminderController(ScheduleRepository *repository, QObject *parent = nullptr);

    void start();
    void checkAt(const QDateTime &localNow);

    static bool occursInWeek(const Course &course, int week);
    static int weekForDate(const Semester &semester, const QDate &date);

signals:
    void reminderDue(const CourseOccurrence &occurrence, int leadMinutes,
                     const QString &displayText, const QString &speechText);
    void reminderError(const QString &message);

private:
    ScheduleRepository *repository_ = nullptr;
    QTimer *timer_ = nullptr;
};
