#include "core/data/databasemanager.h"
#include "core/data/journalrepository.h"
#include "core/data/noterepository.h"
#include "core/data/schedulerepository.h"
#include "core/data/todorepository.h"
#include "core/schedule/courseremindercontroller.h"
#include "core/schedule/wakeupscheduleimporter.h"

#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

namespace {

QString temporaryDatabasePath(QTemporaryDir &directory)
{
    return directory.filePath(QStringLiteral("test.db"));
}

QString uniqueConnectionName()
{
    return QStringLiteral("test-helper-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

bool executeOnDatabase(const QString &path, const QStringList &statements, QString *error = nullptr)
{
    const QString connectionName = uniqueConnectionName();
    bool success = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(path);
        if (!database.open()) {
            if (error)
                *error = database.lastError().text();
        } else {
            success = true;
            for (const QString &statement : statements) {
                QSqlQuery query(database);
                if (!query.exec(statement)) {
                    success = false;
                    if (error)
                        *error = query.lastError().text();
                    break;
                }
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return success;
}

QVariant scalar(const QString &path, const QString &statement, bool *success = nullptr)
{
    const QString connectionName = uniqueConnectionName();
    QVariant value;
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(path);
        if (database.open()) {
            QSqlQuery query(database);
            ok = query.exec(statement) && query.next();
            if (ok)
                value = query.value(0);
            query.finish();
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    if (success)
        *success = ok;
    return value;
}

} // namespace

class DatabaseTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void createsSchemaAndCanReopen();
    void rejectsUnsupportedAndBrokenDatabases();
    void rollsBackFailedMigration();
    void journalCrudAndDailyUniqueness();
    void noteCrudSearchTagsAndCascade();
    void todoCrudFiltersAndCompletionTime();
    void scheduleCrudImportAndReminderDedupe();
    void wakeUpBackupParsing();
};

void DatabaseTests::initTestCase()
{
    QVERIFY2(QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")),
             qPrintable(QStringLiteral("QSQLITE 不可用；可用驱动：%1")
                            .arg(QSqlDatabase::drivers().join(QStringLiteral(", ")))));
}

void DatabaseTests::createsSchemaAndCanReopen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = temporaryDatabasePath(directory);

    {
        DatabaseManager manager(path);
        QVERIFY2(manager.initialize(), qPrintable(manager.errorString()));
        QCOMPARE(scalar(path, QStringLiteral("PRAGMA user_version")).toInt(),
                 DatabaseManager::latestSchemaVersion());

        QSqlDatabase database = manager.database();
        QSqlQuery foreignKeys(database);
        QVERIFY(foreignKeys.exec(QStringLiteral("PRAGMA foreign_keys")));
        QVERIFY(foreignKeys.next());
        QCOMPARE(foreignKeys.value(0).toInt(), 1);

        QSqlQuery trustedSchema(database);
        QVERIFY(trustedSchema.exec(QStringLiteral("PRAGMA trusted_schema")));
        QVERIFY(trustedSchema.next());
        QCOMPARE(trustedSchema.value(0).toInt(), 0);
    }

    DatabaseManager reopened(path);
    QVERIFY2(reopened.initialize(), qPrintable(reopened.errorString()));
    QVERIFY(reopened.isReady());
}

void DatabaseTests::rejectsUnsupportedAndBrokenDatabases()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString newerPath = directory.filePath(QStringLiteral("newer.db"));
    QVERIFY(executeOnDatabase(newerPath, {QStringLiteral("PRAGMA user_version = 99")}));
    DatabaseManager newer(newerPath);
    QVERIFY(!newer.initialize());
    QVERIFY(newer.errorString().contains(QStringLiteral("高于")));
    QCOMPARE(scalar(newerPath, QStringLiteral("PRAGMA user_version")).toInt(), 99);

    const QString brokenPath = directory.filePath(QStringLiteral("broken.db"));
    QFile broken(brokenPath);
    QVERIFY(broken.open(QIODevice::WriteOnly));
    QVERIFY(broken.write("this is not sqlite") > 0);
    broken.close();
    DatabaseManager brokenManager(brokenPath);
    QVERIFY(!brokenManager.initialize());
    QVERIFY(!brokenManager.errorString().isEmpty());

    DatabaseManager directoryAsDatabase(directory.path());
    QVERIFY(!directoryAsDatabase.initialize());
    QVERIFY(!directoryAsDatabase.errorString().isEmpty());

    DatabaseManager missingDriver(directory.filePath(QStringLiteral("missing.db")),
                                  QStringLiteral("QNO_SUCH_DRIVER"));
    QVERIFY(!missingDriver.initialize());
    QVERIFY(missingDriver.errorString().contains(QStringLiteral("不可用")));
}

void DatabaseTests::rollsBackFailedMigration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = temporaryDatabasePath(directory);
    QVERIFY(executeOnDatabase(path,
                              {QStringLiteral("CREATE TABLE journal_entries (id INTEGER PRIMARY KEY)"),
                               QStringLiteral("PRAGMA user_version = 0")}));

    DatabaseManager manager(path);
    QVERIFY(!manager.initialize());
    QCOMPARE(scalar(path, QStringLiteral("PRAGMA user_version")).toInt(), 0);
    QCOMPARE(scalar(path,
                    QStringLiteral("SELECT count(*) FROM sqlite_master "
                                   "WHERE type = 'table' AND name = 'notes'"))
                 .toInt(),
             0);
}

void DatabaseTests::journalCrudAndDailyUniqueness()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = temporaryDatabasePath(directory);
    const QDate date(2026, 9, 8);
    qint64 id = -1;

