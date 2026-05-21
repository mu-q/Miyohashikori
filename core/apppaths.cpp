#include "apppaths.h"

#include <QCoreApplication>
#include <QDir>

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
    return QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/assets"));
}

QString modesRoot()
{
    return QDir::cleanPath(assetsRoot() + QStringLiteral("/modes"));
}

} // namespace AppPaths
