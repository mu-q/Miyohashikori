#include "noterepository.h"

#include "databasemanager.h"
#include "sqlhelpers.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

Note noteFromQuery(const QSqlQuery &query)
{
    Note note;
    note.id = query.value(QStringLiteral("id")).toLongLong();
    note.title = query.value(QStringLiteral("title")).toString();
    note.body = query.value(QStringLiteral("body")).toString();
    note.createdAt = SqlHelpers::fromUtcText(query.value(QStringLiteral("created_at")).toString());
    note.updatedAt = SqlHelpers::fromUtcText(query.value(QStringLiteral("updated_at")).toString());
    return note;
}

QString unavailableError()
{
    return QStringLiteral("笔记数据库尚未初始化");
}

} // namespace

NoteRepository::NoteRepository(DatabaseManager *databaseManager)
    : databaseManager_(databaseManager)
{
}

DataResult<Note> NoteRepository::create(const QString &title, const QString &body,
                                        const QStringList &tags)
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<Note>::fail(unavailableError());

    QSqlDatabase database = databaseManager_->database();
    if (!database.transaction()) {
        return DataResult<Note>::fail(
            QStringLiteral("无法开始创建笔记事务：%1").arg(database.lastError().text()));
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral("INSERT INTO notes (title, body, created_at, updated_at) "
                                 "VALUES (:title, :body, :created, :updated)"));
    const QString now = SqlHelpers::utcNowText();
    query.bindValue(QStringLiteral(":title"), SqlHelpers::nonNullText(title));
    query.bindValue(QStringLiteral(":body"), SqlHelpers::nonNullText(body));
    query.bindValue(QStringLiteral(":created"), now);
    query.bindValue(QStringLiteral(":updated"), now);
    if (!query.exec()) {
        database.rollback();
        return DataResult<Note>::fail(SqlHelpers::queryError(QStringLiteral("创建笔记失败"), query));
    }

    const qint64 id = query.lastInsertId().toLongLong();
    query.finish();
    const DataResult<bool> tagsResult = replaceTags(database, id, tags);
    if (!tagsResult.success) {
        database.rollback();
        return DataResult<Note>::fail(tagsResult.error);
    }
    if (!database.commit()) {
        const QString error = QStringLiteral("无法提交创建笔记事务：%1")
                                  .arg(database.lastError().text());
        database.rollback();
        return DataResult<Note>::fail(error);
    }

    Note note;
    note.id = id;
    note.title = SqlHelpers::nonNullText(title);
    note.body = SqlHelpers::nonNullText(body);
    note.tags = SqlHelpers::normalizedTags(tags);
    note.createdAt = SqlHelpers::fromUtcText(now);
    note.updatedAt = note.createdAt;
    return DataResult<Note>::ok(note);
}

DataResult<std::optional<Note>> NoteRepository::findById(qint64 id) const
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<std::optional<Note>>::fail(unavailableError());

    QSqlDatabase database = databaseManager_->database();
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT id, title, body, created_at, updated_at "
                                 "FROM notes WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        return DataResult<std::optional<Note>>::fail(
            SqlHelpers::queryError(QStringLiteral("读取笔记失败"), query));
    }
    if (!query.next())
        return DataResult<std::optional<Note>>::ok(std::nullopt);
    Note note = noteFromQuery(query);
    query.finish();

    const DataResult<QStringList> tagsResult = loadTags(database, note.id);
    if (!tagsResult.success)
        return DataResult<std::optional<Note>>::fail(tagsResult.error);
    note.tags = tagsResult.value;
    return DataResult<std::optional<Note>>::ok(note);
}