    {
        DatabaseManager manager(path);
        QVERIFY2(manager.initialize(), qPrintable(manager.errorString()));
        JournalRepository repository(&manager);

        const auto created = repository.create(date, QStringLiteral("标题"), QStringLiteral("正文🙂"));
        QVERIFY2(created.success, qPrintable(created.error));
        id = created.value.id;
        QVERIFY(!repository.create(date, QStringLiteral("重复"), QString()).success);

        const auto found = repository.findByDate(date);
        QVERIFY2(found.success, qPrintable(found.error));
        QVERIFY(found.value.has_value());
        QCOMPARE(found.value->body, QStringLiteral("正文🙂"));

        const auto updated = repository.update(id, QStringLiteral("新标题"), QStringLiteral("新正文"));
        QVERIFY(updated.success);
        QVERIFY(updated.value);
        const auto listed = repository.list(date.addDays(-1), date.addDays(1));
        QVERIFY(listed.success);
        QCOMPARE(listed.value.size(), 1);
        QCOMPARE(listed.value.first().title, QStringLiteral("新标题"));
    }

    DatabaseManager reopened(path);
    QVERIFY(reopened.initialize());
    JournalRepository repository(&reopened);
    const auto persisted = repository.findByDate(date);
    QVERIFY(persisted.success && persisted.value.has_value());
    QCOMPARE(persisted.value->id, id);
    const auto removed = repository.remove(id);
    QVERIFY(removed.success && removed.value);
    QVERIFY(repository.findByDate(date).value == std::nullopt);
}

void DatabaseTests::noteCrudSearchTagsAndCascade()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = temporaryDatabasePath(directory);
    DatabaseManager manager(path);
    QVERIFY2(manager.initialize(), qPrintable(manager.errorString()));
    NoteRepository repository(&manager);

    const auto created = repository.create(QStringLiteral("项目方案"), QStringLiteral("正文含有 100%"),
                                           {QStringLiteral("Work"), QStringLiteral("work"),
                                            QStringLiteral(" 中文 ")});
    QVERIFY2(created.success, qPrintable(created.error));
    QCOMPARE(created.value.tags.size(), 2);
    QCOMPARE(scalar(path, QStringLiteral("SELECT count(*) FROM tags")).toInt(), 2);
    QCOMPARE(scalar(path, QStringLiteral("SELECT count(*) FROM note_tags")).toInt(), 2);

    const auto percentSearch = repository.search(QStringLiteral("100%"));
    QVERIFY(percentSearch.success);
    QCOMPARE(percentSearch.value.size(), 1);
    const auto byTag = repository.listByTag(QStringLiteral("WORK"));
    QVERIFY(byTag.success);
    QCOMPARE(byTag.value.size(), 1);

    const auto updated = repository.update(created.value.id, QStringLiteral("更新"),
                                           QStringLiteral("内容"), {QStringLiteral("个人")});
    QVERIFY(updated.success && updated.value);
    const auto found = repository.findById(created.value.id);
    QVERIFY(found.success && found.value.has_value());
    QCOMPARE(found.value->tags, QStringList{QStringLiteral("个人")});

    const auto removed = repository.remove(created.value.id);
    QVERIFY(removed.success && removed.value);
    QCOMPARE(scalar(path, QStringLiteral("SELECT count(*) FROM note_tags")).toInt(), 0);
}

