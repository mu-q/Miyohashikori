#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class AppConfig;
class QAudioOutput;
class QMediaPlayer;

class VoicePlayer : public QObject
{
    Q_OBJECT

public:
    explicit VoicePlayer(QObject *parent = nullptr);

    void playEmotion(const QString &emotion, const AppConfig &config);
    void stop();

private:
    QStringList voiceCandidatesForEmotion(const QString &emotion) const;
    QString resolveVoiceRoot() const;
    QString pickRandomVoice(const QStringList &relativePaths) const;
    void setVolume(double volume);

    QMediaPlayer *player_ = nullptr;
    QAudioOutput *audioOutput_ = nullptr;
};
