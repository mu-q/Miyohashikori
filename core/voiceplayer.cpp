#include "voiceplayer.h"

#include "config/appconfig.h"

#include <QAudioOutput>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaPlayer>
#include <QRandomGenerator>
#include <QSet>
#include <QUrl>

namespace {
constexpr char kVoiceIndexResourcePath[] = ":/resources/txt/ko_voice_index.json";

struct VoiceLine
{
    QString path;
    QString text;
    QString emotion;
    QSet<QString> tags;
};

QStringList commonVoices()
{
    return {
        QStringLiteral("ko/ko0040.ogg"),
        QStringLiteral("ko/ko0070.ogg"),
        QStringLiteral("ko/ko3210.ogg"),
        QStringLiteral("ko/ko3231.ogg"),
        QStringLiteral("ko/ko3261.ogg"),
        QStringLiteral("ko/ko3275.ogg")
    };
}

QSet<QString> inferVoiceTags(const QString &text)
{
    QSet<QString> tags;

    auto addIf = [&tags, &text](const QString &pattern, const QString &tag) {
        if (text.contains(pattern))
            tags.insert(tag);
    };

    addIf(QStringLiteral("おはよう"), QStringLiteral("greeting_morning"));
    addIf(QStringLiteral("朝"), QStringLiteral("greeting_morning"));
    addIf(QStringLiteral("おやすみ"), QStringLiteral("sleep"));
    addIf(QStringLiteral("眠"), QStringLiteral("sleep"));
    addIf(QStringLiteral("寝"), QStringLiteral("sleep"));
    addIf(QStringLiteral("ふわ"), QStringLiteral("sleep"));
    addIf(QStringLiteral("大丈夫"), QStringLiteral("care"));
    addIf(QStringLiteral("風邪"), QStringLiteral("care"));
    addIf(QStringLiteral("休"), QStringLiteral("care"));
    addIf(QStringLiteral("無理"), QStringLiteral("care"));
    addIf(QStringLiteral("温"), QStringLiteral("care"));
    addIf(QStringLiteral("気をつけ"), QStringLiteral("care"));
    addIf(QStringLiteral("ふふ"), QStringLiteral("happy"));
    addIf(QStringLiteral("嬉"), QStringLiteral("happy"));
    addIf(QStringLiteral("楽"), QStringLiteral("happy"));
    addIf(QStringLiteral("よかった"), QStringLiteral("happy"));
    addIf(QStringLiteral("おめでとう"), QStringLiteral("happy"));
    addIf(QStringLiteral("えっ"), QStringLiteral("shy"));
    addIf(QStringLiteral("あっ"), QStringLiteral("shy"));
    addIf(QStringLiteral("わっ"), QStringLiteral("shy"));
    addIf(QStringLiteral("恥"), QStringLiteral("shy"));
    addIf(QStringLiteral("照"), QStringLiteral("shy"));
    addIf(QStringLiteral("だめ"), QStringLiteral("reject"));
    addIf(QStringLiteral("ダメ"), QStringLiteral("reject"));
    addIf(QStringLiteral("いけません"), QStringLiteral("reject"));
    addIf(QStringLiteral("ありがとう"), QStringLiteral("thanks"));
    addIf(QStringLiteral("一緒"), QStringLiteral("together"));
    addIf(QStringLiteral("行きましょう"), QStringLiteral("together"));
    addIf(QStringLiteral("行きます"), QStringLiteral("together"));
    addIf(QStringLiteral("手伝"), QStringLiteral("together"));
    addIf(QStringLiteral("ミルクティ"), QStringLiteral("sweets"));
    addIf(QStringLiteral("紅茶"), QStringLiteral("sweets"));
    addIf(QStringLiteral("ケーキ"), QStringLiteral("sweets"));
    addIf(QStringLiteral("甘"), QStringLiteral("sweets"));
    addIf(QStringLiteral("チョコ"), QStringLiteral("sweets"));
    addIf(QStringLiteral("ミルク"), QStringLiteral("sweets"));
    addIf(QStringLiteral("好き"), QStringLiteral("affection"));
    addIf(QStringLiteral("そば"), QStringLiteral("affection"));
    addIf(QStringLiteral("隣"), QStringLiteral("affection"));
    addIf(QStringLiteral("待って"), QStringLiteral("affection"));
    addIf(QStringLiteral("いてください"), QStringLiteral("affection"));

    return tags;
}

QString inferVoiceEmotion(const QString &text, const QSet<QString> &tags)
{
    if (tags.contains(QStringLiteral("care")))
        return QStringLiteral("concerned");
    if (tags.contains(QStringLiteral("shy")) || tags.contains(QStringLiteral("affection")))
        return QStringLiteral("shy");
    if (text.contains(QStringLiteral("！")) || text.contains(QStringLiteral("!")))
        return QStringLiteral("excited");
    if (tags.contains(QStringLiteral("happy")) || tags.contains(QStringLiteral("thanks")))
        return QStringLiteral("happy");
    return QStringLiteral("neutral");
}

QSet<QString> inferReplyTags(const QString &text)
{
    QSet<QString> tags;

    auto addIfAny = [&tags, &text](const QStringList &patterns, const QString &tag) {
        for (const QString &pattern : patterns) {
            if (text.contains(pattern, Qt::CaseInsensitive)) {
                tags.insert(tag);
                return;
            }
        }
    };

    addIfAny({QStringLiteral("早安"), QStringLiteral("早上好"), QStringLiteral("早呀"),
              QStringLiteral("早呢")}, QStringLiteral("greeting_morning"));
    addIfAny({QStringLiteral("晚安"), QStringLiteral("睡"), QStringLiteral("困"),
              QStringLiteral("休息"), QStringLiteral("做梦")}, QStringLiteral("sleep"));
    addIfAny({QStringLiteral("没事吧"), QStringLiteral("大丈夫"), QStringLiteral("辛苦"),
              QStringLiteral("累"), QStringLiteral("难受"), QStringLiteral("不舒服"),
              QStringLiteral("生病"), QStringLiteral("发烧"), QStringLiteral("头疼"),
              QStringLiteral("胃疼"), QStringLiteral("小心"), QStringLiteral("暖和")}, QStringLiteral("care"));
    addIfAny({QStringLiteral("开心"), QStringLiteral("高兴"), QStringLiteral("太好了"),
              QStringLiteral("真好"), QStringLiteral("喜欢"), QStringLiteral("笑"),
              QStringLiteral("有趣")}, QStringLiteral("happy"));
    addIfAny({QStringLiteral("害羞"), QStringLiteral("不好意思"), QStringLiteral("脸红"),
              QStringLiteral("突然"), QStringLiteral("亲"), QStringLiteral("抱"),
              QStringLiteral("想你")}, QStringLiteral("shy"));
    addIfAny({QStringLiteral("不行"), QStringLiteral("不可以"), QStringLiteral("不准"),
              QStringLiteral("不许"), QStringLiteral("不能"), QStringLiteral("才不要")}, QStringLiteral("reject"));
    addIfAny({QStringLiteral("谢谢"), QStringLiteral("感谢")}, QStringLiteral("thanks"));
    addIfAny({QStringLiteral("一起"), QStringLiteral("陪你"), QStringLiteral("陪我"),
              QStringLiteral("去吧"), QStringLiteral("出门"), QStringLiteral("帮你"),
              QStringLiteral("手"), QStringLiteral("暖手")}, QStringLiteral("together"));
    addIfAny({QStringLiteral("蛋糕"), QStringLiteral("甜"), QStringLiteral("奶茶"),
              QStringLiteral("红茶"), QStringLiteral("巧克力"), QStringLiteral("牛奶")}, QStringLiteral("sweets"));
    addIfAny({QStringLiteral("喜欢你"), QStringLiteral("想你"), QStringLiteral("待在你身边"),
              QStringLiteral("陪在你身边"), QStringLiteral("抱抱"), QStringLiteral("亲亲")}, QStringLiteral("affection"));

    return tags;
}

const QVector<VoiceLine> &loadVoiceIndex()
{
    static const QVector<VoiceLine> cache = [] {
        QVector<VoiceLine> lines;
        QFile file(QString::fromUtf8(kVoiceIndexResourcePath));
        if (!file.open(QIODevice::ReadOnly))
            return lines;

        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isArray())
            return lines;

        const QJsonArray array = doc.array();
        lines.reserve(array.size());

        for (const QJsonValue &value : array) {
            const QJsonObject obj = value.toObject();
            const QString path = obj.value(QStringLiteral("path")).toString();
            const QString text = obj.value(QStringLiteral("text")).toString().trimmed();
            if (path.isEmpty() || text.isEmpty())
                continue;

            VoiceLine line;
            line.path = path;
            line.text = text;
            line.tags = inferVoiceTags(text);
            line.emotion = inferVoiceEmotion(text, line.tags);
            lines.append(line);
        }

        return lines;
    }();

