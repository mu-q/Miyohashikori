#include "todorepository.h"

#include "databasemanager.h"
#include "sqlhelpers.h"

#include <QSqlQuery>
#include <QVariant>

namespace {

TodoItem todoFromQuery(const QSqlQuery &query)
{
    TodoItem item;
    item.id = query.value(QStringLiteral("id")).toLongLong();
    item.title = query.value(QStringLiteral("title")).toString();
    item.description = query.value(QStringLiteral("description")).toString();
    item.status = static_cast<TodoStatus>(query.value(QStringLiteral("status")).toInt());
    item.priority = static_cast<TodoPriority>(query.value(QStringLiteral("priority")).toInt());
    item.dueAt = SqlHelpers::fromUtcText(query.value(QStringLiteral("due_at")).toString());
    item.completedAt = SqlHelpers::fromUtcText(
        query.value(QStringLiteral("completed_at")).toString());
    item.createdAt = SqlHelpers::fromUtcText(query.value(QStringLiteral("created_at")).toString());
    item.updatedAt = SqlHelpers::fromUtcText(query.value(QStringLiteral("updated_at")).toString());
    return item;
}

QString unavailableError()
{
    return QStringLiteral("待办数据库尚未初始化");
}

QVariant nullableDateTime(const QDateTime &dateTime)
{
    if (!dateTime.isValid())
        return {};
    return SqlHelpers::toUtcText(dateTime);
}

} // namespace

TodoRepository::TodoRepository(DatabaseManager *databaseManager)
    : databaseManager_(databaseManager)
{
}

DataResult<TodoItem> TodoRepository::create(const QString &title, const QString &description,
                                            TodoPriority priority, const QDateTime &dueAt,
                                            TodoStatus status)
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<TodoItem>::fail(unavailableError());
    const QString validationError = validate(title, status, priority);
    if (!validationError.isEmpty())
        return DataResult<TodoItem>::fail(validationError);

    const QString now = SqlHelpers::utcNowText();
    QSqlQuery query(databaseManager_->database());
    query.prepare(QStringLiteral("INSERT INTO todos "
                                 "(title, description, status, priority, due_at, completed_at, "
                                 "created_at, updated_at) VALUES "
                                 "(:title, :description, :status, :priority, :due_at, "
                                 ":completed_at, :created_at, :updated_at)"));
    query.bindValue(QStringLiteral(":title"), title.trimmed());
    query.bindValue(QStringLiteral(":description"), SqlHelpers::nonNullText(description));
    query.bindValue(QStringLiteral(":status"), static_cast<int>(status));
    query.bindValue(QStringLiteral(":priority"), static_cast<int>(priority));
    query.bindValue(QStringLiteral(":due_at"), nullableDateTime(dueAt));
    query.bindValue(QStringLiteral(":completed_at"),
                    status == TodoStatus::Done ? QVariant(now) : QVariant());
    query.bindValue(QStringLiteral(":created_at"), now);
    query.bindValue(QStringLiteral(":updated_at"), now);
    if (!query.exec()) {
        return DataResult<TodoItem>::fail(
            SqlHelpers::queryError(QStringLiteral("创建待办失败"), query));
    }

    TodoItem item;
    item.id = query.lastInsertId().toLongLong();
    item.title = title.trimmed();
    item.description = SqlHelpers::nonNullText(description);
    item.status = status;
    item.priority = priority;
    item.dueAt = dueAt.isValid() ? dueAt.toUTC() : QDateTime();
    item.completedAt = status == TodoStatus::Done ? SqlHelpers::fromUtcText(now) : QDateTime();
    item.createdAt = SqlHelpers::fromUtcText(now);
    item.updatedAt = item.createdAt;
    return DataResult<TodoItem>::ok(item);
}

DataResult<std::optional<TodoItem>> TodoRepository::findById(qint64 id) const
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<std::optional<TodoItem>>::fail(unavailableError());

    QSqlQuery query(databaseManager_->database());
    query.prepare(QStringLiteral("SELECT id, title, description, status, priority, due_at, "
                                 "completed_at, created_at, updated_at FROM todos WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        return DataResult<std::optional<TodoItem>>::fail(
            SqlHelpers::queryError(QStringLiteral("读取待办失败"), query));
    }
    if (!query.next())
        return DataResult<std::optional<TodoItem>>::ok(std::nullopt);
    return DataResult<std::optional<TodoItem>>::ok(todoFromQuery(query));
}

