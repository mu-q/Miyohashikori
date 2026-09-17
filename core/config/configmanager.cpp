#include "configmanager.h"

#include "../apppaths.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , config_(AppConfig::defaults())
    , configFilePath_(AppPaths::configFilePath())
{
}

ConfigManager::ConfigManager(const QString &configFilePath, QObject *parent)
    : QObject(parent)
    , config_(AppConfig::defaults())
    , configFilePath_(QDir::cleanPath(configFilePath))
{
}

bool ConfigManager::load()
{
    if (!ensureDataDirectory())
        return false;

    QFile file(configFilePath_);
    if (!file.exists()) {
        config_ = AppConfig::defaults();
        return save();
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        config_ = AppConfig::defaults();
        return false;
    }

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        config_ = AppConfig::defaults();
        if (!backupInvalidConfig())
            return false;
        return save();
    }

    config_ = AppConfig::fromJson(doc.object());
    return true;
}

bool ConfigManager::save() const
{
    if (!ensureDataDirectory())
        return false;

    QSaveFile file(configFilePath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    const QJsonDocument doc(config_.toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    return file.commit();
}

const AppConfig &ConfigManager::config() const
{
    return config_;
}

void ConfigManager::setConfig(const AppConfig &config)
{
    config_ = config;
}

bool ConfigManager::ensureDataDirectory() const
{
    QDir dir(QFileInfo(configFilePath_).absolutePath());
    if (dir.exists())
        return true;
    return dir.mkpath(QStringLiteral("."));
}

bool ConfigManager::backupInvalidConfig() const
{
    const QString stamp = QDateTime::currentDateTimeUtc().toString(
        QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    QString backupPath = configFilePath_ + QStringLiteral(".invalid-") + stamp
                         + QStringLiteral(".bak");
    int suffix = 1;
    while (QFileInfo::exists(backupPath)) {
        backupPath = configFilePath_ + QStringLiteral(".invalid-") + stamp
                     + QStringLiteral("-%1.bak").arg(suffix++);
    }
    return QFile::copy(configFilePath_, backupPath);
}
