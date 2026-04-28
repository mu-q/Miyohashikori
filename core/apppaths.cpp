#include "apppaths.h"

#include <QCoreApplication>
#include <QDir>

namespace AppPaths {

QString assetsRoot()
{
    return QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/assets"));
}

QString modesRoot()
{
    return QDir::cleanPath(assetsRoot() + QStringLiteral("/modes"));
}

} // namespace AppPaths
