#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>

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
