#pragma once

#include <QJsonArray>
#include <QString>
#include <QVector>

class ChatHistory
{
public:
    void clear();
    void addUserMessage(const QString &text);
    void addAssistantMessage(const QString &text);
    QJsonArray toOpenAiMessages(const QString &systemPrompt,
                                const QJsonArray &prefillMessages = {}) const;

private:
    struct Message
    {
        QString role;
        QString content;
    };

    void trimToRecentTurns();

    QVector<Message> messages_;
};
