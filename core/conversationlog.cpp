#include "conversationlog.h"

#include "apppaths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {
constexpr int kMaximumStoredEntries = 500;
}

ConversationLog::ConversationLog(QObject *parent)
    : ConversationLog(AppPaths::conversationHistoryFilePath(), parent)
{
}

ConversationLog::ConversationLog(const QString &filePath, QObject *parent)
    : QObject(parent), filePath_(QDir::cleanPath(filePath))
{
    load();
}

void ConversationLog::addUser(const QString &text) { add(Speaker::User, text); }
void ConversationLog::addHyori(const QString &text) { add(Speaker::Hyori, text); }

void ConversationLog::add(Speaker speaker, const QString &text)
{
    const QString cleaned = text.trimmed();
    if (cleaned.isEmpty()) return;
    entries_.append({speaker, cleaned, QDateTime::currentDateTime()});
    while (entries_.size() > kMaximumStoredEntries)
        entries_.removeFirst();
    save();
    emit entryAdded(entries_.last());
}

void ConversationLog::clear()
{
    entries_.clear();
    save();
    emit historyCleared();
}

void ConversationLog::load()
{
    QFile file(filePath_);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray())
        return;

    const QJsonArray array = document.array();
    const qsizetype first = qMax<qsizetype>(0, array.size() - kMaximumStoredEntries);
    for (qsizetype i = first; i < array.size(); ++i) {
        const QJsonObject object = array.at(i).toObject();
        const QString speaker = object.value(QStringLiteral("speaker")).toString();
        const QString text = object.value(QStringLiteral("text")).toString().trimmed();
        const QDateTime timestamp = QDateTime::fromString(
            object.value(QStringLiteral("timestamp")).toString(), Qt::ISODateWithMs);
        if ((speaker != QStringLiteral("user") && speaker != QStringLiteral("hyori"))
            || text.isEmpty() || !timestamp.isValid()) {
            continue;
        }
        entries_.append({speaker == QStringLiteral("user") ? Speaker::User : Speaker::Hyori,
                         text, timestamp});
    }
}

bool ConversationLog::save() const
{
    QDir directory(QFileInfo(filePath_).absolutePath());
    if (!directory.exists() && !directory.mkpath(QStringLiteral(".")))
        return false;

    QJsonArray array;
    for (const Entry &entry : entries_) {
        array.append(QJsonObject{
            {QStringLiteral("speaker"), entry.speaker == Speaker::User
                                            ? QStringLiteral("user")
                                            : QStringLiteral("hyori")},
            {QStringLiteral("text"), entry.text},
            {QStringLiteral("timestamp"), entry.timestamp.toString(Qt::ISODateWithMs)}});
    }

    QSaveFile file(filePath_);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
    return file.commit();
}
