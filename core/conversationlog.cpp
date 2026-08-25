#include "conversationlog.h"

#include <QDateTime>

ConversationLog::ConversationLog(QObject *parent) : QObject(parent) {}

void ConversationLog::addUser(const QString &text) { add(Speaker::User, text); }
void ConversationLog::addHyori(const QString &text) { add(Speaker::Hyori, text); }

void ConversationLog::add(Speaker speaker, const QString &text)
{
    const QString cleaned = text.trimmed();
    if (cleaned.isEmpty()) return;
    entries_.append({speaker, cleaned, QDateTime::currentDateTime()});
    emit entryAdded(entries_.last());
}