void DatabaseTests::todoCrudFiltersAndCompletionTime()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = temporaryDatabasePath(directory);
    DatabaseManager manager(path);
    QVERIFY2(manager.initialize(), qPrintable(manager.errorString()));
    TodoRepository repository(&manager);

    QVERIFY(!repository.create(QStringLiteral("   ")).success);
    const QDateTime dueAt = QDateTime::currentDateTimeUtc().addDays(1);
    const auto created = repository.create(QStringLiteral("完成数据层"), QStringLiteral("测试"),
                                           TodoPriority::High, dueAt);
    QVERIFY2(created.success, qPrintable(created.error));
    QVERIFY(created.value.dueAt.isValid());
    QVERIFY(!created.value.completedAt.isValid());

    TodoFilter highPriority;
    highPriority.priority = TodoPriority::High;
    highPriority.dueBefore = dueAt.addSecs(1);
    const auto filtered = repository.list(highPriority);
    QVERIFY(filtered.success);
    QCOMPARE(filtered.value.size(), 1);

    const auto noDeadline = repository.create(QStringLiteral("无截止时间"));
    QVERIFY2(noDeadline.success, qPrintable(noDeadline.error));
    QVERIFY(!noDeadline.value.dueAt.isValid());

    TodoItem edited = noDeadline.value;
    edited.description = QStringLiteral("更新后的说明");
    edited.priority = TodoPriority::Low;
    edited.status = TodoStatus::InProgress;
    const auto updateResult = repository.update(edited);
    QVERIFY(updateResult.success && updateResult.value);
    const auto editedResult = repository.findById(edited.id);
    QVERIFY(editedResult.success && editedResult.value.has_value());
    QCOMPARE(editedResult.value->description, QStringLiteral("更新后的说明"));
    QCOMPARE(editedResult.value->priority, TodoPriority::Low);
    QCOMPARE(editedResult.value->status, TodoStatus::InProgress);

    auto statusResult = repository.setStatus(created.value.id, TodoStatus::Done);
    QVERIFY(statusResult.success && statusResult.value);
    auto found = repository.findById(created.value.id);
    QVERIFY(found.success && found.value.has_value());
    QVERIFY(found.value->completedAt.isValid());

    statusResult = repository.setStatus(created.value.id, TodoStatus::Pending);
    QVERIFY(statusResult.success && statusResult.value);
    found = repository.findById(created.value.id);
    QVERIFY(found.success && found.value.has_value());
    QVERIFY(!found.value->completedAt.isValid());

    QSqlQuery invalid(manager.database());
    invalid.prepare(QStringLiteral("INSERT INTO todos "
                                   "(title, status, priority, created_at, updated_at) "
                                   "VALUES ('bad', 9, 1, :now, :now)"));
    invalid.bindValue(QStringLiteral(":now"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    QVERIFY(!invalid.exec());

    const auto removed = repository.remove(created.value.id);
    QVERIFY(removed.success && removed.value);
    QVERIFY(repository.remove(noDeadline.value.id).success);
}

void DatabaseTests::scheduleCrudImportAndReminderDedupe()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DatabaseManager manager(temporaryDatabasePath(directory));
    QVERIFY2(manager.initialize(), qPrintable(manager.errorString()));
    ScheduleRepository repository(&manager);

    const QDate monday(2026, 9, 7);
    auto first = repository.createSemester(QStringLiteral("秋季学期"), monday, 18, true);
    QVERIFY2(first.success, qPrintable(first.error));
    Course course;
    course.semesterId = first.value.id;
    course.name = QStringLiteral("数据库原理");
    course.teacher = QStringLiteral("林老师");
    course.room = QStringLiteral("A-204");
    course.weekday = 1;
    course.startTime = QTime(8, 0);
    course.endTime = QTime(9, 40);
    course.startWeek = 1;
    course.endWeek = 16;
    auto created = repository.createCourse(course);
    QVERIFY2(created.success, qPrintable(created.error));
    QCOMPARE(repository.listCourses(first.value.id).value.size(), 1);
    QCOMPARE(CourseReminderController::weekForDate(first.value, monday.addDays(14)), 3);
    QVERIFY(CourseReminderController::occursInWeek(created.value, 3));

    CourseReminderController controller(&repository);
    QVector<int> leads;
    connect(&controller, &CourseReminderController::reminderDue, this,
            [&leads](const CourseOccurrence &, int lead, const QString &, const QString &) {
                leads.append(lead);
            });
    controller.checkAt(QDateTime(monday.addDays(7), QTime(7, 30, 30)));
    controller.checkAt(QDateTime(monday.addDays(7), QTime(7, 30, 40)));
    controller.checkAt(QDateTime(monday.addDays(7), QTime(7, 40, 30)));
    QCOMPARE(leads, QVector<int>({30, 20}));

    auto marked = repository.markReminderSent(created.value.id, monday, 30);
    QVERIFY(marked.success && marked.value);
    marked = repository.markReminderSent(created.value.id, monday, 30);
    QVERIFY(marked.success && !marked.value);

    auto second = repository.createSemester(QStringLiteral("春季学期"), monday.addMonths(6), 20, true);
    QVERIFY(second.success);
    QVERIFY(repository.activeSemester().value.has_value());
    QCOMPARE(repository.activeSemester().value->id, second.value.id);
    QVERIFY(repository.removeSemester(first.value.id).value);
    QCOMPARE(scalar(manager.databasePath(), QStringLiteral("SELECT count(*) FROM courses")).toInt(), 0);
    QCOMPARE(scalar(manager.databasePath(), QStringLiteral("SELECT count(*) FROM course_reminders")).toInt(), 0);
}

