#include "chatlogwindow.h"
#include "../core/theme.h"

#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QScrollBar>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

ChatLogWindow::ChatLogWindow(ConversationLog *log, QWidget *parent)
    : QDialog(parent), log_(log)
{
    setWindowTitle(QStringLiteral("对话记录"));
    resize(520, 620);
    auto *layout = new QVBoxLayout(this);
    browser_ = new QTextBrowser(this);
    browser_->setOpenExternalLinks(false);
    applyTheme();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &ChatLogWindow::applyTheme);
    layout->addWidget(browser_);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto *clearButton = buttons->addButton(QStringLiteral("清空记录"), QDialogButtonBox::ResetRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    connect(clearButton, &QPushButton::clicked, this, [this, log] {
        if (QMessageBox::question(this, QStringLiteral("清空对话记录"),
                                  QStringLiteral("确定要删除全部本地对话记录吗？"))
            == QMessageBox::Yes) {
            log->clear();
        }
    });
    connect(log, &ConversationLog::historyCleared, browser_, &QTextBrowser::clear);
    layout->addWidget(buttons);
    for (const ConversationLog::Entry &entry : log->entries()) appendEntry(entry);
    connect(log, &ConversationLog::entryAdded, this, &ChatLogWindow::appendEntry);
}

void ChatLogWindow::appendEntry(const ConversationLog::Entry &entry)
{
    const QString speaker = entry.speaker == ConversationLog::Speaker::User ? QStringLiteral("你") : QStringLiteral("冰织");
    const bool light = ThemeManager::instance()->isLight();
    const QString color = entry.speaker == ConversationLog::Speaker::User
        ? (light ? QStringLiteral("#3f6fa9") : QStringLiteral("#b7d7ff"))
        : (light ? QStringLiteral("#7562ae") : QStringLiteral("#dfc8a7"));
    const QString timeColor = light ? QStringLiteral("#71809a") : QStringLiteral("#858b92");
    const QString textColor = light ? QStringLiteral("#253552") : QStringLiteral("#edf0f1");
    browser_->append(QStringLiteral("<p style='margin:0 0 14px 0'><span style='color:%1;font-weight:600'>%2</span> <span style='color:#858b92;font-size:10px'>%3</span><br><span style='color:#edf0f1'>%4</span></p>")
        .replace(QStringLiteral("#858b92"), timeColor)
        .replace(QStringLiteral("#edf0f1"), textColor)
        .arg(color, speaker, entry.timestamp.toString(QStringLiteral("HH:mm:ss")), entry.text.toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br>"))));
    browser_->verticalScrollBar()->setValue(browser_->verticalScrollBar()->maximum());
}

void ChatLogWindow::applyTheme()
{
    const bool light = ThemeManager::instance()->isLight();
    setStyleSheet(light
        ? QStringLiteral("QDialog { background:#f4f8ff; color:#253552; } QPushButton { background:#e2ecfb; color:#253552; border:1px solid #b6c8e3; border-radius:8px; padding:7px 13px; } QPushButton:hover { border-color:#8b78c5; }")
        : QStringLiteral("QDialog { background:#172238; color:#edf4ff; } QPushButton { background:#2e4164; color:#edf4ff; border:1px solid #5875a2; border-radius:8px; padding:7px 13px; } QPushButton:hover { border-color:#7caaf8; }"));
    browser_->setStyleSheet(light
        ? QStringLiteral("QTextBrowser { background:#ffffff; color:#253552; border:1px solid #c1d0e6; border-radius:10px; padding:12px; }")
        : QStringLiteral("QTextBrowser { background:#121c30; color:#edf0f1; border:1px solid #40587e; border-radius:10px; padding:12px; }"));
    if (!log_ || browser_->document()->isEmpty())
        return;
    browser_->clear();
    for (const ConversationLog::Entry &entry : log_->entries())
        appendEntry(entry);
}