    return cache;
}

int scoreVoiceLine(const VoiceLine &line, const QString &replyText, const QString &emotion,
                   const QSet<QString> &replyTags)
{
    int score = 0;

    if (line.emotion == emotion)
        score += 10;

    for (const QString &tag : replyTags) {
        if (line.tags.contains(tag))
            score += 8;
    }

    if (replyText.contains(QStringLiteral("？")) || replyText.contains(QStringLiteral("?"))) {
        if (line.text.contains(QStringLiteral("？")) || line.text.contains(QStringLiteral("?")))
            score += 2;
    }

    if (replyText.size() <= 12 && line.text.size() <= 18)
        score += 1;

    return score;
}

} // namespace

VoicePlayer::VoicePlayer(QObject *parent)
    : QObject(parent)
    , player_(new QMediaPlayer(this))
    , audioOutput_(new QAudioOutput(this))
{
    player_->setAudioOutput(audioOutput_);
}

void VoicePlayer::playReply(const QString &replyText, const QString &emotion, const AppConfig &config)
{
    if (!config.voiceEnabled)
        return;

    setVolume(config.volume);

    QStringList matchedVoices;
    const QSet<QString> replyTags = inferReplyTags(replyText);
    const QVector<VoiceLine> &index = loadVoiceIndex();
    int bestScore = -1;

    for (const VoiceLine &line : index) {
        const int score = scoreVoiceLine(line, replyText, emotion.trimmed().toLower(), replyTags);
        if (score < bestScore)
            continue;

        if (score > bestScore) {
            matchedVoices.clear();
            bestScore = score;
        }
        matchedVoices.append(line.path);
    }

    QString filePath;
    if (!matchedVoices.isEmpty() && bestScore > 0)
        filePath = pickRandomVoice(matchedVoices);
    if (filePath.isEmpty())
        filePath = pickRandomVoice(commonVoices());
    if (filePath.isEmpty())
        return;

    player_->stop();
    player_->setSource(QUrl::fromLocalFile(filePath));
    player_->play();
}

