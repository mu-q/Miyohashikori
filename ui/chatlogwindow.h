#pragma once

#include <QDialog>

#include "../core/conversationlog.h"

class QTextBrowser;

class ChatLogWindow : public QDialog
{
    Q_OBJECT
public:
    explicit ChatLogWindow(ConversationLog *log, QWidget *parent = nullptr);

private:
    void appendEntry(const ConversationLog::Entry &entry);
    void applyTheme();
    ConversationLog *log_ = nullptr;
    QTextBrowser *browser_ = nullptr;
};
