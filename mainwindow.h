#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QPoint>
#include <QWidget>

class CharacterSpriteView;
class IAiSession;
class QLabel;
class QLineEdit;
class SpriteCatalog;

/// 桌宠主壳：无边框置顶、透明背景、立绘区 + 原型输入条 + AI 会话占位。
class MainWindow : public QWidget
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void applyWindowChrome();
    void wireAiSession();
    void syncChromeToSprite();
    void showPetMenu(const QPoint &globalPos);

    void startDrag(const QPoint &globalPress);
    void updateDrag(const QPoint &globalPos);
    void endDrag();

    SpriteCatalog *catalog_ = nullptr;
    CharacterSpriteView *sprite_ = nullptr;
    QLabel *replyLabel_ = nullptr;
    QLineEdit *inputLine_ = nullptr;
    IAiSession *ai_ = nullptr;

    bool dragging_ = false;
    bool draggingStarted_ = false;
    QPoint dragOffset_;
};

#endif // MAINWINDOW_H
