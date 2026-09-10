#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QTime>
#include <QVector>

#include <optional>

struct JournalEntry
{
    qint64 id = -1;
    QDate entryDate;
    QString title;
    QString body;
    QDateTime createdAt;
    QDateTime updatedAt;
};

struct Note
{
    qint64 id = -1;
    QString title;
    QString body;
    QStringList tags;
    QDateTime createdAt;
    QDateTime updatedAt;
};

enum class TodoStatus {
    Pending = 0,
    InProgress = 1,
    Done = 2
};

enum class TodoPriority {
    Low = 0,
    Normal = 1,
    High = 2
};

struct TodoItem
{
    qint64 id = -1;
    QString title;
    QString description;
    TodoStatus status = TodoStatus::Pending;
    TodoPriority priority = TodoPriority::Normal;
    QDateTime dueAt;
    QDateTime completedAt;
    QDateTime createdAt;
    QDateTime updatedAt;
};

struct TodoFilter
{
    std::optional<TodoStatus> status;
    std::optional<TodoPriority> priority;
    QDateTime dueBefore;
};

enum class CourseWeekPattern {
    EveryWeek = 0,
    OddWeeks = 1,
    EvenWeeks = 2
};

struct Semester
{
    qint64 id = -1;
    QString name;
    QDate startDate;
    int totalWeeks = 20;
    bool active = false;
    QDateTime createdAt;
    QDateTime updatedAt;
};

struct Course
{
    qint64 id = -1;
    qint64 semesterId = -1;
    QString name;
    QString teacher;
    QString room;
    int weekday = 1;
    QTime startTime;
    QTime endTime;
    int startWeek = 1;
    int endWeek = 20;
    CourseWeekPattern weekPattern = CourseWeekPattern::EveryWeek;
    QDateTime createdAt;
    QDateTime updatedAt;
};

struct CourseOccurrence
{
    Course course;
    Semester semester;
    QDate date;
    QDateTime startsAt;
    QDateTime endsAt;
    int week = 0;
};

struct ImportedSchedule
{
    QString name;
    QDate startDate;
    int totalWeeks = 20;
    QVector<Course> courses;
};
