#include "databasemanager.h"

#include "../apppaths.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>

#include <utility>

namespace {

QString queryError(const QString &context, const QSqlQuery &query)
{
    return QStringLiteral("%1：%2").arg(context, query.lastError().text());
}

} // namespace

DatabaseManager::DatabaseManager(QString databasePath, QString driverName)
    : databasePath_(databasePath.isEmpty() ? AppPaths::databaseFilePath()
                                           : QDir::cleanPath(databasePath))
    , driverName_(std::move(driverName))
    , connectionName_(QStringLiteral("hyori-db-%1")
                          .arg(reinterpret_cast<quintptr>(this), 0, 16))
{
}

DatabaseManager::~DatabaseManager()
{
    close();
}

bool DatabaseManager::initialize()
{
    if (ready_)
        return true;

    errorString_.clear();
    if (!QSqlDatabase::isDriverAvailable(driverName_)) {
        return fail(QStringLiteral("数据库驱动 %1 不可用；可用驱动：%2")
                        .arg(driverName_, QSqlDatabase::drivers().join(QStringLiteral(", "))));
    }

    const QFileInfo info(databasePath_);
    QDir directory = info.dir();
    if (!directory.exists() && !directory.mkpath(QStringLiteral(".")))
        return fail(QStringLiteral("无法创建数据库目录：%1").arg(directory.absolutePath()));

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(driverName_, connectionName_);
        db.setDatabaseName(databasePath_);
        if (driverName_ == QStringLiteral("QSQLITE"))
            db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=3000"));

        if (!db.open()) {
            const QString message = QStringLiteral("无法打开数据库 %1：%2")
                                        .arg(databasePath_, db.lastError().text());
            db.close();
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(connectionName_);
            return fail(message);
        }

        if (!configureConnection(db) || !migrate(db)) {
            db.close();
            ready_ = false;
        } else {
            ready_ = true;
        }
    }

    if (!ready_)
        QSqlDatabase::removeDatabase(connectionName_);
    return ready_;
}

void DatabaseManager::close()
{
    if (!QSqlDatabase::contains(connectionName_)) {
        ready_ = false;
        return;
    }

    {
        QSqlDatabase db = QSqlDatabase::database(connectionName_, false);
        if (db.isValid())
            db.close();
    }
    QSqlDatabase::removeDatabase(connectionName_);
    ready_ = false;
}

bool DatabaseManager::isReady() const
{
    return ready_;
}

QString DatabaseManager::errorString() const
{
    return errorString_;
}

QString DatabaseManager::databasePath() const
{
    return databasePath_;
}

QString DatabaseManager::connectionName() const
{
    return connectionName_;
}

QSqlDatabase DatabaseManager::database() const
{
    if (!ready_ || !QSqlDatabase::contains(connectionName_))
        return {};
    return QSqlDatabase::database(connectionName_, false);
}

bool DatabaseManager::configureConnection(QSqlDatabase &database)
{
    if (driverName_ != QStringLiteral("QSQLITE"))
        return true;

    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys = ON")))
        return fail(queryError(QStringLiteral("无法启用外键约束"), query));
    if (!query.exec(QStringLiteral("PRAGMA trusted_schema = OFF")))
        return fail(queryError(QStringLiteral("无法关闭 trusted_schema"), query));
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys")) || !query.next()
        || query.value(0).toInt() != 1) {
        return fail(QStringLiteral("SQLite 外键约束未能启用"));
    }
    return true;
}

bool DatabaseManager::migrate(QSqlDatabase &database)
{
    QSqlQuery versionQuery(database);
    if (!versionQuery.exec(QStringLiteral("PRAGMA user_version")) || !versionQuery.next())
        return fail(queryError(QStringLiteral("无法读取数据库版本"), versionQuery));

    const int version = versionQuery.value(0).toInt();
    versionQuery.finish();
    if (version > latestSchemaVersion()) {
        return fail(QStringLiteral("数据库版本 %1 高于程序支持的版本 %2，拒绝降级打开")
                        .arg(version)
                        .arg(latestSchemaVersion()));
    }
    if (version == latestSchemaVersion())
        return true;

    if (!database.transaction())
        return fail(QStringLiteral("无法开始数据库迁移事务：%1").arg(database.lastError().text()));

    bool success = true;
    if (version < 1)
        success = migrateToVersion1(database);

    if (success)
        success = execute(database, QStringLiteral("PRAGMA user_version = 1"));

    if (!success) {
        database.rollback();
        return false;
    }
    if (!database.commit()) {
        const QString message = QStringLiteral("无法提交数据库迁移：%1")
                                    .arg(database.lastError().text());
        database.rollback();
        return fail(message);
    }
    return true;
}

bool DatabaseManager::migrateToVersion1(QSqlDatabase &database)
{
    const QStringList statements = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS journal_entries ("
                       "id INTEGER PRIMARY KEY, entry_date TEXT NOT NULL UNIQUE, "
                       "title TEXT NOT NULL DEFAULT '', body TEXT NOT NULL DEFAULT '', "
                       "created_at TEXT NOT NULL, updated_at TEXT NOT NULL)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_journal_entry_date "
                       "ON journal_entries(entry_date)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_journal_updated_at "
                       "ON journal_entries(updated_at)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS notes ("
                       "id INTEGER PRIMARY KEY, title TEXT NOT NULL DEFAULT '', "
                       "body TEXT NOT NULL DEFAULT '', created_at TEXT NOT NULL, "
                       "updated_at TEXT NOT NULL)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_notes_updated_at ON notes(updated_at)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS tags ("
                       "id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE COLLATE NOCASE)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS note_tags ("
                       "note_id INTEGER NOT NULL, tag_id INTEGER NOT NULL, "
                       "PRIMARY KEY(note_id, tag_id), "
                       "FOREIGN KEY(note_id) REFERENCES notes(id) ON DELETE CASCADE, "
                       "FOREIGN KEY(tag_id) REFERENCES tags(id) ON DELETE CASCADE)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_note_tags_tag_id ON note_tags(tag_id)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS todos ("
                       "id INTEGER PRIMARY KEY, title TEXT NOT NULL, "
                       "description TEXT NOT NULL DEFAULT '', "
                       "status INTEGER NOT NULL DEFAULT 0 CHECK(status IN (0, 1, 2)), "
                       "priority INTEGER NOT NULL DEFAULT 1 CHECK(priority IN (0, 1, 2)), "
                       "due_at TEXT, completed_at TEXT, created_at TEXT NOT NULL, "
                       "updated_at TEXT NOT NULL)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_todos_status ON todos(status)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_todos_priority ON todos(priority)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_todos_due_at ON todos(due_at)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_todos_updated_at ON todos(updated_at)")
    };

    for (const QString &statement : statements) {
        if (!execute(database, statement))
            return false;
    }
    return true;
}

bool DatabaseManager::execute(QSqlDatabase &database, const QString &statement)
{
    QSqlQuery query(database);
    if (query.exec(statement))
        return true;
    return fail(queryError(QStringLiteral("数据库迁移失败"), query));
}

bool DatabaseManager::fail(const QString &message)
{
    errorString_ = message;
    return false;
}
