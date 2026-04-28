#pragma once

#include <QImage>
#include <QObject>
#include <QStringList>
#include <QVector>

/// 扫描 `assets/modes/<模式名>/` 下的位图，提供当前帧与切换（无 UI）。
class SpriteCatalog : public QObject
{
    Q_OBJECT

public:
    explicit SpriteCatalog(QObject *parent = nullptr);

    QStringList modeNames() const;
    QString currentMode() const;
    bool setMode(const QString &name);

    int frameCount() const;
    int currentFrameIndex() const;
    void setFrameIndex(int index);
    void nextFrame();

    QImage currentImage() const;

signals:
    void modeChanged(const QString &name);
    void frameChanged(int index);

private:
    void rescanCurrentMode();
    bool loadFrameFiles();

    QString modesBase_;
    QStringList modeNames_;
    QString currentMode_;
    QStringList frameFiles_;
    QVector<QImage> frames_;
    int currentIndex_ = 0;
};
