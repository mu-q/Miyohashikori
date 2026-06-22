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
        QStringLiteral("me/me0001.ogg"),
        QStringLiteral("me/me0002.ogg"),
        QStringLiteral("me/me0003.ogg"),
        QStringLiteral("me/me0004.ogg"),
        QStringLiteral("me/me0005.ogg"),
        QStringLiteral("me/me0006.ogg")
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
            QStringLiteral("me/me0007.ogg"),
            QStringLiteral("me/me0008.ogg"),
            QStringLiteral("me/me0011.ogg"),
            QStringLiteral("me/me0012.ogg")
        };
    }

    if (normalized == QStringLiteral("shy")) {
        return {
            QStringLiteral("me/me0013.ogg"),
            QStringLiteral("me/me0014.ogg"),
            QStringLiteral("me/me0015.ogg"),
            QStringLiteral("me/me0016.ogg")
        };
    }

    if (normalized == QStringLiteral("concerned")) {
        return {
            QStringLiteral("me/me0017.ogg"),
            QStringLiteral("me/me0018.ogg"),
            QStringLiteral("me/me0019.ogg"),
            QStringLiteral("me/me0020.ogg")
        };
    }

    if (normalized == QStringLiteral("excited")) {
        return {
            QStringLiteral("me/me0021.ogg"),
            QStringLiteral("me/me0022.ogg"),
            QStringLiteral("me/me0023.ogg"),
            QStringLiteral("me/me0024.ogg")
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