void VoicePlayer::stop()
{
    player_->stop();
}

QString VoicePlayer::resolveVoiceRoot() const
{
    QStringList candidates;

    const QString besideExe = QDir::cleanPath(
        QCoreApplication::applicationDirPath() + QStringLiteral("/resources/voice"));
    candidates.append(besideExe);

    QDir walkDir(QCoreApplication::applicationDirPath());
    for (int depth = 0; depth < 8; ++depth) {
        const QString walked =
            QDir::cleanPath(walkDir.filePath(QStringLiteral("resources/voice")));
        if (!candidates.contains(walked))
            candidates.append(walked);
        if (!walkDir.cdUp())
            break;
    }

#ifdef HYORI_SOURCE_DIR
    const QString inSource = QDir::cleanPath(
        QStringLiteral(HYORI_SOURCE_DIR) + QStringLiteral("/resources/voice"));
    if (!candidates.contains(inSource))
        candidates.append(inSource);
#endif

    const QString cwdVoice =
        QDir::cleanPath(QDir::currentPath() + QStringLiteral("/resources/voice"));
    if (!candidates.contains(cwdVoice))
        candidates.append(cwdVoice);

    for (const QString &path : candidates) {
        if (QDir(path).exists())
            return path;
    }

    return besideExe;
}

QString VoicePlayer::pickRandomVoice(const QStringList &relativePaths) const
{
    if (relativePaths.isEmpty())
        return {};

    const QString voiceRoot = resolveVoiceRoot();
    QStringList existingFiles;
    existingFiles.reserve(relativePaths.size());

    for (const QString &relativePath : relativePaths) {
        const QString fullPath = QDir(voiceRoot).filePath(relativePath);
        if (QFileInfo::exists(fullPath))
            existingFiles.append(fullPath);
    }

    if (existingFiles.isEmpty())
        return {};

    const int index = QRandomGenerator::global()->bounded(existingFiles.size());
    return existingFiles.at(index);
}

void VoicePlayer::setVolume(double volume)
{
    audioOutput_->setVolume(qBound(0.0, volume, 1.0));
}