DataResult<bool> NoteRepository::update(qint64 id, const QString &title, const QString &body,
                                        const QStringList &tags)
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<bool>::fail(unavailableError());

    QSqlDatabase database = databaseManager_->database();
    if (!database.transaction()) {
        return DataResult<bool>::fail(
            QStringLiteral("无法开始更新笔记事务：%1").arg(database.lastError().text()));
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral("UPDATE notes SET title = :title, body = :body, "
                                 "updated_at = :updated WHERE id = :id"));
    query.bindValue(QStringLiteral(":title"), SqlHelpers::nonNullText(title));
    query.bindValue(QStringLiteral(":body"), SqlHelpers::nonNullText(body));
    query.bindValue(QStringLiteral(":updated"), SqlHelpers::utcNowText());
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        database.rollback();
        return DataResult<bool>::fail(SqlHelpers::queryError(QStringLiteral("更新笔记失败"), query));
    }
    const bool found = query.numRowsAffected() > 0;
    query.finish();
    if (!found) {
        database.rollback();
        return DataResult<bool>::ok(false);
    }

    const DataResult<bool> tagsResult = replaceTags(database, id, tags);
    if (!tagsResult.success) {
        database.rollback();
        return tagsResult;
    }
    if (!database.commit()) {
        const QString error = QStringLiteral("无法提交更新笔记事务：%1")
                                  .arg(database.lastError().text());
        database.rollback();
        return DataResult<bool>::fail(error);
    }
    return DataResult<bool>::ok(true);
}

