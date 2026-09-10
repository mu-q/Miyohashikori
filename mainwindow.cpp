#include "mainwindow.h"

#include "core/ai/iaisession.h"
#include "core/ai/openaichatsession.h"
#include "core/conversationlog.h"
#include "core/apppaths.h"
#include "core/config/appconfig.h"
#include "core/config/configmanager.h"
#include "core/data/databasemanager.h"
#include "core/data/schedulerepository.h"
#include "core/schedule/courseremindercontroller.h"
#include "core/spritecatalog.h"
#include "core/ttsclient.h"
#include "core/voiceplayer.h"
#include "ui/characterspriteview.h"
#include "ui/replybubble.h"
#include "ui/focuswindow.h"
#include "ui/chatlogwindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QRect>
#include <QScreen>
#include <QShowEvent>
#include <QStyleHints>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , catalog_(new SpriteCatalog(this))
    , sprite_(new CharacterSpriteView(catalog_, this))
    , replyBubble_(new ReplyBubble(this))
    , inputLine_(new QLineEdit(this))
    , configManager_(new ConfigManager(this))
    , databaseManager_(std::make_unique<DatabaseManager>())
    , ai_(nullptr)
    , voicePlayer_(new VoicePlayer(this))
    , ttsClient_(new TtsClient(this))
    , conversationLog_(new ConversationLog(this))
    , chatLogWindow_(new ChatLogWindow(conversationLog_, this))
{
    applyWindowChrome();
    configManager_->load();
    if (!databaseManager_->initialize()) {
        qWarning().noquote() << QStringLiteral("扩展数据功能初始化失败：%1")
                                    .arg(databaseManager_->errorString());
    }
    scheduleRepository_ = std::make_unique<ScheduleRepository>(databaseManager_.get());
    ai_ = new OpenAiChatSession(configManager_, this);
    courseReminder_ = new CourseReminderController(scheduleRepository_.get(), this);

    inputLine_->setPlaceholderText(QStringLiteral("输入对话…（回车发送）"));
    inputLine_->setClearButtonEnabled(true);
    inputLine_->setStyleSheet(
        QStringLiteral("QLineEdit { background: rgba(40,44,56,0.92); color: #e8eaf0; "
                       "border: 1px solid #5c6370; border-radius: 6px; padding: 6px; }"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);
    root->addWidget(sprite_, 0, Qt::AlignHCenter);
    root->addWidget(replyBubble_, 0, Qt::AlignHCenter);
    root->addWidget(inputLine_);
    setLayout(root);

    sprite_->setCursor(Qt::OpenHandCursor);
    sprite_->installEventFilter(this);

    connect(sprite_, &CharacterSpriteView::rightClicked, this, [this] {
        showPetMenu(QCursor::pos());
    });

    connect(catalog_, &SpriteCatalog::emotionChanged, this, [this](const QString &) {
        syncChromeToSprite();
    });
    connect(catalog_, &SpriteCatalog::modeChanged, this, [this](const QString &) {
        syncChromeToSprite();
    });

    connect(inputLine_, &QLineEdit::returnPressed, this, [this] {
        const QString text = inputLine_->text().trimmed();
        if (text.isEmpty())
            return;

        setReplyStatus(QStringLiteral("正在等待冰织回复…"));
        setInputWaiting(true);
        ttsClient_->cancel();
        pendingTtsRequestId_ = 0;
        voicePlayer_->stop();
        conversationLog_->addUser(text);
        ai_->submit(text);
        inputLine_->clear();
    });

    wireAiSession();
    connect(courseReminder_, &CourseReminderController::reminderDue,
            this, &MainWindow::handleCourseReminder);
    connect(courseReminder_, &CourseReminderController::reminderError, this,
            [](const QString &error) { qWarning().noquote() << QStringLiteral("课程提醒：%1").arg(error); });
    courseReminder_->start();
    refreshConfigHint();
    QTimer::singleShot(0, this, [this] { applyWindowPlacement(); });
}

MainWindow::~MainWindow() = default;

//无边框、透明背景、始终置顶、不在任务栏显示独立图标的浮动窗口
void MainWindow::applyWindowChrome()
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground,true);
    setStyleSheet(QStringLiteral("#MainWindowPet { background:transparent; }"));
    setObjectName(QStringLiteral("MainWindowPet"));
}

