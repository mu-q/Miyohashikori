#pragma once

#include <QObject>
#include <QString>

#include "appconfig.h"

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigManager(QObject *parent = nullptr);
    explicit ConfigManager(const QString &configFilePath, QObject *parent = nullptr);

    bool load();
    bool save() const;

    const AppConfig &config() const;
    void setConfig(const AppConfig &config);

private:
    bool ensureDataDirectory() const;
    bool backupInvalidConfig() const;

    AppConfig config_;
    QString configFilePath_;
};
