#include "emotionparser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace {
const QSet<QString> kSupportedEmotions = {QStringLiteral("happy"), QStringLiteral("shy"),
                                          QStringLiteral("neutral"),
                                          QStringLiteral("concerned"),
                                          QStringLiteral("excited")};
}

EmotionParser::Result EmotionParser::parse(const QString &rawText)
{
    static const QRegularExpression emotionRegex(
        QStringLiteral("\\[emotion:([A-Za-z]+)\\]"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression speechRegex(
        QStringLiteral("<tts-ja>\\s*([\\s\\S]*?)\\s*</tts-ja>"),
        QRegularExpression::CaseInsensitiveOption);

    Result result;
    result.emotion = QStringLiteral("neutral");

    QString jsonText = rawText.trimmed();
    if (jsonText.startsWith(QStringLiteral("```"))) {
        jsonText.remove(QRegularExpression(QStringLiteral("^```(?:json)?\\s*"),
                                           QRegularExpression::CaseInsensitiveOption));
        jsonText.remove(QRegularExpression(QStringLiteral("\\s*```$")));
    }
    const QJsonDocument document = QJsonDocument::fromJson(jsonText.toUtf8());
    if (document.isObject()) {
        const QJsonObject object = document.object();
        result.text = object.value(QStringLiteral("display_zh")).toString().trimmed();
        result.speechText = object.value(QStringLiteral("speech_ja")).toString().trimmed();
        const QString jsonEmotion =
            object.value(QStringLiteral("emotion")).toString().trimmed().toLower();
        if (kSupportedEmotions.contains(jsonEmotion))
            result.emotion = jsonEmotion;
        if (!result.text.isEmpty())
            return result;
    }

    const QRegularExpressionMatch speechMatch = speechRegex.match(rawText);
    if (speechMatch.hasMatch())
        result.speechText = speechMatch.captured(1).trimmed();

    QRegularExpressionMatchIterator it = emotionRegex.globalMatch(rawText);
    QRegularExpressionMatch lastMatch;
    while (it.hasNext())
        lastMatch = it.next();

    QString text = rawText;
    text.remove(speechRegex);
    if (lastMatch.hasMatch()) {
        const QString emotion = lastMatch.captured(1).toLower();
        if (kSupportedEmotions.contains(emotion))
            result.emotion = emotion;
        text.remove(lastMatch.capturedStart(0), lastMatch.capturedLength(0));
    }

    result.text = text.trimmed();
    return result;
}