void MainWindow::persistWindowPosition() const
{
    AppConfig config = configManager_->config();
    config.windowPos = pos();
    configManager_->setConfig(config);
    configManager_->save();
}

void MainWindow::wireAiSession()
{
    connect(ttsClient_, &TtsClient::audioReady, this,
            [this](quint64 requestId, const QString &filePath) {
                if (requestId != pendingTtsRequestId_)
                    return;
                configManager_->load();
                voicePlayer_->playFile(filePath, configManager_->config().volume);
                pendingTtsRequestId_ = 0;
            });
    connect(ttsClient_, &TtsClient::synthesisFailed, this,
            [this](quint64 requestId, const QString &reason) {
                if (requestId != pendingTtsRequestId_)
                    return;
                qWarning().noquote() << QStringLiteral("GPT-SoVITS 合成失败，回退原作语音：%1")
                                             .arg(reason);
                configManager_->load();
                voicePlayer_->playReply(pendingVoiceText_, pendingVoiceEmotion_,
                                        configManager_->config());
                pendingTtsRequestId_ = 0;
            });

    connect(ai_, &IAiSession::assistantMessage, this, [this](const QString &text) {
        lastAssistantText_ = text;
        conversationLog_->addHyori(text);
        setReplyMessage(text);
        setInputWaiting(false);
    });
    connect(ai_, &IAiSession::assistantSpeech, this, [this](const QString &text) {
        lastAssistantSpeechText_ = text.trimmed();
    });
    connect(ai_, &IAiSession::sessionStatus, this, [this](const QString &status) {
        setReplyStatus(status);
    });
    connect(ai_, &IAiSession::sessionError, this, [this](const QString &error) {
        setReplyError(error);
        setInputWaiting(false);
    });
    connect(ai_, &IAiSession::assistantEmotion, catalog_, &SpriteCatalog::setEmotion);
    connect(ai_, &IAiSession::assistantEmotion, this, [this](const QString &emotion) {
        configManager_->load();
        const AppConfig config = configManager_->config();
        pendingVoiceText_ = lastAssistantText_;
        pendingVoiceEmotion_ = emotion;

        if (!config.voiceEnabled) {
            ttsClient_->cancel();
            pendingTtsRequestId_ = 0;
            voicePlayer_->stop();
            return;
        }

        if (ttsClient_->isConfigured(config) && !lastAssistantSpeechText_.isEmpty()) {
            AppConfig japaneseTtsConfig = config;
            japaneseTtsConfig.ttsTextLanguage = QStringLiteral("ja");
            pendingTtsRequestId_ =
                ttsClient_->synthesize(lastAssistantSpeechText_, japaneseTtsConfig);
            return;
        }

        pendingTtsRequestId_ = 0;
        voicePlayer_->playReply(lastAssistantText_, emotion, config);
    });
}

void MainWindow::refreshConfigHint()
{
    configManager_->load();
    const AppConfig &config = configManager_->config();

    if (config.llmApiKey.trimmed().isEmpty()) {
        setReplyError(QStringLiteral("未配置 API Key，请在 %1 填写 llmApiKey。")
                          .arg(AppPaths::configFilePath()));
        return;
    }
    if (config.llmEndpoint.trimmed().isEmpty() || config.llmModel.trimmed().isEmpty()) {
        setReplyError(QStringLiteral("LLM 配置不完整，请检查 %1 中的 endpoint 与 model。")
                          .arg(AppPaths::configFilePath()));
        return;
    }

    setReplyMessage(QStringLiteral("配置已就绪，输入对话开始聊天。"));
}

void MainWindow::setReplyStatus(const QString &text)
{
    replyBubble_->setTone(ReplyBubble::Tone::Status);
    replyBubble_->setText(text);
    syncChromeToSprite();
}

