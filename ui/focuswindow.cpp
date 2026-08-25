#include "focuswindow.h"

#include "../core/ai/iaisession.h"
#include "../core/conversationlog.h"
#include "../core/config/appconfig.h"
#include "../core/config/configmanager.h"
#include "../core/pomodorocontroller.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QPushButton>
#include <QResizeEvent>
#include <QSpinBox>
#include <QStackedLayout>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoWidget>
#include "chatlogwindow.h"

namespace {
QString focusStyle()
{
    return QStringLiteral(R"(
        QWidget#FocusWindow { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #30343b, stop:0.52 #20242b, stop:1 #171a20); }
        QLabel { color: #edf0f1; }
        QLabel#eyebrow { color: #c5b9a8; font-size: 12px; font-weight: 600; letter-spacing: 2px; }
        QLabel#timer { color: #f6f1e9; font-size: 68px; font-weight: 300; letter-spacing: 3px; }
        QLabel#cycles { color: #aeb3b7; font-size: 12px; }
        QFrame#glass, QFrame#dialogue, QFrame#durations { background: rgba(17, 20, 25, 190); border: 1px solid rgba(222, 209, 188, 70); border-radius: 10px; }
        QLabel#speaker { color: #dfc8a7; font-weight: 600; font-size: 13px; }
        QLabel#dialogueText { color: #f1f2f3; font-size: 15px; }
        QLineEdit { background: rgba(11, 13, 17, 218); color: #f1f2f3; border: 1px solid rgba(222, 209, 188, 110); border-radius: 8px; padding: 10px 12px; font-size: 14px; }
        QLineEdit:focus { border: 1px solid #dfc8a7; }
        QPushButton, QToolButton { background: rgba(33, 38, 46, 218); color: #f2eee8; border: 1px solid rgba(222, 209, 188, 105); border-radius: 7px; padding: 8px 14px; min-width: 62px; }
        QPushButton:hover, QToolButton:hover { background: rgba(84, 78, 72, 230); border-color: #dfc8a7; }
        QPushButton:pressed, QToolButton:pressed { background: rgba(15, 17, 21, 240); }
        QSpinBox { background: rgba(10, 12, 16, 210); color: #f2eee8; border: 1px solid rgba(222, 209, 188, 90); border-radius: 5px; padding: 5px; min-width: 42px; }
    )");
}
}

FocusWindow::FocusWindow(ConfigManager *configManager, IAiSession *ai, ConversationLog *conversationLog,
                         ChatLogWindow *chatLogWindow, QWidget *parent)
    : QWidget(parent), configManager_(configManager), ai_(ai), conversationLog_(conversationLog), chatLogWindow_(chatLogWindow), pomodoro_(new PomodoroController(configManager, this)),
      videoPlayer_(new QMediaPlayer(this)), videoWidget_(new QVideoWidget(this)), background_(new QWidget(this)),
      trayIcon_(new QSystemTrayIcon(style()->standardIcon(QStyle::SP_ComputerIcon), this))
{
    setObjectName(QStringLiteral("FocusWindow"));
    setWindowFlag(Qt::Window, true);
    setWindowTitle(QStringLiteral("冰织 · 专注时间"));
    resize(900, 660);
    setMinimumSize(640, 520);
    setStyleSheet(focusStyle());

    videoWidget_->setParent(background_);
    videoWidget_->setAspectRatioMode(Qt::KeepAspectRatioByExpanding);
    videoPlayer_->setVideoOutput(videoWidget_);
    connect(videoPlayer_, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) videoPlayer_->play();
        if (status == QMediaPlayer::InvalidMedia) useFallbackBackground();
    });
    connect(videoPlayer_, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &) { useFallbackBackground(); });

    auto *stack = new QStackedLayout(this);
    stack->setStackingMode(QStackedLayout::StackAll);
    stack->setContentsMargins(0, 0, 0, 0);
    stack->addWidget(background_);
    auto *overlay = new QWidget(this);
    stack->addWidget(overlay);
    // StackAll 会将 current widget 置于顶层；确保交互层不会被视频/降级背景遮住。
    stack->setCurrentWidget(overlay);

    auto *root = new QVBoxLayout(overlay);
    root->setContentsMargins(42, 30, 42, 32);
    root->setSpacing(12);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("冰织的专注时间"));
    title->setObjectName(QStringLiteral("eyebrow"));
    cyclesLabel_ = new QLabel;
    cyclesLabel_->setObjectName(QStringLiteral("cycles"));
    auto *settings = new QToolButton;
    settings->setText(QStringLiteral("时长"));
    auto *history = new QToolButton;
    history->setText(QStringLiteral("记录"));
    header->addWidget(title); header->addStretch(); header->addWidget(cyclesLabel_); header->addSpacing(12); header->addWidget(history); header->addWidget(settings);
    root->addLayout(header);

    durationPanel_ = new QFrame;
    durationPanel_->setObjectName(QStringLiteral("durations"));
    auto *durations = new QHBoxLayout(durationPanel_);
    durations->setContentsMargins(14, 8, 14, 8);
    workSpin_ = new QSpinBox; shortSpin_ = new QSpinBox; longSpin_ = new QSpinBox;
    for (QSpinBox *spin : {workSpin_, shortSpin_, longSpin_}) { spin->setSuffix(QStringLiteral(" 分")); spin->setKeyboardTracking(false); }
    workSpin_->setRange(1, 180); shortSpin_->setRange(1, 60); longSpin_->setRange(1, 120);
    const AppConfig &config = configManager_->config();
    workSpin_->setValue(config.pomodoroWorkMinutes); shortSpin_->setValue(config.pomodoroShortBreakMinutes); longSpin_->setValue(config.pomodoroLongBreakMinutes);
    durations->addWidget(new QLabel(QStringLiteral("专注"))); durations->addWidget(workSpin_);
    durations->addSpacing(12); durations->addWidget(new QLabel(QStringLiteral("短休息"))); durations->addWidget(shortSpin_);
    durations->addSpacing(12); durations->addWidget(new QLabel(QStringLiteral("长休息"))); durations->addWidget(longSpin_); durations->addStretch();
    auto *saveDurations = new QPushButton(QStringLiteral("保存时长")); durations->addWidget(saveDurations);
    durationPanel_->hide(); root->addWidget(durationPanel_);

    root->addStretch(3);
    auto *clock = new QFrame; clock->setObjectName(QStringLiteral("glass"));
    auto *clockLayout = new QVBoxLayout(clock); clockLayout->setContentsMargins(44, 27, 44, 24); clockLayout->setSpacing(2);
    phaseLabel_ = new QLabel; phaseLabel_->setObjectName(QStringLiteral("eyebrow")); phaseLabel_->setAlignment(Qt::AlignCenter);
    timerLabel_ = new QLabel; timerLabel_->setObjectName(QStringLiteral("timer")); timerLabel_->setAlignment(Qt::AlignCenter);
    clockLayout->addWidget(phaseLabel_); clockLayout->addWidget(timerLabel_);
    root->addWidget(clock, 0, Qt::AlignHCenter);
    auto *controls = new QHBoxLayout; controls->setSpacing(10);
    controls->addStretch(); playButton_ = new QToolButton; auto *skip = new QToolButton; skip->setText(QStringLiteral("跳过")); controls->addWidget(playButton_); controls->addWidget(skip); controls->addStretch(); root->addLayout(controls);
    root->addStretch(2);

    auto *dialogue = new QFrame; dialogue->setObjectName(QStringLiteral("dialogue"));
    auto *dialogueLayout = new QVBoxLayout(dialogue); dialogueLayout->setContentsMargins(18, 11, 18, 13); dialogueLayout->setSpacing(5);
    auto *speaker = new QLabel(QStringLiteral("冰织")); speaker->setObjectName(QStringLiteral("speaker"));
    dialogueText_ = new QLabel(QStringLiteral("准备好了就开始吧。我会在这里陪着你。")); dialogueText_->setObjectName(QStringLiteral("dialogueText")); dialogueText_->setWordWrap(true);
    dialogueLayout->addWidget(speaker); dialogueLayout->addWidget(dialogueText_); root->addWidget(dialogue);
    input_ = new QLineEdit; input_->setPlaceholderText(QStringLiteral("和冰织说点什么…（回车发送）")); input_->setClearButtonEnabled(true); root->addWidget(input_);

    connect(settings, &QToolButton::clicked, this, &FocusWindow::toggleDurationEditor);
    connect(history, &QToolButton::clicked, this, [this] { if (chatLogWindow_) { chatLogWindow_->show(); chatLogWindow_->raise(); chatLogWindow_->activateWindow(); } });
    connect(saveDurations, &QPushButton::clicked, this, &FocusWindow::saveDurations);
    connect(playButton_, &QToolButton::clicked, this, [this] { pomodoro_->isRunning() ? pomodoro_->pause() : pomodoro_->start(); });
    connect(skip, &QToolButton::clicked, pomodoro_, &PomodoroController::skip);
    connect(input_, &QLineEdit::returnPressed, this, &FocusWindow::sendChat);
    connect(pomodoro_, &PomodoroController::tick, this, &FocusWindow::updateTimer);
    connect(pomodoro_, &PomodoroController::phaseChanged, this, [this](PomodoroController::Phase, const QString &name) { updatePhase(name); });
    connect(pomodoro_, &PomodoroController::runningChanged, this, &FocusWindow::updateRunning);
    connect(pomodoro_, &PomodoroController::completed, this, [this](PomodoroController::Phase finished) {
        const bool workFinished = finished == PomodoroController::Phase::Work;
        const QString body = workFinished ? QStringLiteral("一轮专注完成，休息一下吧。") : QStringLiteral("休息结束，准备继续专注。 ");
        showNotification(QStringLiteral("冰织的番茄钟"), body);
        QApplication::beep();
        setDialogue(body);
        if (ai_) ai_->submit(workFinished ? QStringLiteral("我刚完成了一轮番茄钟，请用一句话温柔地提醒我休息。") : QStringLiteral("我的休息时间结束了，请用一句话陪我开始专注。"));
    });
    if (ai_) {
        connect(ai_, &IAiSession::assistantMessage, this, [this](const QString &text) { setDialogue(text); input_->setEnabled(true); });
        connect(ai_, &IAiSession::sessionStatus, this, [this](const QString &text) { setDialogue(text); });
        connect(ai_, &IAiSession::sessionError, this, [this](const QString &text) { setDialogue(text, true); input_->setEnabled(true); });
    }
    updatePhase(pomodoro_->phaseName()); updateTimer(pomodoro_->remainingSeconds()); updateRunning(false); reloadBackground();
}

void FocusWindow::reloadBackground()
{
    const QString path = configManager_->config().backgroundVideoPath;
    if (path.isEmpty()) { useFallbackBackground(); return; }
    videoWidget_->show(); background_->setStyleSheet(QString());
    videoPlayer_->stop(); videoPlayer_->setSource(QUrl::fromLocalFile(path)); videoPlayer_->play();
}

void FocusWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (videoWidget_) videoWidget_->setGeometry(background_->rect());
}

void FocusWindow::closeEvent(QCloseEvent *event)
{
    pomodoro_->pause();
    QWidget::closeEvent(event);
}

void FocusWindow::updateTimer(int seconds)
{
    timerLabel_->setText(QStringLiteral("%1:%2").arg(seconds / 60, 2, 10, QLatin1Char('0')).arg(seconds % 60, 2, 10, QLatin1Char('0')));
    cyclesLabel_->setText(QStringLiteral("今日完成 %1 轮").arg(pomodoro_->completedCycles()));
}
void FocusWindow::updatePhase(const QString &name) { phaseLabel_->setText(name); }
void FocusWindow::updateRunning(bool running) { playButton_->setText(running ? QStringLiteral("暂停") : QStringLiteral("开始")); }
void FocusWindow::setDialogue(const QString &text, bool isError)
{
    dialogueText_->setText(text);
    dialogueText_->setStyleSheet(isError ? QStringLiteral("color: #efb4ad;") : QString());
}
void FocusWindow::sendChat()
{
    const QString text = input_->text().trimmed(); if (text.isEmpty() || !ai_) return;
    if (conversationLog_) conversationLog_->addUser(text);
    input_->clear(); input_->setEnabled(false); setDialogue(QStringLiteral("正在等待冰织回复…")); ai_->submit(text);
}
void FocusWindow::toggleDurationEditor() { durationPanel_->setVisible(!durationPanel_->isVisible()); }
void FocusWindow::saveDurations()
{
    pomodoro_->setDurations(workSpin_->value(), shortSpin_->value(), longSpin_->value());
    durationPanel_->hide(); setDialogue(QStringLiteral("时长已经调整好了。按自己的节奏来就好。"));
}
void FocusWindow::showNotification(const QString &title, const QString &body)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
    trayIcon_->show(); trayIcon_->showMessage(title, body, QSystemTrayIcon::Information, 5000);
}
void FocusWindow::useFallbackBackground()
{
    videoPlayer_->stop(); videoWidget_->hide();
    background_->setStyleSheet(QStringLiteral("background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #47494d, stop:0.5 #2b2d32, stop:1 #1b1d22);"));
}