DataResult<bool> NoteRepository::remove(qint64 id)
{
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<bool>::fail(unavailableError());

    QSqlQuery query(databaseManager_->database());
    query.prepare(QStringLiteral("DELETE FROM notes WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec())
        return DataResult<bool>::fail(SqlHelpers::queryError(QStringLiteral("删除笔记失败"), query));
    return DataResult<bool>::ok(query.numRowsAffected() > 0);
}

DataResult<QVector<Note>> NoteRepository::list(int offset, int limit) const
{
    const QString pageError = validatePage(offset, limit);
    if (!pageError.isEmpty())
        return DataResult<QVector<Note>>::fail(pageError);
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<QVector<Note>>::fail(unavailableError());

    QSqlDatabase database = databaseManager_->database();
    return runListQuery(database,
                        QStringLiteral("SELECT id, title, body, created_at, updated_at FROM notes "
                                       "ORDER BY updated_at DESC, id DESC LIMIT ? OFFSET ?"),
                        {limit, offset});
}

DataResult<QVector<Note>> NoteRepository::search(const QString &term, int offset, int limit) const
{
    const QString pageError = validatePage(offset, limit);
    if (!pageError.isEmpty())
        return DataResult<QVector<Note>>::fail(pageError);
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<QVector<Note>>::fail(unavailableError());

    const QString pattern = QStringLiteral("%") + SqlHelpers::escapedLike(term.trimmed())
                            + QStringLiteral("%");
    QSqlDatabase database = databaseManager_->database();
    return runListQuery(database,
                        QStringLiteral("SELECT id, title, body, created_at, updated_at FROM notes "
                                       "WHERE title LIKE ? ESCAPE '\\' OR body LIKE ? ESCAPE '\\' "
                                       "ORDER BY updated_at DESC, id DESC LIMIT ? OFFSET ?"),
                        {pattern, pattern, limit, offset});
}

DataResult<QVector<Note>> NoteRepository::listByTag(const QString &tag, int offset,
                                                    int limit) const
{
    const QString pageError = validatePage(offset, limit);
    if (!pageError.isEmpty())
        return DataResult<QVector<Note>>::fail(pageError);
    if (!databaseManager_ || !databaseManager_->isReady())
        return DataResult<QVector<Note>>::fail(unavailableError());

    QSqlDatabase database = databaseManager_->database();
    return runListQuery(database,
                        QStringLiteral("SELECT n.id, n.title, n.body, n.created_at, n.updated_at "
                                       "FROM notes n JOIN note_tags nt ON nt.note_id = n.id "
                                       "JOIN tags t ON t.id = nt.tag_id "
                                       "WHERE t.name = ? COLLATE NOCASE "
                                       "ORDER BY n.updated_at DESC, n.id DESC LIMIT ? OFFSET ?"),
                        {tag.trimmed(), limit, offset});
}

DataResult<bool> NoteRepository::replaceTags(QSqlDatabase &database, qint64 noteId,
                                             const QStringList &tags) const
{
    QSqlQuery removeQuery(database);
    removeQuery.prepare(QStringLiteral("DELETE FROM note_tags WHERE note_id = :note_id"));
    removeQuery.bindValue(QStringLiteral(":note_id"), noteId);
    if (!removeQuery.exec()) {
        return DataResult<bool>::fail(
            SqlHelpers::queryError(QStringLiteral("清理笔记标签失败"), removeQuery));
    }

    for (const QString &tag : SqlHelpers::normalizedTags(tags)) {
        QSqlQuery insertTag(database);
        insertTag.prepare(QStringLiteral("INSERT OR IGNORE INTO tags (name) VALUES (:name)"));
        insertTag.bindValue(QStringLiteral(":name"), tag);
        if (!insertTag.exec()) {
            return DataResult<bool>::fail(
                SqlHelpers::queryError(QStringLiteral("保存标签失败"), insertTag));
        }

        QSqlQuery findTag(database);
        findTag.prepare(QStringLiteral("SELECT id FROM tags WHERE name = :name COLLATE NOCASE"));
        findTag.bindValue(QStringLiteral(":name"), tag);
        if (!findTag.exec() || !findTag.next()) {
            return DataResult<bool>::fail(
                SqlHelpers::queryError(QStringLiteral("读取标签失败"), findTag));
        }
        const qint64 tagId = findTag.value(0).toLongLong();
        findTag.finish();

        QSqlQuery link(database);
        link.prepare(QStringLiteral("INSERT INTO note_tags (note_id, tag_id) "
                                    "VALUES (:note_id, :tag_id)"));
        link.bindValue(QStringLiteral(":note_id"), noteId);
        link.bindValue(QStringLiteral(":tag_id"), tagId);
        if (!link.exec()) {
            return DataResult<bool>::fail(
                SqlHelpers::queryError(QStringLiteral("关联笔记标签失败"), link));
        }
    }
    return DataResult<bool>::ok(true);
}

DataResult<QStringList> NoteRepository::loadTags(QSqlDatabase &database, qint64 noteId) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT t.name FROM tags t "
                                 "JOIN note_tags nt ON nt.tag_id = t.id "
                                 "WHERE nt.note_id = :note_id "
                                 "ORDER BY t.name COLLATE NOCASE"));
    query.bindValue(QStringLiteral(":note_id"), noteId);
    if (!query.exec()) {
        return DataResult<QStringList>::fail(
            SqlHelpers::queryError(QStringLiteral("读取笔记标签失败"), query));
    }

    QStringList tags;
    while (query.next())
        tags.append(query.value(0).toString());
    return DataResult<QStringList>::ok(tags);
}

DataResult<QVector<Note>> NoteRepository::runListQuery(QSqlDatabase &database,
                                                       const QString &sql,
                                                       const QVariantList &bindings) const
{
    QSqlQuery query(database);
    query.prepare(sql);
    for (const QVariant &binding : bindings)
        query.addBindValue(binding);
    if (!query.exec()) {
        return DataResult<QVector<Note>>::fail(
            SqlHelpers::queryError(QStringLiteral("列出笔记失败"), query));
    }

    QVector<Note> notes;
    while (query.next())
        notes.append(noteFromQuery(query));
    query.finish();

    for (Note &note : notes) {
        const DataResult<QStringList> tagsResult = loadTags(database, note.id);
        if (!tagsResult.success)
            return DataResult<QVector<Note>>::fail(tagsResult.error);
        note.tags = tagsResult.value;
    }
    return DataResult<QVector<Note>>::ok(notes);
}

QString NoteRepository::validatePage(int offset, int limit)
{
    if (offset < 0 || limit <= 0 || limit > 1000)
        return QStringLiteral("分页参数无效：offset 必须非负，limit 必须为 1 到 1000");
    return {};
}
