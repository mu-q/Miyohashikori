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
    QString ttsReferenceAudioPath;
    QString ttsReferenceText;
    QString ttsReferenceLanguage = QStringLiteral("ja");
    QString ttsTextLanguage = QStringLiteral("ja");
    bool ttsEnabled = false;
    double ttsSpeedFactor = 1.0;
    QPoint windowPos;
    bool voiceEnabled = true;
    double volume = 0.8;
    QString backgroundVideoPath;
    int pomodoroWorkMinutes = 25;
    int pomodoroShortBreakMinutes = 5;
    int pomodoroLongBreakMinutes = 15;
    QString pomodoroPhase = QStringLiteral("work");
    int pomodoroRemainingSeconds = 25 * 60;
    int pomodoroCompletedCycles = 0;
    QString pomodoroCompletedDate;

    static AppConfig defaults();
    static AppConfig fromJson(const QJsonObject &obj);
    QJsonObject toJson() const;
};