DataResult<bool> TodoRepository::update(const TodoItem &item)
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<bool>::fail(unavailableError());
    const QString validationError = validate(item.title, item.status, item.priority);
    if (!validationError.isEmpty())
        return DataResult<bool>::fail(validationError);

    QSqlQuery query(databaseManager_->database());
    query.prepare(QStringLiteral("UPDATE todos SET title = :title, description = :description, "
                                 "status = :status, priority = :priority, due_at = :due_at, "
                                 "completed_at = CASE WHEN :status = 2 "
                                 "THEN COALESCE(completed_at, :completed_now) ELSE NULL END, "
                                 "updated_at = :updated_at WHERE id = :id"));
    query.bindValue(QStringLiteral(":title"), item.title.trimmed());
    query.bindValue(QStringLiteral(":description"), SqlHelpers::nonNullText(item.description));
    query.bindValue(QStringLiteral(":status"), static_cast<int>(item.status));
    query.bindValue(QStringLiteral(":priority"), static_cast<int>(item.priority));
    query.bindValue(QStringLiteral(":due_at"), nullableDateTime(item.dueAt));
    const QString now = SqlHelpers::utcNowText();
    query.bindValue(QStringLiteral(":completed_now"), now);
    query.bindValue(QStringLiteral(":updated_at"), now);
    query.bindValue(QStringLiteral(":id"), item.id);
    if (!query.exec())
        return DataResult<bool>::fail(SqlHelpers::queryError(QStringLiteral("更新待办失败"), query));
    return DataResult<bool>::ok(query.numRowsAffected() > 0);
}

DataResult<bool> TodoRepository::remove(qint64 id)
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<bool>::fail(unavailableError());

    QSqlQuery query(databaseManager_->database());
    query.prepare(QStringLiteral("DELETE FROM todos WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec())
        return DataResult<bool>::fail(SqlHelpers::queryError(QStringLiteral("删除待办失败"), query));
    return DataResult<bool>::ok(query.numRowsAffected() > 0);
}

DataResult<bool> TodoRepository::setStatus(qint64 id, TodoStatus status)
{
    const QString validationError = validate(QStringLiteral("placeholder"), status,
                                             TodoPriority::Normal);
    if (!validationError.isEmpty())
        return DataResult<bool>::fail(validationError);
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<bool>::fail(unavailableError());

    const QString now = SqlHelpers::utcNowText();
    QSqlQuery query(databaseManager_->database());
    query.prepare(QStringLiteral("UPDATE todos SET status = :status, "
                                 "completed_at = CASE WHEN :status = 2 "
                                 "THEN COALESCE(completed_at, :completed_now) ELSE NULL END, "
                                 "updated_at = :updated_at WHERE id = :id"));
    query.bindValue(QStringLiteral(":status"), static_cast<int>(status));
    query.bindValue(QStringLiteral(":completed_now"), now);
    query.bindValue(QStringLiteral(":updated_at"), now);
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        return DataResult<bool>::fail(
            SqlHelpers::queryError(QStringLiteral("更新待办状态失败"), query));
    }
    return DataResult<bool>::ok(query.numRowsAffected() > 0);
}

DataResult<QVector<TodoItem>> TodoRepository::list(const TodoFilter &filter) const
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<QVector<TodoItem>>::fail(unavailableError());
    if (filter.status.has_value()) {
        const int value = static_cast<int>(*filter.status);
        if (value < 0 || value > 2)
            return DataResult<QVector<TodoItem>>::fail(QStringLiteral("待办状态无效"));
    }
    if (filter.priority.has_value()) {
        const int value = static_cast<int>(*filter.priority);
        if (value < 0 || value > 2)
            return DataResult<QVector<TodoItem>>::fail(QStringLiteral("待办优先级无效"));
    }

    QString sql = QStringLiteral("SELECT id, title, description, status, priority, due_at, "
                                 "completed_at, created_at, updated_at FROM todos WHERE 1 = 1");
    QVariantList bindings;
    if (filter.status.has_value()) {
        sql += QStringLiteral(" AND status = ?");
        bindings.append(static_cast<int>(*filter.status));
    }
    if (filter.priority.has_value()) {
        sql += QStringLiteral(" AND priority = ?");
        bindings.append(static_cast<int>(*filter.priority));
    }
    if (filter.dueBefore.isValid()) {
        sql += QStringLiteral(" AND due_at IS NOT NULL AND due_at <= ?");
        bindings.append(SqlHelpers::toUtcText(filter.dueBefore));
    }
    sql += QStringLiteral(" ORDER BY status ASC, due_at IS NULL ASC, due_at ASC, "
                          "priority DESC, updated_at DESC, id DESC");

    QSqlQuery query(databaseManager_->database());
    query.prepare(sql);
    for (const QVariant &binding : bindings)
        query.addBindValue(binding);
    if (!query.exec()) {
        return DataResult<QVector<TodoItem>>::fail(
            SqlHelpers::queryError(QStringLiteral("列出待办失败"), query));
    }

    QVector<TodoItem> items;
    while (query.next())
        items.append(todoFromQuery(query));
    return DataResult<QVector<TodoItem>>::ok(items);
}

QString TodoRepository::validate(const QString &title, TodoStatus status, TodoPriority priority)
{
    if (title.trimmed().isEmpty())
        return QStringLiteral("待办标题不能为空");
    const int statusValue = static_cast<int>(status);
    if (statusValue < 0 || statusValue > 2)
        return QStringLiteral("待办状态无效");
    const int priorityValue = static_cast<int>(priority);
    if (priorityValue < 0 || priorityValue > 2)
        return QStringLiteral("待办优先级无效");
    return {};
}
