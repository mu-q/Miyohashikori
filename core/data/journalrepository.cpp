#include "journalrepository.h"

#include "databasemanager.h"
#include "sqlhelpers.h"

#include <QSqlQuery>
#include <QVariant>

namespace {

JournalEntry journalFromQuery(const QSqlQuery &query)
{
    JournalEntry entry;
    entry.id = query.value(QStringLiteral("id")).toLongLong();
    entry.entryDate = QDate::fromString(query.value(QStringLiteral("entry_date")).toString(),
                                        Qt::ISODate);
    entry.title = query.value(QStringLiteral("title")).toString();
    entry.body = query.value(QStringLiteral("body")).toString();
    entry.createdAt = SqlHelpers::fromUtcText(query.value(QStringLiteral("created_at")).toString());
    entry.updatedAt = SqlHelpers::fromUtcText(query.value(QStringLiteral("updated_at")).toString());
    return entry;
}

QString unavailableError()
{
    return QStringLiteral("日记数据库尚未初始化");
}

} // namespace

JournalRepository::JournalRepository(DatabaseManager *databaseManager)
    : databaseManager_(databaseManager)
{
}

DataResult<JournalEntry> JournalRepository::create(const QDate &date, const QString &title,
                                                    const QString &body)
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<JournalEntry>::fail(unavailableError());
    if (!date.isValid())
        return DataResult<JournalEntry>::fail(QStringLiteral("日记日期无效"));

    QSqlQuery query(databaseManager_->database());
    query.prepare(QStringLiteral("INSERT INTO journal_entries "
                                 "(entry_date, title, body, created_at, updated_at) "
                                 "VALUES (:date, :title, :body, :created, :updated)"));
    const QString now = SqlHelpers::utcNowText();
    query.bindValue(QStringLiteral(":date"), date.toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":title"), SqlHelpers::nonNullText(title));
    query.bindValue(QStringLiteral(":body"), SqlHelpers::nonNullText(body));
    query.bindValue(QStringLiteral(":created"), now);
    query.bindValue(QStringLiteral(":updated"), now);
    if (!query.exec())
        return DataResult<JournalEntry>::fail(
            SqlHelpers::queryError(QStringLiteral("创建日记失败"), query));

    JournalEntry entry;
    entry.id = query.lastInsertId().toLongLong();
    entry.entryDate = date;
    entry.title = SqlHelpers::nonNullText(title);
    entry.body = SqlHelpers::nonNullText(body);
    entry.createdAt = SqlHelpers::fromUtcText(now);
    entry.updatedAt = entry.createdAt;
    return DataResult<JournalEntry>::ok(entry);
}

DataResult<std::optional<JournalEntry>> JournalRepository::findByDate(const QDate &date) const
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<std::optional<JournalEntry>>::fail(unavailableError());
    if (!date.isValid())
        return DataResult<std::optional<JournalEntry>>::fail(QStringLiteral("日记日期无效"));

    QSqlQuery query(databaseManager_->database());
    query.prepare(QStringLiteral("SELECT id, entry_date, title, body, created_at, updated_at "
                                 "FROM journal_entries WHERE entry_date = :date"));
    query.bindValue(QStringLiteral(":date"), date.toString(Qt::ISODate));
    if (!query.exec()) {
        return DataResult<std::optional<JournalEntry>>::fail(
            SqlHelpers::queryError(QStringLiteral("读取日记失败"), query));
    }
    if (!query.next())
        return DataResult<std::optional<JournalEntry>>::ok(std::nullopt);
    return DataResult<std::optional<JournalEntry>>::ok(journalFromQuery(query));
}

DataResult<bool> JournalRepository::update(qint64 id, const QString &title, const QString &body)
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<bool>::fail(unavailableError());

    QSqlQuery query(databaseManager_->database());
    query.prepare(QStringLiteral("UPDATE journal_entries SET title = :title, body = :body, "
                                 "updated_at = :updated WHERE id = :id"));
    query.bindValue(QStringLiteral(":title"), SqlHelpers::nonNullText(title));
    query.bindValue(QStringLiteral(":body"), SqlHelpers::nonNullText(body));
    query.bindValue(QStringLiteral(":updated"), SqlHelpers::utcNowText());
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec())
        return DataResult<bool>::fail(SqlHelpers::queryError(QStringLiteral("更新日记失败"), query));
    return DataResult<bool>::ok(query.numRowsAffected() > 0);
}

DataResult<bool> JournalRepository::remove(qint64 id)
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<bool>::fail(unavailableError());

    QSqlQuery query(databaseManager_->database());
    query.prepare(QStringLiteral("DELETE FROM journal_entries WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec())
        return DataResult<bool>::fail(SqlHelpers::queryError(QStringLiteral("删除日记失败"), query));
    return DataResult<bool>::ok(query.numRowsAffected() > 0);
}

DataResult<QVector<JournalEntry>> JournalRepository::list(const QDate &from,
                                                          const QDate &to) const
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<QVector<JournalEntry>>::fail(unavailableError());
    if (!from.isValid() || !to.isValid() || from > to)
        return DataResult<QVector<JournalEntry>>::fail(QStringLiteral("日记日期范围无效"));

    QSqlQuery query(databaseManager_->database());
    query.prepare(QStringLiteral("SELECT id, entry_date, title, body, created_at, updated_at "
                                 "FROM journal_entries WHERE entry_date BETWEEN :from AND :to "
                                 "ORDER BY entry_date DESC"));
    query.bindValue(QStringLiteral(":from"), from.toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":to"), to.toString(Qt::ISODate));
    if (!query.exec()) {
        return DataResult<QVector<JournalEntry>>::fail(
            SqlHelpers::queryError(QStringLiteral("列出日记失败"), query));
    }

    QVector<JournalEntry> entries;
    while (query.next())
        entries.append(journalFromQuery(query));
    return DataResult<QVector<JournalEntry>>::ok(entries);
}
