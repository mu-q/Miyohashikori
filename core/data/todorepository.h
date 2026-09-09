#pragma once

#include "dataresult.h"
#include "models.h"

#include <QVector>

#include <optional>

class DatabaseManager;

class TodoRepository
{
public:
    explicit TodoRepository(DatabaseManager *databaseManager);

    DataResult<TodoItem> create(const QString &title, const QString &description = {},
                                TodoPriority priority = TodoPriority::Normal,
                                const QDateTime &dueAt = {},
                                TodoStatus status = TodoStatus::Pending);
    DataResult<std::optional<TodoItem>> findById(qint64 id) const;
    DataResult<bool> update(const TodoItem &item);
    DataResult<bool> remove(qint64 id);
    DataResult<bool> setStatus(qint64 id, TodoStatus status);
    DataResult<QVector<TodoItem>> list(const TodoFilter &filter = {}) const;

private:
    static QString validate(const QString &title, TodoStatus status, TodoPriority priority);

    DatabaseManager *databaseManager_ = nullptr;
};