void MainWindow::setReplyMessage(const QString &text)
{
    replyBubble_->setTone(ReplyBubble::Tone::Message);
    replyBubble_->setText(text);
    syncChromeToSprite();
}

void MainWindow::setReplyError(const QString &text)
{
    replyBubble_->setTone(ReplyBubble::Tone::Error);
    replyBubble_->setText(text);
    syncChromeToSprite();
}

void MainWindow::setInputWaiting(bool waiting)
{
    inputLine_->setEnabled(!waiting);
    if (waiting) {
        inputLine_->setPlaceholderText(QStringLiteral("等待回复中…"));
    } else {
        inputLine_->setPlaceholderText(QStringLiteral("输入对话…（回车发送）"));
    }
}

void MainWindow::syncChromeToSprite()
{
    sprite_->adjustSize();
    replyBubble_->adjustSize();
    layout()->activate();
    adjustSize();
}

void MainWindow::applyWindowPlacement()
{
    syncChromeToSprite();

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect avail = screen->availableGeometry();
    QPoint target = configManager_->config().windowPos;
    if (target.isNull())
        target = QPoint(avail.right() - width() - 24, avail.top() + 24);

    move(target);
    if (!avail.intersects(frameGeometry()))
        move(QPoint(avail.right() - width() - 24, avail.top() + 24));

    clampWindowToScreen(avail);
    raise();
    activateWindow();
}

