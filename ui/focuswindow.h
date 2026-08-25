#pragma once

#include <QWidget>

class ConfigManager;
class IAiSession;
class ConversationLog;
class ChatLogWindow;
class PomodoroController;
class QFrame;
class QLabel;
class QLineEdit;
class QMediaPlayer;
class QSpinBox;
class QSystemTrayIcon;
class QToolButton;
class QVideoWidget;

class FocusWindow : public QWidget
{
    Q_OBJECT
public:
    explicit FocusWindow(ConfigManager *configManager, IAiSession *ai, ConversationLog *conversationLog,
                         ChatLogWindow *chatLogWindow, QWidget *parent = nullptr);
    void reloadBackground();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void updateTimer(int seconds);
    void updatePhase(const QString &name);
    void updateRunning(bool running);
    void setDialogue(const QString &text, bool isError = false);
    void sendChat();
    void toggleDurationEditor();
    void saveDurations();
    void showNotification(const QString &title, const QString &body);
    void useFallbackBackground();

    ConfigManager *configManager_ = nullptr;
    IAiSession *ai_ = nullptr;
    ConversationLog *conversationLog_ = nullptr;
    ChatLogWindow *chatLogWindow_ = nullptr;
    PomodoroController *pomodoro_ = nullptr;
    QMediaPlayer *videoPlayer_ = nullptr;
    QVideoWidget *videoWidget_ = nullptr;
    QWidget *background_ = nullptr;
    QLabel *phaseLabel_ = nullptr;
    QLabel *timerLabel_ = nullptr;
    QLabel *cyclesLabel_ = nullptr;
    QLabel *dialogueText_ = nullptr;
    QLineEdit *input_ = nullptr;
    QToolButton *playButton_ = nullptr;
    QFrame *durationPanel_ = nullptr;
    QSpinBox *workSpin_ = nullptr;
    QSpinBox *shortSpin_ = nullptr;
    QSpinBox *longSpin_ = nullptr;
    QSystemTrayIcon *trayIcon_ = nullptr;
};
