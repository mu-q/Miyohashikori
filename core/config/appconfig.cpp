#include "appconfig.h"

#include <QtGlobal>

namespace {
constexpr double kDefaultVolume = 0.8;
}

AppConfig AppConfig::defaults()
{
    AppConfig config;
    config.voiceEnabled = true;
    config.volume = kDefaultVolume;
    return config;
}

AppConfig AppConfig::fromJson(const QJsonObject &obj)
{
    AppConfig config = defaults();
    config.llmEndpoint = obj.value(QStringLiteral("llmEndpoint")).toString();
    config.llmApiKey = obj.value(QStringLiteral("llmApiKey")).toString();
    config.llmModel = obj.value(QStringLiteral("llmModel")).toString();
    config.ttsEndpoint = obj.value(QStringLiteral("ttsEndpoint")).toString();
    config.voiceEnabled = obj.value(QStringLiteral("voiceEnabled")).toBool(config.voiceEnabled);
    config.volume = qBound(0.0, obj.value(QStringLiteral("volume")).toDouble(config.volume), 1.0);

    const QJsonObject posObj = obj.value(QStringLiteral("windowPos")).toObject();
    if (posObj.contains(QStringLiteral("x")) && posObj.contains(QStringLiteral("y"))) {
        config.windowPos = QPoint(posObj.value(QStringLiteral("x")).toInt(),
                                  posObj.value(QStringLiteral("y")).toInt());
    }

    return config;
}

QJsonObject AppConfig::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("llmEndpoint"), llmEndpoint);
    obj.insert(QStringLiteral("llmApiKey"), llmApiKey);
    obj.insert(QStringLiteral("llmModel"), llmModel);
    obj.insert(QStringLiteral("ttsEndpoint"), ttsEndpoint);
    obj.insert(QStringLiteral("voiceEnabled"), voiceEnabled);
    obj.insert(QStringLiteral("volume"), qBound(0.0, volume, 1.0));

    QJsonObject posObj;
    posObj.insert(QStringLiteral("x"), windowPos.x());
    posObj.insert(QStringLiteral("y"), windowPos.y());
    obj.insert(QStringLiteral("windowPos"), posObj);

    return obj;
}
