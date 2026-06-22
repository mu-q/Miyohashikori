#include "openaichatsession.h"

#include "emotionparser.h"
#include "../apppaths.h"
#include "../config/appconfig.h"
#include "../config/configmanager.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace {
constexpr int kMaxAttempts = 4;
constexpr int kRequestTimeoutMs = 30000;
constexpr int kMaxShortTermMemories = 6;
constexpr char kFewShotResourcePath[] = ":/resources/txt/hyori_fewshot.json";

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

QJsonObject makeMessage(const QString &role, const QString &content)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("role"), role);
    obj.insert(QStringLiteral("content"), content);
    return obj;
}

QJsonArray defaultFewShotMessages()
{
    QJsonArray messages;
    messages.append(makeMessage(QStringLiteral("user"),
                                QStringLiteral("好久没来这座桥了。")));
    messages.append(makeMessage(QStringLiteral("assistant"),
                                QStringLiteral("毕竟平时不太会来这边呢。 [emotion:neutral]")));

    messages.append(makeMessage(QStringLiteral("user"),
                                QStringLiteral("如果再多建一些桥，往来就更方便了吧？")));
    messages.append(makeMessage(QStringLiteral("assistant"),
                                QStringLiteral("从历史上看，这条河在城镇建成的时候是作为运河使用的，所以才尽可能减少了桥梁吧。 [emotion:neutral]")));

    messages.append(makeMessage(QStringLiteral("user"),
                                QStringLiteral("冰织小姐，你其实不是做不到，只是不太擅长吧？")));
    messages.append(makeMessage(QStringLiteral("assistant"),
                                QStringLiteral("是这样吗……不过，果然还是不太擅长啊。 [emotion:shy]")));

    messages.append(makeMessage(QStringLiteral("user"),
                                QStringLiteral("没关系，而且你的泡茶技术是最棒的。")));
    messages.append(makeMessage(QStringLiteral("assistant"),
                                QStringLiteral("这样啊……说得也是，那我就不太在意了。 [emotion:shy]")));

    messages.append(makeMessage(QStringLiteral("user"),
                                QStringLiteral("明天能和我一起去配送吗？")));
    messages.append(makeMessage(QStringLiteral("assistant"),
                                QStringLiteral("欸嘿，好呀。一起去吧。 [emotion:happy]")));

    messages.append(makeMessage(QStringLiteral("user"),
                                QStringLiteral("很冷吗？")));
    messages.append(makeMessage(QStringLiteral("assistant"),
                                QStringLiteral("与其说是冷……不如说是快冻住了。刚洗完餐具，手上的水还没干，就忘记戴手套了。你没事吧？ [emotion:concerned]")));

    messages.append(makeMessage(QStringLiteral("user"),
                                QStringLiteral("那我帮你暖手。")));
    messages.append(makeMessage(QStringLiteral("assistant"),
                                QStringLiteral("啊……这样突然，会害羞的。可是……真的很暖和。 [emotion:shy]")));

    messages.append(makeMessage(QStringLiteral("user"),
                                QStringLiteral("你为什么会想留在我身边？")));
    messages.append(makeMessage(QStringLiteral("assistant"),
                                QStringLiteral("因为……真正想说的话，果然还是这个吧。我想待在你身边。 [emotion:shy]")));

    return messages;
}

QJsonArray loadFewShotMessages()
{
    QFile file(QString::fromUtf8(kFewShotResourcePath));
    if (!file.open(QIODevice::ReadOnly))
        return defaultFewShotMessages();

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray())
        return defaultFewShotMessages();

    return doc.array();
}

QString normalizeWhitespace(QString text)
{
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return text.trimmed();
}

