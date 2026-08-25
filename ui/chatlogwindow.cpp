#include "chatlogwindow.h"

#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QScrollBar>
#include <QVBoxLayout>

ChatLogWindow::ChatLogWindow(ConversationLog *log, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("本次运行的对话记录"));
    resize(520, 620);
    auto *layout = new QVBoxLayout(this);
    browser_ = new QTextBrowser(this);
    browser_->setOpenExternalLinks(false);
    browser_->setStyleSheet(QStringLiteral("QTextBrowser { background:#20242b; color:#edf0f1; border:1px solid #4f555d; padding:12px; }"));
    layout->addWidget(browser_);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    layout->addWidget(buttons);
    for (const ConversationLog::Entry &entry : log->entries()) appendEntry(entry);
    connect(log, &ConversationLog::entryAdded, this, &ChatLogWindow::appendEntry);
}

void ChatLogWindow::appendEntry(const ConversationLog::Entry &entry)
{
    const QString speaker = entry.speaker == ConversationLog::Speaker::User ? QStringLiteral("你") : QStringLiteral("冰织");
    const QString color = entry.speaker == ConversationLog::Speaker::User ? QStringLiteral("#b7d7ff") : QStringLiteral("#dfc8a7");
    browser_->append(QStringLiteral("<p style='margin:0 0 14px 0'><span style='color:%1;font-weight:600'>%2</span> <span style='color:#858b92;font-size:10px'>%3</span><br><span style='color:#edf0f1'>%4</span></p>")
        .arg(color, speaker, entry.timestamp.toString(QStringLiteral("HH:mm:ss")), entry.text.toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br>"))));
    browser_->verticalScrollBar()->setValue(browser_->verticalScrollBar()->maximum());
}
