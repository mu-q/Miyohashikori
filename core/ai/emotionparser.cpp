#include "emotionparser.h"

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