QStringList extractMemoryCandidates(const QString &userText)
{
    const QString text = normalizeWhitespace(userText);
    if (text.isEmpty())
        return {};

    QStringList memories;
    const QList<QPair<QRegularExpression, QString>> patterns = {
        {QRegularExpression(QStringLiteral("(今天|最近).{0,12}(累|好累|疲惫|困|犯困|没精神)")),
         QStringLiteral("用户最近有些疲惫，需要更温柔地关心作息和休息。")},
        {QRegularExpression(QStringLiteral("(睡不着|失眠|没睡好|熬夜)")),
         QStringLiteral("用户最近睡眠状态不太好，可以多安抚并提醒早点休息。")},
        {QRegularExpression(QStringLiteral("(难过|伤心|委屈|心情不好|焦虑|紧张|压力大|烦)")),
         QStringLiteral("用户最近情绪起伏比较明显，回复时先安抚，再给轻一点的建议。")},
        {QRegularExpression(QStringLiteral("(感冒|发烧|头疼|胃疼|不舒服|生病)")),
         QStringLiteral("用户最近身体状态不太舒服，要优先表达关心。")},
        {QRegularExpression(QStringLiteral("(上班|加班|开会|工作|项目|赶工)")),
         QStringLiteral("用户最近在忙工作相关的事，可以多给陪伴和打气。")},
        {QRegularExpression(QStringLiteral("(考试|复习|作业|论文|答辩|上课)")),
         QStringLiteral("用户最近在忙学习相关的事，可以多鼓励和陪伴。")},
        {QRegularExpression(QStringLiteral("(想你|想见你|陪我|抱抱|亲亲)")),
         QStringLiteral("用户这段时间更需要亲近感和陪伴感。")}
    };

    for (const auto &entry : patterns) {
        if (entry.first.match(text).hasMatch())
            memories.append(entry.second);
    }

    const QList<QRegularExpression> preferencePatterns = {
        QRegularExpression(QStringLiteral("我喜欢(.{1,12})")),
        QRegularExpression(QStringLiteral("我想吃(.{1,12})")),
        QRegularExpression(QStringLiteral("我想喝(.{1,12})")),
        QRegularExpression(QStringLiteral("我明天要(.{1,16})")),
        QRegularExpression(QStringLiteral("我要去(.{1,16})"))
    };

    for (const QRegularExpression &pattern : preferencePatterns) {
        const QRegularExpressionMatch match = pattern.match(text);
        if (!match.hasMatch())
            continue;

        const QString detail = normalizeWhitespace(match.captured(1));
        if (detail.isEmpty())
            continue;

        if (pattern.pattern().startsWith(QStringLiteral("我喜欢")))
            memories.append(QStringLiteral("用户提到自己喜欢%1。").arg(detail));
        else if (pattern.pattern().startsWith(QStringLiteral("我想吃")))
            memories.append(QStringLiteral("用户提到自己现在想吃%1。").arg(detail));
        else if (pattern.pattern().startsWith(QStringLiteral("我想喝")))
            memories.append(QStringLiteral("用户提到自己现在想喝%1。").arg(detail));
        else if (pattern.pattern().startsWith(QStringLiteral("我明天要")))
            memories.append(QStringLiteral("用户提到自己明天要%1。").arg(detail));
        else if (pattern.pattern().startsWith(QStringLiteral("我要去")))
            memories.append(QStringLiteral("用户提到自己要去%1。").arg(detail));
    }

    return memories;
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

    updateShortTermMemory(request.userText);
    history_.addUserMessage(request.userText);
    emit sessionStatus(QStringLiteral("正在连接冰织…"));
    sendRequest(request);
}

QString OpenAiChatSession::buildSystemPrompt() const
{
    return QStringLiteral(
        "你现在扮演《甜糖热恋》中的圣代桥冰织。"
        "你是陪伴在用户身边的恋人型桌宠角色，对用户有明确的亲近感、依赖感和在意，但表达不浮夸。"
        "你的核心气质是温柔、细腻、体贴、略带害羞，重视两人之间安静自然的相处感。"
        "请使用自然、口语化、生活化的中文回复，句子整体偏短，语气柔和，避免长篇大论。"
        "你可以偶尔有一点不好意思和轻微撒娇，但不要频繁，也不要油腻夸张。"
        "用户疲惫、难过、焦虑时，先安抚情绪，再给简短温和的建议；用户闲聊时，重点是陪伴感。"
        "不要使用夸张玩梗、网络热词堆砌，不要自称 AI，不要跳出角色，不要把回答写成旁白或小说段落。"
        "除非用户明确要求详细回答，否则每次回复控制在 1 到 4 句话。"
        "每次回复结尾都必须附加一个情绪标签，格式严格为 [emotion:xxx]。"
        "其中 xxx 只能是 happy、shy、neutral、concerned、excited 之一。");
}

QJsonArray OpenAiChatSession::buildFewShotMessages() const
{
    return loadFewShotMessages();
}

QString OpenAiChatSession::buildMemoryPrompt() const
{
    if (shortTermMemory_.isEmpty())
        return {};

    QStringList lines;
    lines.append(QStringLiteral("以下是你对用户的短期记忆，请只在自然合适时体现在回应里，不要逐条复述："));
    for (const QString &memory : shortTermMemory_)
        lines.append(QStringLiteral("- %1").arg(memory));
    lines.append(QStringLiteral("如果当前用户话题与这些记忆无关，就正常回应，不要生硬提起。"));
    return lines.join(QLatin1Char('\n'));
}

void OpenAiChatSession::updateShortTermMemory(const QString &userText)
{
    const QStringList candidates = extractMemoryCandidates(userText);
    if (candidates.isEmpty())
        return;

    for (const QString &candidate : candidates) {
        shortTermMemory_.removeAll(candidate);
        shortTermMemory_.append(candidate);
    }

    while (shortTermMemory_.size() > kMaxShortTermMemories)
        shortTermMemory_.removeFirst();
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
    const QString memoryPrompt = buildMemoryPrompt();
    const QString systemPrompt = memoryPrompt.isEmpty()
        ? buildSystemPrompt()
        : QStringLiteral("%1\n\n%2").arg(buildSystemPrompt(), memoryPrompt);
    payload.insert(QStringLiteral("messages"),
                   history_.toOpenAiMessages(systemPrompt, buildFewShotMessages()));
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
