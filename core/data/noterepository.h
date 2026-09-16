#pragma once

#include "dataresult.h"
#include "models.h"

#include <QVector>
#include <QVariant>

#include <optional>

class DatabaseManager;
class QSqlDatabase;

class NoteRepository
{
public:
    explicit NoteRepository(DatabaseManager *databaseManager);

    DataResult<Note> create(const QString &title, const QString &body,
                            const QStringList &tags = {});
    DataResult<std::optional<Note>> findById(qint64 id) const;
    DataResult<bool> update(qint64 id, const QString &title, const QString &body,
                            const QStringList &tags);
    DataResult<bool> remove(qint64 id);
    DataResult<QVector<Note>> list(int offset = 0, int limit = 100) const;
    DataResult<QVector<Note>> search(const QString &term, int offset = 0,
                                     int limit = 100) const;
    DataResult<QVector<Note>> listByTag(const QString &tag, int offset = 0,
                                        int limit = 100) const;

private:
    DataResult<bool> replaceTags(QSqlDatabase &database, qint64 noteId,
                                 const QStringList &tags) const;
    DataResult<QStringList> loadTags(QSqlDatabase &database, qint64 noteId) const;
    DataResult<QVector<Note>> runListQuery(QSqlDatabase &database, const QString &sql,
                                           const QVariantList &bindings) const;
    static QString validatePage(int offset, int limit);

    DatabaseManager *databaseManager_ = nullptr;
};
