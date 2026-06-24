#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QPoint>
#include <QWidget>

class CharacterSpriteView;
class ConfigManager;
class IAiSession;
class QLineEdit;
class QCloseEvent;
class QShowEvent;
class QRect;
class ReplyBubble;
class SpriteCatalog;
class VoicePlayer;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void applyWindowChrome();
    void persistWindowPosition() const;
    void wireAiSession();
    void refreshConfigHint();
    void setReplyStatus(const QString &text);
    void setReplyMessage(const QString &text);
    void setReplyError(const QString &text);
    void setInputWaiting(bool waiting);
    void syncChromeToSprite();
    void applyWindowPlacement();
    void clampWindowToScreen(const QRect &avail);
    void showPetMenu(const QPoint &globalPos);

    void startDrag(const QPoint &globalPress);
    void updateDrag(const QPoint &globalPos);
    void endDrag();

    SpriteCatalog *catalog_ = nullptr;
    CharacterSpriteView *sprite_ = nullptr;
    ReplyBubble *replyBubble_ = nullptr;
    QLineEdit *inputLine_ = nullptr;
    ConfigManager *configManager_ = nullptr;
    IAiSession *ai_ = nullptr;
    VoicePlayer *voicePlayer_ = nullptr;
    QString lastAssistantText_;

    bool dragging_ = false;
    bool draggingStarted_ = false;
    QPoint dragOffset_;
};

#endif // MAINWINDOW_H
