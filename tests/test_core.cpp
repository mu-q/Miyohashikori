#include "../core/config/appconfig.h"
#include "../core/config/configmanager.h"
#include "../core/pomodorocontroller.h"
#include "../core/ai/openaichatsession.h"
#include "../core/conversationlog.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

class ControlledHttpServer : public QObject
{
    Q_OBJECT

public:
    explicit ControlledHttpServer(QObject *parent = nullptr) : QObject(parent)
    {
        connect(&server_, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = server_.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    QByteArray &buffer = buffers_[socket];
                    buffer.append(socket->readAll());
                    if (!buffer.contains("\r\n\r\n") || queuedSockets_.contains(socket))
                        return;
                    queuedSockets_.append(socket);
                    emit requestReceived();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }

    bool listen() { return server_.listen(QHostAddress::LocalHost); }
    quint16 port() const { return server_.serverPort(); }
    int requestCount() const { return queuedSockets_.size(); }

    void respondNext()
    {
        QVERIFY(!queuedSockets_.isEmpty());
        QTcpSocket *socket = queuedSockets_.takeFirst();
        const QByteArray body = QByteArrayLiteral(
            "{\"choices\":[{\"message\":{\"content\":\"{\\\"display_zh\\\":\\\"好的\\\","
            "\\\"speech_ja\\\":\\\"はい\\\",\\\"emotion\\\":\\\"neutral\\\"}\"}}]}");
        const QByteArray response = QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n")
                                    + "Content-Length: " + QByteArray::number(body.size())
                                    + "\r\nConnection: close\r\n\r\n" + body;
        socket->write(response);
        socket->disconnectFromHost();
    }

signals:
    void requestReceived();

private:
    QTcpServer server_;
    QHash<QTcpSocket *, QByteArray> buffers_;
    QList<QTcpSocket *> queuedSockets_;
};

class CoreTests : public QObject
{
    Q_OBJECT

private slots:
    void durationChangesResetPausedPhase();
    void skippingWorkDoesNotCountAsCompletion();
    void naturalCompletionCountsAndNotifies();
    void malformedConfigIsBackedUpBeforeReset();
    void themePreferenceRoundTrips();
    void aiRequestsAreProcessedSerially();
    void conversationHistoryPersistsAndCanBeCleared();
};

void CoreTests::themePreferenceRoundTrips()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ConfigManager manager(directory.filePath(QStringLiteral("config.json")));
    QVERIFY(manager.load());
    QCOMPARE(manager.config().theme, QStringLiteral("system"));

    AppConfig config = manager.config();
    config.theme = QStringLiteral("mist");
    manager.setConfig(config);
    QVERIFY(manager.save());

    ConfigManager reloaded(directory.filePath(QStringLiteral("config.json")));
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.config().theme, QStringLiteral("mist"));

    QJsonObject invalidTheme = reloaded.config().toJson();
    invalidTheme.insert(QStringLiteral("theme"), QStringLiteral("unknown"));
    QCOMPARE(AppConfig::fromJson(invalidTheme).theme, QStringLiteral("system"));
}

void CoreTests::durationChangesResetPausedPhase()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ConfigManager manager(directory.filePath(QStringLiteral("config.json")));
    QVERIFY(manager.load());
    PomodoroController controller(&manager);

    controller.setDurations(42, 7, 19);

    QCOMPARE(controller.remainingSeconds(), 42 * 60);
    QCOMPARE(manager.config().pomodoroWorkMinutes, 42);
}

void CoreTests::skippingWorkDoesNotCountAsCompletion()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ConfigManager manager(directory.filePath(QStringLiteral("config.json")));
    QVERIFY(manager.load());
    PomodoroController controller(&manager);
    QSignalSpy completedSpy(&controller, &PomodoroController::completed);

    controller.skip();

    QCOMPARE(controller.phase(), PomodoroController::Phase::ShortBreak);
    QCOMPARE(controller.completedCycles(), 0);
    QCOMPARE(completedSpy.count(), 0);
}

