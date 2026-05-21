#include "configmanager.h"

#include "../apppaths.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , config_(AppConfig::defaults())
{
}

bool ConfigManager::load()
{
    if (!ensureDataDirectory())
        return false;

    QFile file(AppPaths::configFilePath());
    if (!file.exists()) {
        config_ = AppConfig::defaults();
        return save();
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        config_ = AppConfig::defaults();
        return save();
    }

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        config_ = AppConfig::defaults();
        return save();
    }

    config_ = AppConfig::fromJson(doc.object());
    return true;
}

bool ConfigManager::save() const
{
    if (!ensureDataDirectory())
        return false;

    QSaveFile file(AppPaths::configFilePath());
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
    QDir dir(AppPaths::appDataRoot());
    if (dir.exists())
        return true;
    return dir.mkpath(QStringLiteral("."));
}
