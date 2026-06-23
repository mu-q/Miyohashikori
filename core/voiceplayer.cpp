#include "voiceplayer.h"

#include "config/appconfig.h"

#include <QAudioOutput>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QRandomGenerator>
#include <QUrl>

namespace {

QStringList commonVoices()
{
    return {
        QStringLiteral("ko/ko0040.ogg"),
        QStringLiteral("ko/ko0070.ogg"),
        QStringLiteral("ko/ko3210.ogg"),
        QStringLiteral("ko/ko3231.ogg"),
        QStringLiteral("ko/ko3261.ogg"),
        QStringLiteral("ko/ko3275.ogg")
    };
}

} // namespace

VoicePlayer::VoicePlayer(QObject *parent)
    : QObject(parent)
    , player_(new QMediaPlayer(this))
    , audioOutput_(new QAudioOutput(this))
{
    player_->setAudioOutput(audioOutput_);
}

void VoicePlayer::playEmotion(const QString &emotion, const AppConfig &config)
{
    if (!config.voiceEnabled)
        return;

    setVolume(config.volume);

    const QString filePath = pickRandomVoice(voiceCandidatesForEmotion(emotion));
    if (filePath.isEmpty())
        return;

    player_->stop();
    player_->setSource(QUrl::fromLocalFile(filePath));
    player_->play();
}

void VoicePlayer::stop()
{
    player_->stop();
}

QStringList VoicePlayer::voiceCandidatesForEmotion(const QString &emotion) const
{
    const QString normalized = emotion.trimmed().toLower();

    if (normalized == QStringLiteral("happy")) {
        return {
            QStringLiteral("ko/ko3344.ogg"),
            QStringLiteral("ko/ko3258.ogg"),
            QStringLiteral("ko/ko3263.ogg"),
            QStringLiteral("ko/ko3294.ogg")
        };
    }

    if (normalized == QStringLiteral("shy")) {
        return {
            QStringLiteral("ko/ko3297.ogg"),
            QStringLiteral("ko/ko3424.ogg"),
            QStringLiteral("ko/ko3357.ogg"),
            QStringLiteral("ko/ko3385.ogg")
        };
    }

    if (normalized == QStringLiteral("concerned")) {
        return {
            QStringLiteral("ko/ko3384.ogg"),
            QStringLiteral("ko/ko0054.ogg"),
            QStringLiteral("ko/ko3236.ogg"),
            QStringLiteral("ko/ko3354.ogg")
        };
    }

    if (normalized == QStringLiteral("excited")) {
        return {
            QStringLiteral("ko/ko3247.ogg"),
            QStringLiteral("ko/ko3249.ogg"),
            QStringLiteral("ko/ko3671.ogg"),
            QStringLiteral("ko/ko3677.ogg")
        };
    }

    return commonVoices();
}

QString VoicePlayer::resolveVoiceRoot() const
{
    QStringList candidates;

    const QString besideExe = QDir::cleanPath(
        QCoreApplication::applicationDirPath() + QStringLiteral("/resources/voice"));
    candidates.append(besideExe);

    QDir walkDir(QCoreApplication::applicationDirPath());
    for (int depth = 0; depth < 8; ++depth) {
        const QString walked =
            QDir::cleanPath(walkDir.filePath(QStringLiteral("resources/voice")));
        if (!candidates.contains(walked))
            candidates.append(walked);
        if (!walkDir.cdUp())
            break;
    }

#ifdef HYORI_SOURCE_DIR
    const QString inSource = QDir::cleanPath(
        QStringLiteral(HYORI_SOURCE_DIR) + QStringLiteral("/resources/voice"));
    if (!candidates.contains(inSource))
        candidates.append(inSource);
#endif

    const QString cwdVoice =
        QDir::cleanPath(QDir::currentPath() + QStringLiteral("/resources/voice"));
    if (!candidates.contains(cwdVoice))
        candidates.append(cwdVoice);

    for (const QString &path : candidates) {
        if (QDir(path).exists())
            return path;
    }

    return besideExe;
}

QString VoicePlayer::pickRandomVoice(const QStringList &relativePaths) const
{
    if (relativePaths.isEmpty())
        return {};

    const QString voiceRoot = resolveVoiceRoot();
    QStringList existingFiles;
    existingFiles.reserve(relativePaths.size());

    for (const QString &relativePath : relativePaths) {
        const QString fullPath = QDir(voiceRoot).filePath(relativePath);
        if (QFileInfo::exists(fullPath))
            existingFiles.append(fullPath);
    }

    if (existingFiles.isEmpty())
        return {};

    const int index = QRandomGenerator::global()->bounded(existingFiles.size());
    return existingFiles.at(index);
}

void VoicePlayer::setVolume(double volume)
{
    audioOutput_->setVolume(qBound(0.0, volume, 1.0));
}