void CoreTests::naturalCompletionCountsAndNotifies()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ConfigManager manager(directory.filePath(QStringLiteral("config.json")));
    QVERIFY(manager.load());
    AppConfig config = manager.config();
    config.pomodoroRemainingSeconds = 1;
    manager.setConfig(config);
    QVERIFY(manager.save());
    PomodoroController controller(&manager);
    QSignalSpy completedSpy(&controller, &PomodoroController::completed);

    controller.start();
    QTRY_COMPARE_WITH_TIMEOUT(completedSpy.count(), 1, 2000);

    controller.pause();
    QCOMPARE(controller.completedCycles(), 1);
    QCOMPARE(controller.phase(), PomodoroController::Phase::ShortBreak);
}

void CoreTests::malformedConfigIsBackedUpBeforeReset()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString configPath = directory.filePath(QStringLiteral("config.json"));
    const QByteArray invalidJson("{ definitely not valid json");
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(invalidJson), invalidJson.size());
    file.close();

    ConfigManager manager(configPath);
    QVERIFY(manager.load());

    const QStringList backups = QDir(directory.path()).entryList(
        {QStringLiteral("config.json.invalid-*.bak")}, QDir::Files);
    QCOMPARE(backups.size(), 1);
    QFile backup(directory.filePath(backups.first()));
    QVERIFY(backup.open(QIODevice::ReadOnly));
    QCOMPARE(backup.readAll(), invalidJson);

    QFile resetConfig(configPath);
    QVERIFY(resetConfig.open(QIODevice::ReadOnly));
    QVERIFY(QJsonDocument::fromJson(resetConfig.readAll()).isObject());
}

void CoreTests::aiRequestsAreProcessedSerially()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ControlledHttpServer server;
    QVERIFY(server.listen());

    ConfigManager manager(directory.filePath(QStringLiteral("config.json")));
    QVERIFY(manager.load());
    AppConfig config = manager.config();
    config.llmEndpoint = QStringLiteral("http://127.0.0.1:%1/chat/completions").arg(server.port());
    config.llmApiKey = QStringLiteral("test-key");
    config.llmModel = QStringLiteral("test-model");
    manager.setConfig(config);
    QVERIFY(manager.save());

    OpenAiChatSession session(&manager);
    QSignalSpy replies(&session, &OpenAiChatSession::assistantMessage);
    QSignalSpy busyChanges(&session, &OpenAiChatSession::busyChanged);

    session.submit(QStringLiteral("第一条"));
    session.submit(QStringLiteral("第二条"));
    QTRY_COMPARE_WITH_TIMEOUT(server.requestCount(), 1, 1000);
    QTest::qWait(100);
    QCOMPARE(server.requestCount(), 1);

    server.respondNext();
    QTRY_COMPARE_WITH_TIMEOUT(replies.count(), 1, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(server.requestCount(), 1, 1000);
    server.respondNext();
    QTRY_COMPARE_WITH_TIMEOUT(replies.count(), 2, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!busyChanges.isEmpty()
                                 && !busyChanges.last().at(0).toBool(),
                             1000);
    QCOMPARE(busyChanges.first().at(0).toBool(), true);
}

void CoreTests::conversationHistoryPersistsAndCanBeCleared()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("conversation-history.json"));
    {
        ConversationLog log(path);
        log.addUser(QStringLiteral("晚上好"));
        log.addHyori(QStringLiteral("晚上好。今天辛苦了。"));
        QCOMPARE(log.entries().size(), 2);
    }
    {
        ConversationLog reopened(path);
        QCOMPARE(reopened.entries().size(), 2);
        QCOMPARE(reopened.entries().first().speaker, ConversationLog::Speaker::User);
        QCOMPARE(reopened.entries().first().text, QStringLiteral("晚上好"));
        QCOMPARE(reopened.entries().last().speaker, ConversationLog::Speaker::Hyori);
        QSignalSpy cleared(&reopened, &ConversationLog::historyCleared);
        reopened.clear();
        QCOMPARE(cleared.count(), 1);
    }
    ConversationLog empty(path);
    QVERIFY(empty.entries().isEmpty());
}

QTEST_GUILESS_MAIN(CoreTests)

#include "test_core.moc"
