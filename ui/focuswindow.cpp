#include "focuswindow.h"

#include "../core/ai/iaisession.h"
#include "../core/conversationlog.h"
#include "../core/config/appconfig.h"
#include "../core/config/configmanager.h"
#include "../core/pomodorocontroller.h"
#include "../core/data/schedulerepository.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMediaPlayer>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QSpinBox>
#include <QStackedLayout>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QToolButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoWidget>
#include "chatlogwindow.h"
#include "coursesidebar.h"

namespace {
QString focusStyle()
{
    return QStringLiteral(R"(
        QWidget#FocusWindow { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #263550, stop:0.54 #171c2d, stop:1 #2d2341); }
        QDialog, QMessageBox { background: #171c2d; }
        QLabel { color: #edf4ff; font-family: "Microsoft YaHei UI"; }
        QLabel#brand { color: #edf4ff; font-size: 19px; font-weight: 700; }
        QLabel#eyebrow { color: #b9cbed; font-size: 12px; font-weight: 600; letter-spacing: 2px; }
        QLabel#headerDate { color: #9eacd0; font-size: 12px; }
        QLabel#headerTime { color: #edf4ff; font-size: 30px; font-weight: 300; letter-spacing: 2px; }
        QLabel#timer { color: #f7f9ff; font-size: 54px; font-weight: 300; letter-spacing: 2px; }
        QLabel#cycles { color: #aab7d5; font-size: 12px; }
        QLabel#sceneHint { color: #9cabca; font-size: 12px; }
        QFrame#scene { background: rgba(15, 21, 37, 118); border: 1px solid rgba(191, 212, 242, 46); border-radius: 24px; }
        QFrame#timerRing { background: rgba(20, 25, 44, 220); border: 3px solid #8fcbea; border-radius: 116px; }
        QFrame#dialogue, QFrame#durations { background: rgba(18, 24, 41, 210); border: 1px solid rgba(191, 212, 242, 70); border-radius: 14px; }
        QWidget#courseSidebar { background: rgba(18, 24, 41, 218); border: 1px solid rgba(191, 212, 242, 70); border-radius: 16px; }
        QLabel#sidebarTitle { color: #f4f7ff; font-size: 20px; font-weight: 600; }
        QLabel#scheduleMeta { color: #9eacd0; font-size: 12px; }
        QLabel#nextCourse { background: rgba(112, 136, 183, 92); color: #f5f7ff; border-left: 3px solid #bca9ef; border-radius: 8px; padding: 11px; font-size: 13px; }
        QLabel#speaker { color: #bca9ef; font-weight: 700; font-size: 13px; }
        QLabel#dialogueText { color: #edf2fc; font-size: 14px; }
        QLineEdit { background: rgba(18, 24, 41, 225); color: #edf4ff; border: 1px solid rgba(191, 212, 242, 90); border-radius: 12px; padding: 10px 13px; font-size: 14px; }
        QLineEdit:focus { border: 1px solid #bca9ef; }
        QPushButton, QToolButton { background: rgba(47, 57, 82, 218); color: #edf4ff; border: 1px solid rgba(191, 212, 242, 82); border-radius: 10px; padding: 8px 14px; min-width: 58px; }
        QPushButton:hover, QToolButton:hover { background: rgba(92, 81, 131, 225); border-color: #cbbcf0; }
        QPushButton:pressed, QToolButton:pressed { background: rgba(39, 45, 66, 240); }
        QToolButton#timerControl { min-width: 58px; border-radius: 18px; background: #bcd8f5; color: #1b243a; font-weight: 700; }
        QSpinBox { background: rgba(15, 20, 35, 220); color: #edf4ff; border: 1px solid rgba(191, 212, 242, 80); border-radius: 7px; padding: 5px; min-width: 42px; }
        QComboBox, QDateEdit, QTimeEdit { background: rgba(15, 20, 35, 220); color: #edf4ff; border: 1px solid rgba(191, 212, 242, 80); border-radius: 8px; padding: 6px 8px; }
        QComboBox QAbstractItemView { background: #202941; color: #edf4ff; selection-background-color: #625680; }
        QListWidget#courseList { background: transparent; color: #eef3fc; border: none; outline: none; }
        QPushButton#viewSchedule { color: #dceaff; padding-left: 10px; padding-right: 10px; }
        QPushButton#importSpreadsheet { color: #d8c9ff; }
        QPushButton#smallCourseAction { min-width: 38px; padding: 7px 8px; }
    )");
}
}

FocusWindow::FocusWindow(ConfigManager *configManager, IAiSession *ai, ConversationLog *conversationLog,
                         ChatLogWindow *chatLogWindow, ScheduleRepository *scheduleRepository,
                         QWidget *parent)
    : QWidget(parent), configManager_(configManager), ai_(ai), conversationLog_(conversationLog), chatLogWindow_(chatLogWindow), pomodoro_(new PomodoroController(configManager, this)),
      videoPlayer_(new QMediaPlayer(this)), videoWidget_(new QVideoWidget(this)), background_(new QWidget(this)),
      trayIcon_(new QSystemTrayIcon(style()->standardIcon(QStyle::SP_ComputerIcon), this))
{
    setObjectName(QStringLiteral("FocusWindow"));
    setWindowFlag(Qt::Window, true);
    setWindowTitle(QStringLiteral("冰织 · 陪伴与专注"));
    setWindowIcon(QIcon(QStringLiteral(":/resources/icons/hyori_chibi.png")));
    resize(1280, 780);
    setMinimumSize(820, 560);
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
    root->setContentsMargins(34, 24, 34, 28);
    root->setSpacing(14);
    auto *header = new QHBoxLayout;
    auto *identity = new QVBoxLayout;
    identity->setSpacing(1);
    auto *title = new QLabel(QStringLiteral("冰织的陪伴空间"));
    title->setObjectName(QStringLiteral("brand"));
    dateLabel_ = new QLabel;
    dateLabel_->setObjectName(QStringLiteral("headerDate"));
    identity->addWidget(title);
    identity->addWidget(dateLabel_);
    timeLabel_ = new QLabel;
    timeLabel_->setObjectName(QStringLiteral("headerTime"));
    cyclesLabel_ = new QLabel;
    cyclesLabel_->setObjectName(QStringLiteral("cycles"));
    auto *settings = new QToolButton;
    settings->setText(QStringLiteral("时长"));
    settings->setToolTip(QStringLiteral("设置专注与休息时长"));
    auto *history = new QToolButton;
    history->setText(QStringLiteral("记录"));
    history->setToolTip(QStringLiteral("查看本次对话记录"));
    auto *records = new QToolButton;
    records->setText(QStringLiteral("手账"));
    records->setToolTip(QStringLiteral("打开日记与笔记"));
    header->addLayout(identity);
    header->addSpacing(20);
    header->addWidget(timeLabel_);
    header->addStretch();
    header->addWidget(cyclesLabel_);
    header->addSpacing(12);
    header->addWidget(records);
    header->addWidget(history);
    header->addWidget(settings);
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

    auto *content = new QHBoxLayout;
    content->setSpacing(16);
    auto *mainColumn = new QVBoxLayout;
    mainColumn->setSpacing(12);
    auto *scene = new QFrame;
    scene->setObjectName(QStringLiteral("scene"));
    auto *sceneLayout = new QHBoxLayout(scene);
    sceneLayout->setContentsMargins(24, 10, 34, 10);
    sceneLayout->setSpacing(18);
    auto *character = new QLabel;
    character->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    character->setPixmap(QPixmap(QStringLiteral(":/assets/modes/default/neutral.png"))
                             .scaled(300, 410, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    character->setMinimumWidth(320);
    auto *characterColumn = new QVBoxLayout;
    characterColumn->addStretch();
    characterColumn->addWidget(character, 1, Qt::AlignHCenter | Qt::AlignBottom);
    auto *sceneHint = new QLabel(QStringLiteral("今天也一起，按自己的节奏向前。"));
    sceneHint->setObjectName(QStringLiteral("sceneHint"));
    sceneHint->setAlignment(Qt::AlignCenter);
    characterColumn->addWidget(sceneHint);
    sceneLayout->addLayout(characterColumn, 1);

    auto *clock = new QFrame;
    clock->setObjectName(QStringLiteral("timerRing"));
    clock->setFixedSize(232, 232);
    auto *clockLayout = new QVBoxLayout(clock);
    clockLayout->setContentsMargins(24, 28, 24, 24);
    clockLayout->setSpacing(4);
    phaseLabel_ = new QLabel; phaseLabel_->setObjectName(QStringLiteral("eyebrow")); phaseLabel_->setAlignment(Qt::AlignCenter);
    timerLabel_ = new QLabel; timerLabel_->setObjectName(QStringLiteral("timer")); timerLabel_->setAlignment(Qt::AlignCenter);
    clockLayout->addWidget(phaseLabel_); clockLayout->addWidget(timerLabel_);
    auto *controls = new QHBoxLayout;
    controls->setSpacing(8);
    controls->addStretch();
    playButton_ = new QToolButton;
    playButton_->setObjectName(QStringLiteral("timerControl"));
    auto *skip = new QToolButton;
    skip->setText(QStringLiteral("跳过"));
    skip->setToolTip(QStringLiteral("进入下一个阶段"));
    controls->addWidget(playButton_);
    controls->addWidget(skip);
    controls->addStretch();
    clockLayout->addLayout(controls);
    sceneLayout->addWidget(clock, 0, Qt::AlignCenter);
    mainColumn->addWidget(scene, 1);

    auto *dialogue = new QFrame; dialogue->setObjectName(QStringLiteral("dialogue"));
    auto *dialogueLayout = new QVBoxLayout(dialogue); dialogueLayout->setContentsMargins(18, 11, 18, 13); dialogueLayout->setSpacing(5);
    auto *speaker = new QLabel(QStringLiteral("冰织")); speaker->setObjectName(QStringLiteral("speaker"));
    dialogueText_ = new QLabel(QStringLiteral("准备好了就开始吧。我会在这里陪着你。")); dialogueText_->setObjectName(QStringLiteral("dialogueText")); dialogueText_->setWordWrap(true);
    dialogueLayout->addWidget(speaker); dialogueLayout->addWidget(dialogueText_); mainColumn->addWidget(dialogue);
    input_ = new QLineEdit; input_->setPlaceholderText(QStringLiteral("和冰织说点什么…（回车发送）")); input_->setClearButtonEnabled(true); mainColumn->addWidget(input_);
    content->addLayout(mainColumn, 1);
    courseSidebar_ = new CourseSidebar(scheduleRepository, overlay);
    content->addWidget(courseSidebar_);
    root->addLayout(content, 1);

    connect(settings, &QToolButton::clicked, this, &FocusWindow::toggleDurationEditor);
    connect(records, &QToolButton::clicked, this, &FocusWindow::recordsRequested);
    connect(history, &QToolButton::clicked, this, [this] { if (chatLogWindow_) { chatLogWindow_->show(); chatLogWindow_->raise(); chatLogWindow_->activateWindow(); } });
    connect(saveDurations, &QPushButton::clicked, this, &FocusWindow::saveDurations);
    connect(playButton_, &QToolButton::clicked, this, [this] { pomodoro_->isRunning() ? pomodoro_->pause() : pomodoro_->start(); });
    connect(skip, &QToolButton::clicked, pomodoro_, &PomodoroController::skip);
    connect(input_, &QLineEdit::returnPressed, this, &FocusWindow::sendChat);
    connect(courseSidebar_, &CourseSidebar::statusMessage, this, &FocusWindow::setDialogue);
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
    auto *dateTimeTimer = new QTimer(this);
    dateTimeTimer->setInterval(1000);
    connect(dateTimeTimer, &QTimer::timeout, this, &FocusWindow::updateDateTime);
    dateTimeTimer->start();
    updateDateTime(); updatePhase(pomodoro_->phaseName()); updateTimer(pomodoro_->remainingSeconds()); updateRunning(false); reloadBackground();
}

void FocusWindow::showCourseReminder(const QString &text)
{
    setDialogue(text);
    showNotification(QStringLiteral("冰织的课程提醒"), text);
    if (courseSidebar_)
        courseSidebar_->refresh();
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

void FocusWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (initialPlacementApplied_)
        return;

    QScreen *screen = parentWidget()
        ? QGuiApplication::screenAt(parentWidget()->frameGeometry().center())
        : QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const QRect available = screen->availableGeometry();
    resize(qMin(1280, available.width() - 48), qMin(780, available.height() - 48));
    move(available.center() - rect().center());
    initialPlacementApplied_ = true;
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
    background_->setStyleSheet(QStringLiteral("background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #263550, stop:0.52 #171c2d, stop:1 #2d2341);"));
}

void FocusWindow::updateDateTime()
{
    const QDateTime now = QDateTime::currentDateTime();
    const QLocale chinese(QLocale::Chinese, QLocale::China);
    dateLabel_->setText(chinese.toString(now.date(), QStringLiteral("yyyy 年 MM 月 dd 日  dddd")));
    timeLabel_->setText(now.time().toString(QStringLiteral("HH:mm")));
}
