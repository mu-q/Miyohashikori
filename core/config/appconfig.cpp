#include "appconfig.h"

#include <QtGlobal>

namespace {
constexpr double kDefaultVolume = 0.8;
constexpr int kDefaultWorkMinutes = 25;
constexpr int kDefaultShortBreakMinutes = 5;
constexpr int kDefaultLongBreakMinutes = 15;

QString normalizeLlmEndpoint(const QString &endpoint)
{
    const QString trimmed = endpoint.trimmed();
    if (trimmed.isEmpty())
        return trimmed;

    // DeepSeek 使用 OpenAI 兼容的 /chat/completions，不是 /anthropic。
    if (trimmed.contains(QStringLiteral("api.deepseek.com"), Qt::CaseInsensitive)
        && trimmed.contains(QStringLiteral("anthropic"), Qt::CaseInsensitive)) {
        return QStringLiteral("https://api.deepseek.com/chat/completions");
    }

    return trimmed;
}
} // namespace

AppConfig AppConfig::defaults()
{
    AppConfig config;
    config.llmEndpoint = QStringLiteral("https://api.deepseek.com/chat/completions");
    config.llmModel = QStringLiteral("deepseek-chat");
    config.voiceEnabled = true;
    config.volume = kDefaultVolume;
    config.pomodoroWorkMinutes = kDefaultWorkMinutes;
    config.pomodoroShortBreakMinutes = kDefaultShortBreakMinutes;
    config.pomodoroLongBreakMinutes = kDefaultLongBreakMinutes;
    config.pomodoroPhase = QStringLiteral("work");
    config.pomodoroRemainingSeconds = kDefaultWorkMinutes * 60;
    return config;
}

AppConfig AppConfig::fromJson(const QJsonObject &obj)
{
    AppConfig config = defaults();

    const QString endpoint =
        normalizeLlmEndpoint(obj.value(QStringLiteral("llmEndpoint")).toString());
    if (!endpoint.isEmpty())
        config.llmEndpoint = endpoint;

    config.llmApiKey = obj.value(QStringLiteral("llmApiKey")).toString().trimmed();

    const QString model = obj.value(QStringLiteral("llmModel")).toString().trimmed();
    if (!model.isEmpty())
        config.llmModel = model;
    config.ttsEndpoint = obj.value(QStringLiteral("ttsEndpoint")).toString();
    config.voiceEnabled = obj.value(QStringLiteral("voiceEnabled")).toBool(config.voiceEnabled);
    config.volume = qBound(0.0, obj.value(QStringLiteral("volume")).toDouble(config.volume), 1.0);
    config.backgroundVideoPath = obj.value(QStringLiteral("backgroundVideoPath")).toString().trimmed();
    config.pomodoroWorkMinutes = qBound(1, obj.value(QStringLiteral("pomodoroWorkMinutes")).toInt(config.pomodoroWorkMinutes), 180);
    config.pomodoroShortBreakMinutes = qBound(1, obj.value(QStringLiteral("pomodoroShortBreakMinutes")).toInt(config.pomodoroShortBreakMinutes), 60);
    config.pomodoroLongBreakMinutes = qBound(1, obj.value(QStringLiteral("pomodoroLongBreakMinutes")).toInt(config.pomodoroLongBreakMinutes), 120);
    const QString phase = obj.value(QStringLiteral("pomodoroPhase")).toString().trimmed();
    if (phase == QStringLiteral("work") || phase == QStringLiteral("shortBreak") || phase == QStringLiteral("longBreak"))
        config.pomodoroPhase = phase;
    config.pomodoroRemainingSeconds = qMax(0, obj.value(QStringLiteral("pomodoroRemainingSeconds")).toInt(config.pomodoroRemainingSeconds));
    config.pomodoroCompletedCycles = qMax(0, obj.value(QStringLiteral("pomodoroCompletedCycles")).toInt(config.pomodoroCompletedCycles));
    config.pomodoroCompletedDate = obj.value(QStringLiteral("pomodoroCompletedDate")).toString();

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
    obj.insert(QStringLiteral("backgroundVideoPath"), backgroundVideoPath);
    obj.insert(QStringLiteral("pomodoroWorkMinutes"), qBound(1, pomodoroWorkMinutes, 180));
    obj.insert(QStringLiteral("pomodoroShortBreakMinutes"), qBound(1, pomodoroShortBreakMinutes, 60));
    obj.insert(QStringLiteral("pomodoroLongBreakMinutes"), qBound(1, pomodoroLongBreakMinutes, 120));
    obj.insert(QStringLiteral("pomodoroPhase"), pomodoroPhase);
    obj.insert(QStringLiteral("pomodoroRemainingSeconds"), qMax(0, pomodoroRemainingSeconds));
    obj.insert(QStringLiteral("pomodoroCompletedCycles"), qMax(0, pomodoroCompletedCycles));
    obj.insert(QStringLiteral("pomodoroCompletedDate"), pomodoroCompletedDate);

    QJsonObject posObj;
    posObj.insert(QStringLiteral("x"), windowPos.x());
    posObj.insert(QStringLiteral("y"), windowPos.y());
    obj.insert(QStringLiteral("windowPos"), posObj);

    return obj;
}
