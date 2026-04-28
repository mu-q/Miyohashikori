#include "nullaisession.h"

#include <QTimer>

NullAiSession::NullAiSession(QObject *parent)
    : IAiSession(parent)
{
}

void NullAiSession::submit(const QString &userText)
{
    Q_UNUSED(userText);
    QTimer::singleShot(200, this, [this, userText] {
        emit assistantMessage(
            QStringLiteral("（原型占位）已收到：%1\n后端未连接，后续在此接入 LLM / TTS。").arg(userText));
        emit assistantEmotion(QStringLiteral("默认"));
    });
}
