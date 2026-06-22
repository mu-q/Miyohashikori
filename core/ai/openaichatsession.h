#pragma once

#include "chathistory.h"
#include "iaisession.h"

#include <QStringList>
class ConfigManager;
class QNetworkAccessManager;
class QNetworkReply;

class OpenAiChatSession : public IAiSession
{
    Q_OBJECT

public:
    explicit OpenAiChatSession(ConfigManager *configManager, QObject *parent = nullptr);

    void submit(const QString &userText) override;

private:
    struct PendingRequest
    {
        QString userText;
        int attempt = 0;
    };

    QString buildSystemPrompt() const;
    QJsonArray buildFewShotMessages() const;
    QString buildMemoryPrompt() const;
    void updateShortTermMemory(const QString &userText);
    void sendRequest(const PendingRequest &request);
    void handleReply(QNetworkReply *reply, PendingRequest request);
    void retryRequest(const PendingRequest &request, const QString &reason);

    QNetworkAccessManager *network_ = nullptr;
    ConfigManager *configManager_ = nullptr;
    ChatHistory history_;
    QStringList shortTermMemory_;
};
