#pragma once

#include <QObject>

#include "appconfig.h"

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigManager(QObject *parent = nullptr);

    bool load();
    bool save() const;

    const AppConfig &config() const;
    void setConfig(const AppConfig &config);

private:
    bool ensureDataDirectory() const;

    AppConfig config_;
};
