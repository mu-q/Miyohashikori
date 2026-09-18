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
    explicit ConversationLog(const QString &filePath, QObject *parent = nullptr);
    const QVector<Entry> &entries() const { return entries_; }
    void addUser(const QString &text);
    void addHyori(const QString &text);
    void clear();

signals:
    void entryAdded(const ConversationLog::Entry &entry);
    void historyCleared();

private:
    void add(Speaker speaker, const QString &text);
    void load();
    bool save() const;
    QVector<Entry> entries_;
    QString filePath_;
};

Q_DECLARE_METATYPE(ConversationLog::Entry)
