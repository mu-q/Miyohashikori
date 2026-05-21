#pragma once

#include <QJsonObject>
#include <QPoint>
#include <QString>

struct AppConfig
{
    QString llmEndpoint;
    QString llmApiKey;
    QString llmModel;
    QString ttsEndpoint;
    QPoint windowPos;
    bool voiceEnabled = true;
    double volume = 0.8;

    static AppConfig defaults();
    static AppConfig fromJson(const QJsonObject &obj);
    QJsonObject toJson() const;
};
