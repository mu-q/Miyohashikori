#include "openaichatsession.h"

#include "emotionparser.h"
#include "../config/appconfig.h"
#include "../config/configmanager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace {
constexpr int kMaxAttempts = 4;

QString extractContent(const QJsonObject &messageObj)
{
    const QJsonValue contentValue = messageObj.value(QStringLiteral("content"));
    if (contentValue.isString())
        return contentValue.toString();

    if (!contentValue.isArray())
        return {};

    QStringList parts;
    const QJsonArray array = contentValue.toArray();
    for (const QJsonValue &value : array) {
        const QJsonObject obj = value.toObject();
        if (obj.value(QStringLiteral("type")).toString() == QStringLiteral("text"))
            parts.append(obj.value(QStringLiteral("text")).toString());
    }

    return parts.join(QString());
}

QString errorTextFromJson(const QJsonObject &root)
{
    const QJsonObject errorObj = root.value(QStringLiteral("error")).toObject();
    return errorObj.value(QStringLiteral("message")).toString();
}
} // namespace

OpenAiChatSession::OpenAiChatSession(ConfigManager *configManager, QObject *parent)
    : IAiSession(parent)
    , network_(new QNetworkAccessManager(this))
    , configManager_(configManager)
{
}

void OpenAiChatSession::submit(const QString &userText)
{
    const AppConfig &config = configManager_->config();
    if (config.llmEndpoint.trimmed().isEmpty() || config.llmApiKey.trimmed().isEmpty()
        || config.llmModel.trimmed().isEmpty()) {
        emit sessionError(QStringLiteral("请先在配置文件中填写 LLM endpoint、API key 和 model。"));
        return;
    }

    PendingRequest request;
    request.userText = userText.trimmed();
    if (request.userText.isEmpty()) {
        emit sessionError(QStringLiteral("发送内容不能为空。"));
        return;
    }

    history_.addUserMessage(request.userText);
    sendRequest(request);
}

QString OpenAiChatSession::buildSystemPrompt() const
{
    return QStringLiteral(
        "你是《甜糖热恋》中的圣代桥冰织。请用温柔体贴、热恋中的女友语气回复，"
        "偶尔带一点害羞，但不要夸张。回复要自然、简洁、有陪伴感。"
        "每次回复结尾都必须附加一个情绪标签，格式严格为 [emotion:xxx]。"
        "其中 xxx 只能是 happy、shy、neutral、concerned、excited 之一。");
}

void OpenAiChatSession::sendRequest(const PendingRequest &request)
{
    const AppConfig &config = configManager_->config();

    QNetworkRequest networkRequest{QUrl(config.llmEndpoint)};
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                             QStringLiteral("application/json"));
    networkRequest.setRawHeader("Authorization",
                                QByteArray("Bearer ") + config.llmApiKey.toUtf8());

    QJsonObject payload;
    payload.insert(QStringLiteral("model"), config.llmModel);
    payload.insert(QStringLiteral("messages"), history_.toOpenAiMessages(buildSystemPrompt()));
    payload.insert(QStringLiteral("temperature"), 0.9);

    QNetworkReply *reply =
        network_->post(networkRequest, QJsonDocument(payload).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, request] { handleReply(reply, request); });
}

void OpenAiChatSession::handleReply(QNetworkReply *reply, PendingRequest request)
{
    const QByteArray body = reply->readAll();

    if (reply->error() != QNetworkReply::NoError) {
        const QString reason = reply->errorString();
        reply->deleteLater();
        retryRequest(request, reason);
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        reply->deleteLater();
        retryRequest(request, QStringLiteral("响应不是有效的 JSON。"));
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        const QString apiError = errorTextFromJson(root);
        reply->deleteLater();
        retryRequest(request, apiError.isEmpty() ? QStringLiteral("接口没有返回 choices。")
                                                 : apiError);
        return;
    }

    const QJsonObject messageObj =
        choices.first().toObject().value(QStringLiteral("message")).toObject();
    const QString rawText = extractContent(messageObj);
    if (rawText.trimmed().isEmpty()) {
        reply->deleteLater();
        retryRequest(request, QStringLiteral("接口返回了空回复。"));
        return;
    }

    const EmotionParser::Result result = EmotionParser::parse(rawText);
    history_.addAssistantMessage(rawText.trimmed());

    emit assistantMessage(result.text.isEmpty() ? rawText.trimmed() : result.text);
    emit assistantEmotion(result.emotion);

    reply->deleteLater();
}

void OpenAiChatSession::retryRequest(const PendingRequest &request, const QString &reason)
{
    PendingRequest next = request;
    ++next.attempt;

    if (next.attempt >= kMaxAttempts) {
        emit sessionError(QStringLiteral("AI 请求失败：%1").arg(reason));
        return;
    }

    const int delayMs = (1 << (next.attempt - 1)) * 1000;
    QTimer::singleShot(delayMs, this, [this, next] { sendRequest(next); });
}
