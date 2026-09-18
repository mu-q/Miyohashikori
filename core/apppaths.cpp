#include "apppaths.h"

#include <QCoreApplication>
#include <QDir>

namespace {

bool hasModeSprites(const QString &assetsPath)
{
    const QDir modeDir(QDir::cleanPath(assetsPath + QStringLiteral("/modes/default")));
    if (!modeDir.exists())
        return false;

    const QStringList files = modeDir.entryList(
        {QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
         QStringLiteral("*.webp")},
        QDir::Files);
    return !files.isEmpty();
}

bool hasVoiceFiles(const QString &voicePath)
{
    const QDir voiceDir(voicePath);
    return voiceDir.exists(QStringLiteral("ko/ko0007.ogg"));
}

} // namespace

namespace AppPaths {

QString appDataRoot()
{
    return QDir::cleanPath(QDir::homePath() + QStringLiteral("/.hyori"));
}

QString configFilePath()
{
    return QDir::cleanPath(appDataRoot() + QStringLiteral("/config.json"));
}

QString databaseFilePath()
{
    return QDir::cleanPath(appDataRoot() + QStringLiteral("/hyori.db"));
}

QString logsRoot()
{
    return QDir::cleanPath(appDataRoot() + QStringLiteral("/logs"));
}

QString conversationHistoryFilePath()
{
    return QDir::cleanPath(appDataRoot() + QStringLiteral("/conversation-history.json"));
}

QString ttsCacheRoot()
{
    return QDir::cleanPath(appDataRoot() + QStringLiteral("/cache/tts"));
}

QString assetsRoot()
{
    QStringList candidates;
    const QString besideExe =
        QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/assets"));
    candidates.append(besideExe);

    QDir walkDir(QCoreApplication::applicationDirPath());
    for (int depth = 0; depth < 8; ++depth) {
        const QString walked = QDir::cleanPath(walkDir.filePath(QStringLiteral("assets")));
        if (!candidates.contains(walked))
            candidates.append(walked);
        if (!walkDir.cdUp())
            break;
    }

#ifdef HYORI_SOURCE_DIR
    const QString inSource =
        QDir::cleanPath(QStringLiteral(HYORI_SOURCE_DIR) + QStringLiteral("/assets"));
    if (!candidates.contains(inSource))
        candidates.append(inSource);
#endif

    const QString cwdAssets = QDir::cleanPath(QDir::currentPath() + QStringLiteral("/assets"));
    if (!candidates.contains(cwdAssets))
        candidates.append(cwdAssets);

    for (const QString &path : candidates) {
        if (hasModeSprites(path))
            return path;
    }

    return besideExe;
}

QString modesRoot()
{
    return QDir::cleanPath(assetsRoot() + QStringLiteral("/modes"));
}

QString voiceRoot()
{
    QStringList candidates;
    const QString besideExe = QDir::cleanPath(
        QCoreApplication::applicationDirPath() + QStringLiteral("/resources/voice"));
    candidates.append(besideExe);

    QDir walkDir(QCoreApplication::applicationDirPath());
    for (int depth = 0; depth < 8; ++depth) {
        const QString walked = QDir::cleanPath(
            walkDir.filePath(QStringLiteral("resources/voice")));
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

    const QString cwdVoice = QDir::cleanPath(
        QDir::currentPath() + QStringLiteral("/resources/voice"));
    if (!candidates.contains(cwdVoice))
        candidates.append(cwdVoice);

    for (const QString &path : candidates) {
        if (hasVoiceFiles(path))
            return path;
    }
    return besideExe;
}

QString defaultTtsReferenceAudioPath()
{
    return QDir(voiceRoot()).filePath(QStringLiteral("ko/ko0007.ogg"));
}

} // namespace AppPaths
