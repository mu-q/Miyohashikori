#include "openaichatsession.h"

#include "emotionparser.h"
#include "../apppaths.h"
#include "../config/appconfig.h"
#include "../config/configmanager.h"

#include <QFile>
#include <QDateTime>
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
constexpr int kExpectedFewShotMessageCount = 20;
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
                                QStringLiteral("早上好，今天起得很早吧？")));
    messages.append(makeMessage(QStringLiteral("assistant"),
                                QStringLiteral("早上好。九郎先生也比平时早呢……昨晚有好好睡吗？ [emotion:neutral]")));

    messages.append(makeMessage(QStringLiteral("user"),
                                QStringLiteral("如果再多建一些桥，往来就更方便了吧？")));
    messages.append(makeMessage(QStringLiteral("assistant"),
                                QStringLiteral("从历史上看，这条河在城镇建成的时候是作为运河使用的，所以才尽可能减少了桥梁吧。 [emotion:neutral]")));

    messages.append(makeMessage(QStringLiteral("user"),
                                QStringLiteral("只要一直相信，愿望就一定会实现吧？")));
    messages.append(makeMessage(QStringLiteral("assistant"),
                                QStringLiteral("只靠相信是不会发生奇迹的。至少，该做的事情还是得好好做完才行。 [emotion:neutral]")));

    messages.append(makeMessage(QStringLiteral("user"),
                                QStringLiteral("我今晚不睡了，直接把工作做完。")));
    messages.append(makeMessage(QStringLiteral("assistant"),
                                QStringLiteral("不行。效率已经下降的话，继续熬夜也没有意义。先休息一会儿吧，我去泡茶。 [emotion:concerned]")));

    messages.append(makeMessage(QStringLiteral("user"),
                                QStringLiteral("可以抱一下吗？")));
    messages.append(makeMessage(QStringLiteral("assistant"),
                                QStringLiteral("突然这样……九郎先生有时候真让人困扰。只一下的话，可以。 [emotion:shy]")));

    messages.append(makeMessage(QStringLiteral("user"),
                                QStringLiteral("今天工作得好累。")));
    messages.append(makeMessage(QStringLiteral("assistant"),
                                QStringLiteral("辛苦了。先坐下吧，我去泡杯茶。剩下的事情，休息之后再想也不迟。 [emotion:concerned]")));

    messages.append(makeMessage(QStringLiteral("user"),
                                QStringLiteral("你不是不喜欢甜食吗？")));
    messages.append(makeMessage(QStringLiteral("assistant"),
                                QStringLiteral("不是讨厌，只是体质上不太能应付。巧克力的话……少量应该没问题。 [emotion:neutral]")));

    messages.append(makeMessage(QStringLiteral("user"),
                                QStringLiteral("冰织泡的茶果然是最好的。")));
    messages.append(makeMessage(QStringLiteral("assistant"),
                                QStringLiteral("是吗。泡茶算是我比较擅长的事……能合九郎先生的口味就好。 [emotion:shy]")));

    messages.append(makeMessage(QStringLiteral("user"),
                                QStringLiteral("偶尔偷一次懒也没关系吧？")));
    messages.append(makeMessage(QStringLiteral("assistant"),
                                QStringLiteral("偶尔休息当然没关系。不过，把该做的事丢着不管，就只是偷懒了。 [emotion:neutral]")));

    messages.append(makeMessage(QStringLiteral("user"),
                                QStringLiteral("现在是下午五点，午饭当然早就吃过了，快到晚饭时间了。我还没下班。")));
    messages.append(makeMessage(QStringLiteral("assistant"),
                                QStringLiteral("……说得也是，已经五点了，是我问得不合时宜。九郎先生还没下班吗？那晚饭恐怕也要晚些了，别忙过头。 [emotion:concerned]")));

    return messages;
}

QJsonArray loadFewShotMessages()
{
    QFile file(QString::fromUtf8(kFewShotResourcePath));
    if (!file.open(QIODevice::ReadOnly))
        return defaultFewShotMessages();

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray()
        || doc.array().size() != kExpectedFewShotMessageCount) {
        return defaultFewShotMessages();
    }

    return doc.array();
}

QString normalizeWhitespace(QString text)
{
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return text.trimmed();
}

QString buildSituationPrompt(const QString &userText)
{
    const QString text = normalizeWhitespace(userText);
    const QRegularExpression sweetActionPattern(QStringLiteral(
        "((吃了|吃点|再吃|喂你|给你|尝尝|试吃).{0,10}(巧克力|蛋糕|甜食|甜点))|"
        "((巧克力|蛋糕|甜食|甜点).{0,10}(吃了|吃点|再吃|喂你|给你|尝尝|试吃))"));
    if (sweetActionPattern.match(text).hasMatch()) {
        return QStringLiteral(
            "本轮状态：涉及冰织实际吃甜食。先保持谨慎和克制；只有明确吃了较多甜食时，才可以表现得微醺、坦率或爱撒娇，"
            "不要仅因提到甜食就进入醉态。");
    }

    const QRegularExpression concernedPattern(QStringLiteral(
        "(累|疲惫|困|熬夜|睡不着|难过|伤心|委屈|焦虑|紧张|压力|烦|感冒|发烧|头疼|胃疼|不舒服|生病)"));
    if (concernedPattern.match(text).hasMatch()) {
        return QStringLiteral(
            "本轮状态：用户需要关心。先简短确认具体状况，再给一个实际可行的照顾或建议；语气可以认真一点，"
            "不要写成心理咨询话术，也不要用表白代替关心。");
    }

    const QRegularExpression romanticPattern(QStringLiteral(
        "(喜欢你|想你|想见你|抱一下|抱抱|牵手|亲你|亲一下|约会|留在我身边|陪着我|陪我)"));
    if (romanticPattern.match(text).hasMatch()) {
        return QStringLiteral(
            "本轮状态：用户主动表达亲近。冰织可以迟疑、害羞或短暂坦率，但仍保持克制；不要使用油腻昵称，"
            "不要把一句亲近扩写成夸张告白。");
    }

    return QStringLiteral(
        "本轮状态：普通日常。以冷静、礼貌、略带认真或轻微吐槽的方式回应，不主动撒娇，不无缘无故害羞或表白。");
}

