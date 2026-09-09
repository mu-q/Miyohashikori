#pragma once

#include <QSqlDatabase>
#include <QString>

class DatabaseManager
{
public:
    explicit DatabaseManager(QString databasePath = {},
                             QString driverName = QStringLiteral("QSQLITE"));
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager &) = delete;
    DatabaseManager &operator=(const DatabaseManager &) = delete;

    bool initialize();
    void close();

    bool isReady() const;
    QString errorString() const;
    QString databasePath() const;
    QString connectionName() const;
    QSqlDatabase database() const;

    static constexpr int latestSchemaVersion() { return 1; }

private:
    bool configureConnection(QSqlDatabase &database);
    bool migrate(QSqlDatabase &database);
    bool migrateToVersion1(QSqlDatabase &database);
    bool execute(QSqlDatabase &database, const QString &statement);
    bool fail(const QString &message);

    QString databasePath_;
    QString driverName_;
    QString connectionName_;
    QString errorString_;
    bool ready_ = false;
};
