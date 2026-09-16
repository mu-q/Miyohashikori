#pragma once

#include "dataresult.h"
#include "models.h"

#include <QVector>

#include <optional>

class DatabaseManager;

class JournalRepository
{
public:
    explicit JournalRepository(DatabaseManager *databaseManager);

    DataResult<JournalEntry> create(const QDate &date, const QString &title,
                                    const QString &body);
    DataResult<std::optional<JournalEntry>> findByDate(const QDate &date) const;
    DataResult<bool> update(qint64 id, const QString &title, const QString &body);
    DataResult<bool> remove(qint64 id);
    DataResult<QVector<JournalEntry>> list(const QDate &from, const QDate &to) const;

private:
    DatabaseManager *databaseManager_ = nullptr;
};