void DatabaseTests::wakeUpBackupParsing()
{
    const QByteArray backup =
        R"({"courseLen":45,"id":1,"name":"默认","sameBreakLen":true,"sameLen":true,"theBreakLen":10})" "\n"
        R"([{"endTime":"08:45","node":1,"startTime":"08:00","timeTable":1},{"endTime":"09:40","node":2,"startTime":"08:55","timeTable":1}])" "\n"
        R"({"id":1,"maxWeek":18,"nodes":2,"showOtherWeekCourse":false,"showSat":true,"showSun":true,"sundayFirst":false,"startDate":"2026-09-07","tableName":"2026 秋季","timeTable":1})" "\n"
        R"([{"courseName":"高等数学","credit":4,"id":7,"note":"","tableId":1}])" "\n"
        R"([{"day":2,"endWeek":16,"id":7,"level":0,"ownTime":false,"room":"B201","startNode":1,"startWeek":1,"step":2,"tableId":1,"teacher":"王老师","type":1}])";
    const auto result = WakeUpScheduleImporter::parse(backup);
    QVERIFY2(result.success, qPrintable(result.error));
    QCOMPARE(result.value.name, QStringLiteral("2026 秋季"));
    QCOMPARE(result.value.startDate, QDate(2026, 9, 7));
    QCOMPARE(result.value.courses.size(), 1);
    QCOMPARE(result.value.courses.first().name, QStringLiteral("高等数学"));
    QCOMPARE(result.value.courses.first().startTime, QTime(8, 0));
    QCOMPARE(result.value.courses.first().endTime, QTime(9, 40));
    QCOMPARE(result.value.courses.first().weekPattern, CourseWeekPattern::OddWeeks);
    QVERIFY(!WakeUpScheduleImporter::parse(QByteArrayLiteral("{}\n[]")).success);
}

QTEST_GUILESS_MAIN(DatabaseTests)

#include "test_database.moc"
