#pragma once

#include <QObject>
#include <QDateTime>
#include <QVector>

class ConversationLog : public QObject
{
    Q_OBJECT
public:
    enum class Speaker { User, Hyori };
    struct Entry { Speaker speaker; QString text; QDateTime timestamp; };

    explicit ConversationLog(QObject *parent = nullptr);
    const QVector<Entry> &entries() const { return entries_; }
    void addUser(const QString &text);
    void addHyori(const QString &text);

signals:
    void entryAdded(const ConversationLog::Entry &entry);

private:
    void add(Speaker speaker, const QString &text);
    QVector<Entry> entries_;
};

Q_DECLARE_METATYPE(ConversationLog::Entry)
