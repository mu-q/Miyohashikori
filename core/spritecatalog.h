#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QStringList>

class QTimer;

/// 扫描 `assets/modes/<模式名>/` 下按 emotion 命名的立绘，驱动当前表情显示。
class SpriteCatalog : public QObject
{
    Q_OBJECT

public:
    static constexpr int kNeutralRestoreMs = 20000;

    explicit SpriteCatalog(QObject *parent = nullptr);

    QStringList modeNames() const;
    QString currentMode() const;
    bool setMode(const QString &name);

    QString currentEmotion() const;
    QStringList availableEmotions() const;
    bool setEmotion(const QString &emotion);

    QImage currentImage() const;

public slots:
    void restoreNeutral();

signals:
    void modeChanged(const QString &name);
    void emotionChanged(const QString &emotion);

private:
    void rescanCurrentMode();
    bool loadEmotionImages();
    void scheduleNeutralRestore();

    QString modesBase_;
    QStringList modeNames_;
    QString currentMode_;
    QHash<QString, QImage> emotionImages_;
    QString currentEmotion_;
    QTimer *neutralRestoreTimer_ = nullptr;
};
