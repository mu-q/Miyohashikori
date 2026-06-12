#include "openaichatsession.h"

#include "emotionparser.h"
#include "../apppaths.h"
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
constexpr int kRequestTimeoutMs = 30000;

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

QString describeNetworkError(QNetworkReply::NetworkError code, const QString &errorString)
{
    switch (code) {
    case QNetworkReply::TimeoutError:
        return QStringLiteral("网络连接超时，请检查网络或代理设置。");
    case QNetworkReply::ConnectionRefusedError:
        return QStringLiteral("无法连接到服务器，请检查 llmEndpoint 地址是否正确。");
    case QNetworkReply::HostNotFoundError:
        return QStringLiteral("找不到服务器主机，请检查 llmEndpoint 地址。");
    case QNetworkReply::SslHandshakeFailedError:
        return QStringLiteral("SSL 握手失败，请检查系统时间或网络代理。");
    case QNetworkReply::AuthenticationRequiredError:
        return QStringLiteral("API 认证失败，请检查 llmApiKey 是否正确。");
    case QNetworkReply::ContentAccessDenied:
    case QNetworkReply::ContentOperationNotPermittedError:
        return QStringLiteral("API 访问被拒绝，请检查 llmApiKey 权限或余额。");
    default:
        break;
    }
    return errorString;
}

QString validateLlmConfig(const AppConfig &config)
{
    if (config.llmApiKey.trimmed().isEmpty()) {
        return QStringLiteral("未配置 llmApiKey，请在 %1 中填写 API Key 后重试。")
            .arg(AppPaths::configFilePath());
    }
    if (config.llmEndpoint.trimmed().isEmpty()) {
        return QStringLiteral("未配置 llmEndpoint，请在 %1 中填写接口地址。")
            .arg(AppPaths::configFilePath());
    }
    if (config.llmModel.trimmed().isEmpty()) {
        return QStringLiteral("未配置 llmModel，请在 %1 中填写模型名称。")
            .arg(AppPaths::configFilePath());
    }
    return {};
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
    configManager_->load();

    const QString configError = validateLlmConfig(configManager_->config());
    if (!configError.isEmpty()) {
        emit sessionError(configError);
        return;
    }

    PendingRequest request;
    request.userText = userText.trimmed();
    if (request.userText.isEmpty()) {
        emit sessionError(QStringLiteral("发送内容不能为空。"));
        return;
    }

    history_.addUserMessage(request.userText);
    emit sessionStatus(QStringLiteral("正在连接冰织…"));
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
    networkRequest.setTransferTimeout(kRequestTimeoutMs);

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
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        const QString reason =
            describeNetworkError(reply->error(), reply->errorString());
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
    if (statusCode >= 400) {
        const QString apiError = errorTextFromJson(root);
        reply->deleteLater();
        retryRequest(request, apiError.isEmpty()
                                   ? QStringLiteral("HTTP 错误 %1。").arg(statusCode)
                                   : apiError);
        return;
    }

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
        emit sessionError(QStringLiteral("AI 请求失败（已重试 %1 次）：%2")
                              .arg(kMaxAttempts - 1)
                              .arg(reason));
        return;
    }

    const int delayMs = (1 << (next.attempt - 1)) * 1000;
    emit sessionStatus(QStringLiteral("请求失败，%1 秒后重试（%2/%3）…")
                           .arg(delayMs / 1000)
                           .arg(next.attempt)
                           .arg(kMaxAttempts - 1));
    QTimer::singleShot(delayMs, this, [this, next] { sendRequest(next); });
}