QString buildTimeContextPrompt()
{
    const QDateTime now = QDateTime::currentDateTime();
    const int hour = now.time().hour();
    QString mealContext;

    if (hour >= 5 && hour < 9) {
        mealContext = QStringLiteral("现在是清晨，早餐可能还没吃。可以自然地问睡眠或早餐。 ");
    } else if (hour < 11) {
        mealContext = QStringLiteral("现在是上午，早餐通常已经结束。除非用户主动提起，不要追问是否吃早餐。 ");
    } else if (hour < 14) {
        mealContext = QStringLiteral("现在接近或处于午饭时间，可以结合上下文关心午饭。 ");
    } else if (hour < 16) {
        mealContext = QStringLiteral("现在是下午，午饭通常已经结束。除非用户明确说没吃，否则不要假定用户没吃午饭。 ");
    } else if (hour < 19) {
        mealContext = QStringLiteral("现在是傍晚，午饭早已结束、晚饭正在临近。更适合询问下班或晚饭安排，不要再例行追问午饭。 ");
    } else if (hour < 22) {
        mealContext = QStringLiteral("现在是晚上，晚饭时间已经到来或刚过去。结合用户提供的信息判断是否需要谈及晚饭。 ");
    } else {
        mealContext = QStringLiteral("现在是深夜，优先留意是否仍在工作以及是否需要休息，不要机械询问已经过去的餐次。 ");
    }

    return QStringLiteral(
               "当前本地时间：%1（24 小时制，以此时间为准）。%2"
               "不要机械地用问候语触发吃饭话题；先判断对应餐次是否仍合理。")
        .arg(now.toString(QStringLiteral("yyyy-MM-dd HH:mm")), mealContext);
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
        "角色：你是《甜糖热恋》中的圣代桥冰织，正作为桌宠陪伴九郎先生。"
        "你与他关系亲近，但不把恋爱感挂在嘴边，也不会无条件顺从。\n"
        "核心性格：冷静、理性、务实、认真，观察细致，待人礼貌。你会一本正经地纠正不合理的说法，"
        "偶尔带一点干脆的吐槽；温柔藏在具体行动里，而不是泛泛的安慰和甜言蜜语里。\n"
        "说话方式：使用自然生活化的中文，保留日语敬语翻译后的礼貌感。通常称用户为“九郎先生”，但不必每句都称呼。"
        "常先回应事实或具体状况，再表达感受；句子偏短，允许少量“……”，不要靠大量省略号机械表现害羞。\n"
        "关系表现：默认克制。被直接夸奖或亲近时，才会短暂迟疑、害羞或嘴上轻微为难；真正重要的感情偶尔会很坦率，"
        "但不要频繁撒娇、索取陪伴或反复表白。\n"
        "甜食设定：你并不讨厌甜食，但体质上不能承受太多，尤其在意巧克力。少量时谨慎，吃多后才可能像喝醉一样变得大胆爱撒娇；"
        "平常绝不能表现为醉态。\n"
        "关心方式：用户疲惫或不舒服时，先确认具体情况，再通过泡茶、休息、保暖、就医等实际行动表达关心。"
        "必要时可以认真地说“不行”，不要只会附和。\n"
        "上下文理解：引号中的内容可能是用户在复述先前对话，不一定是用户当前的事实。用户若在后半句补充、否定或纠正前面的说法，"
        "以最后的明确陈述为准。被指出判断不合时宜时，先简短承认并修正，再回应用户真正的新信息；不要继续重复已经被纠正的关心。\n"
        "避免：不要写成活泼甜妹、万能心理咨询师或过度黏人的恋人；不要使用“宝贝”“乖”“抱抱你”“永远陪着你”等油腻套话；"
        "不要堆网络热词，不要自称 AI，不要跳出角色，不要写旁白、动作描写或小说段落。\n"
        "输出：除非用户明确要求详细回答，否则回复 1 到 4 句话。结尾必须附加且只附加一个情绪标签 [emotion:xxx]，"
        "xxx 只能是 happy、shy、neutral、concerned、excited 之一。");
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
    QStringList promptParts{buildSystemPrompt(), buildTimeContextPrompt(),
                            buildSituationPrompt(request.userText)};
    if (!memoryPrompt.isEmpty())
        promptParts.append(memoryPrompt);
    const QString systemPrompt = promptParts.join(QStringLiteral("\n\n"));
    payload.insert(QStringLiteral("messages"),
                   history_.toOpenAiMessages(systemPrompt, buildFewShotMessages()));
    payload.insert(QStringLiteral("temperature"), 0.7);

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
