#include "chathistory.h"

#include <QJsonObject>

namespace {
constexpr int kMaxContextMessages = 20;
}

void ChatHistory::clear()
{
    messages_.clear();
}

void ChatHistory::addUserMessage(const QString &text)
{
    messages_.append({QStringLiteral("user"), text});
    trimToRecentTurns();
}

void ChatHistory::addAssistantMessage(const QString &text)
{
    messages_.append({QStringLiteral("assistant"), text});
    trimToRecentTurns();
}

QJsonArray ChatHistory::toOpenAiMessages(const QString &systemPrompt,
                                         const QJsonArray &prefillMessages) const
{
    QJsonArray messages;

    QJsonObject system;
    system.insert(QStringLiteral("role"), QStringLiteral("system"));
    system.insert(QStringLiteral("content"), systemPrompt);
    messages.append(system);

    for (const QJsonValue &value : prefillMessages)
        messages.append(value);

    for (const Message &message : messages_) {
        QJsonObject obj;
        obj.insert(QStringLiteral("role"), message.role);
        obj.insert(QStringLiteral("content"), message.content);
        messages.append(obj);
    }

    return messages;
}

void ChatHistory::trimToRecentTurns()
{
    if (messages_.size() <= kMaxContextMessages)
        return;

    const int removeCount = messages_.size() - kMaxContextMessages;
    messages_.erase(messages_.begin(), messages_.begin() + removeCount);
}