void MainWindow::clampWindowToScreen(const QRect &avail)
{
    QRect frame = frameGeometry();
    int x = frame.x();
    int y = frame.y();

    if (frame.width() >= avail.width())
        x = avail.left();
    else if (frame.right() > avail.right())
        x = avail.right() - frame.width();

    if (frame.height() >= avail.height())
        y = avail.top();
    else if (frame.bottom() > avail.bottom())
        y = avail.bottom() - frame.height();

    x = qBound(avail.left(), x, qMax(avail.left(), avail.right() - frame.width()));
    y = qBound(avail.top(), y, qMax(avail.top(), avail.bottom() - frame.height()));

    if (QPoint(x, y) != frame.topLeft())
        move(x, y);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    persistWindowPosition();
    QWidget::closeEvent(event);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, [this] { applyWindowPlacement(); });
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    const bool onSprite = (obj == sprite_);

    if (onSprite && event->type() == QEvent::Wheel) {
        auto *wheelEvent = static_cast<QWheelEvent *>(event);
        if (wheelEvent->modifiers() & Qt::AltModifier) {
            int delta = wheelEvent->angleDelta().y();
            if (delta == 0)
                delta = wheelEvent->angleDelta().x();
            const int dir = (delta > 0) ? 1 : (delta < 0 ? -1 : 0);
            if (dir != 0) {
                const qreal nextScale = sprite_->scale() + dir * 0.05;
                sprite_->setScale(nextScale);
                syncChromeToSprite();
            }
            wheelEvent->accept();
            return true;
        }
    }

    if (onSprite) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton
                && (mouseEvent->modifiers() & Qt::AltModifier)) {
                startDrag(mouseEvent->globalPosition().toPoint());
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            if (dragging_) {
                auto *mouseEvent = static_cast<QMouseEvent *>(event);
                updateDrag(mouseEvent->globalPosition().toPoint());
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            if (dragging_) {
                endDrag();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    return QWidget::eventFilter(obj, event);
}

void MainWindow::startDrag(const QPoint &globalPress)
{
    dragging_ = true;
    draggingStarted_ = false;
    dragOffset_ = globalPress - frameGeometry().topLeft();
    sprite_->setCursor(Qt::ClosedHandCursor);
    sprite_->grabMouse();
}

void MainWindow::updateDrag(const QPoint &globalPos)
{
    const int threshold = QGuiApplication::styleHints()->startDragDistance();
    if (!draggingStarted_) {
        const QPoint delta = globalPos - (dragOffset_ + frameGeometry().topLeft());
        if (delta.manhattanLength() < threshold)
            return;
        draggingStarted_ = true;
    }

    move(globalPos - dragOffset_);
}

void MainWindow::endDrag()
{
    sprite_->releaseMouse();
    dragging_ = false;
    draggingStarted_ = false;
    sprite_->setCursor(Qt::OpenHandCursor);
    persistWindowPosition();
}

void MainWindow::showPetMenu(const QPoint &globalPos)
{
    QMenu menu;
    menu.addAction(QStringLiteral("本次对话记录"), this, &MainWindow::showChatHistory);
    menu.addAction(QStringLiteral("打开专注模式"), this, &MainWindow::openFocusWindow);
    menu.addAction(QStringLiteral("选择背景视频"), this, &MainWindow::chooseBackgroundVideo);
    QAction *clearVideo = menu.addAction(QStringLiteral("清除背景视频"), this, &MainWindow::clearBackgroundVideo);
    clearVideo->setEnabled(!configManager_->config().backgroundVideoPath.isEmpty());
    menu.addSeparator();
    QMenu *modesMenu = menu.addMenu(QStringLiteral("切换模式"));
    const QStringList names = catalog_->modeNames();
    const QString current = catalog_->currentMode();
    if (names.isEmpty()) {
        modesMenu->addAction(QStringLiteral("（无）"))->setEnabled(false);
    } else {
        for (const QString &name : names) {
            QAction *action = modesMenu->addAction(name);
            action->setCheckable(true);
            action->setChecked(name == current);
            connect(action, &QAction::triggered, this, [this, name] { catalog_->setMode(name); });
        }
    }

    menu.addSeparator();
    menu.addAction(QStringLiteral("退出"), qApp, &QApplication::quit);
    menu.exec(globalPos);
}

void MainWindow::openFocusWindow()
{
    if (!focusWindow_)
        focusWindow_ = new FocusWindow(configManager_, ai_, conversationLog_, chatLogWindow_,
                                       scheduleRepository_.get(), this);
    focusWindow_->show();
    focusWindow_->raise();
    focusWindow_->activateWindow();
}

void MainWindow::handleCourseReminder(const CourseOccurrence &, int,
                                      const QString &displayText, const QString &speechText)
{
    conversationLog_->addHyori(displayText);
    setReplyMessage(displayText);
    catalog_->setEmotion(QStringLiteral("concerned"));
    if (focusWindow_)
        focusWindow_->showCourseReminder(displayText);

    configManager_->load();
    const AppConfig config = configManager_->config();
    if (!config.voiceEnabled)
        return;
    ttsClient_->cancel();
    voicePlayer_->stop();
    pendingVoiceText_ = displayText;
    pendingVoiceEmotion_ = QStringLiteral("concerned");
    if (ttsClient_->isConfigured(config)) {
        AppConfig japaneseTtsConfig = config;
        japaneseTtsConfig.ttsTextLanguage = QStringLiteral("ja");
        pendingTtsRequestId_ = ttsClient_->synthesize(speechText, japaneseTtsConfig);
    } else {
        pendingTtsRequestId_ = 0;
        voicePlayer_->playReply(displayText, pendingVoiceEmotion_, config);
    }
}

void MainWindow::showChatHistory()
{
    chatLogWindow_->show();
    chatLogWindow_->raise();
    chatLogWindow_->activateWindow();
}

void MainWindow::chooseBackgroundVideo()
{
    const QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择专注背景视频"),
        QFileInfo(configManager_->config().backgroundVideoPath).absolutePath(),
        QStringLiteral("视频文件 (*.mp4 *.mkv *.avi *.mov *.webm);;所有文件 (*)"));
    if (file.isEmpty())
        return;
    AppConfig config = configManager_->config();
    config.backgroundVideoPath = file;
    configManager_->setConfig(config);
    configManager_->save();
    if (focusWindow_)
        focusWindow_->reloadBackground();
}

void MainWindow::clearBackgroundVideo()
{
    AppConfig config = configManager_->config();
    config.backgroundVideoPath.clear();
    configManager_->setConfig(config);
    configManager_->save();
    if (focusWindow_)
        focusWindow_->reloadBackground();
}
