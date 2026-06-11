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

QString logsRoot()
{
    return QDir::cleanPath(appDataRoot() + QStringLiteral("/logs"));
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

} // namespace AppPaths
