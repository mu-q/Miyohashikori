#include "ttsclient.h"

#include "apppaths.h"
#include "config/appconfig.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>

namespace {
constexpr int kTtsTimeoutMs = 120000;

QString errorFromResponse(const QByteArray &body)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isObject())
        return {};

    const QJsonObject object = document.object();
    QString message = object.value(QStringLiteral("message")).toString().trimmed();
    const QString exception = object.value(QStringLiteral("Exception")).toString().trimmed();
    if (!exception.isEmpty()) {
        if (!message.isEmpty())
            message.append(QStringLiteral(": "));
        message.append(exception);
    }
    return message;
}

bool isWaveAudio(const QByteArray &body)
{
    return body.size() > 44 && body.startsWith("RIFF")
           && body.mid(8, 4) == QByteArrayLiteral("WAVE");
}
} // namespace

TtsClient::TtsClient(QObject *parent)
    : QObject(parent)
    , network_(new QNetworkAccessManager(this))
{
}

bool TtsClient::isConfigured(const AppConfig &config) const
{
    const QUrl endpoint(config.ttsEndpoint.trimmed());
    return config.ttsEnabled && endpoint.isValid() && !endpoint.scheme().isEmpty()
           && !config.ttsReferenceAudioPath.trimmed().isEmpty()
           && !config.ttsReferenceText.trimmed().isEmpty()
           && !config.ttsReferenceLanguage.trimmed().isEmpty()
           && !config.ttsTextLanguage.trimmed().isEmpty();
}

quint64 TtsClient::synthesize(const QString &text, const AppConfig &config)
{
    cancel();
    const quint64 requestId = ++latestRequestId_;

    QJsonObject payload;
    payload.insert(QStringLiteral("text"), text.trimmed());
    payload.insert(QStringLiteral("text_lang"), config.ttsTextLanguage);
    payload.insert(QStringLiteral("ref_audio_path"), config.ttsReferenceAudioPath);
    payload.insert(QStringLiteral("prompt_text"), config.ttsReferenceText);
    payload.insert(QStringLiteral("prompt_lang"), config.ttsReferenceLanguage);
    payload.insert(QStringLiteral("top_k"), 15);
    payload.insert(QStringLiteral("top_p"), 1.0);
    payload.insert(QStringLiteral("temperature"), 1.0);
    payload.insert(QStringLiteral("text_split_method"), QStringLiteral("cut5"));
    payload.insert(QStringLiteral("batch_size"), 1);
    payload.insert(QStringLiteral("speed_factor"), config.ttsSpeedFactor);
    payload.insert(QStringLiteral("media_type"), QStringLiteral("wav"));
    payload.insert(QStringLiteral("streaming_mode"), false);

    const QByteArray requestBody = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    const QString cacheName = QString::fromLatin1(
                                  QCryptographicHash::hash(requestBody, QCryptographicHash::Sha256)
                                      .toHex())
                              + QStringLiteral(".wav");
    const QString cachePath = QDir(AppPaths::ttsCacheRoot()).filePath(cacheName);
    const QFileInfo cachedFile(cachePath);
    if (cachedFile.isFile() && cachedFile.size() > 44) {
        QTimer::singleShot(0, this, [this, requestId, cachePath] {
            if (requestId == latestRequestId_)
                emit audioReady(requestId, cachePath);
        });
        return requestId;
    }

    QNetworkRequest request{QUrl(config.ttsEndpoint)};
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json; charset=utf-8"));
    request.setTransferTimeout(kTtsTimeoutMs);
    QNetworkReply *reply = network_->post(request, requestBody);
    activeReply_ = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId, cachePath] {
        const QByteArray body = reply->readAll();
        const int statusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (activeReply_ == reply)
            activeReply_ = nullptr;

        if (requestId != latestRequestId_) {
            reply->deleteLater();
            return;
        }

        if (reply->error() != QNetworkReply::NoError || statusCode < 200
            || statusCode >= 300 || !isWaveAudio(body)) {
            QString reason = errorFromResponse(body);
            if (reason.isEmpty())
                reason = reply->errorString();
            if (reason.isEmpty())
                reason = QStringLiteral("TTS 服务没有返回有效的 WAV 音频。");
            reply->deleteLater();
            emit synthesisFailed(requestId, reason);
            return;
        }

        QDir cacheDir(AppPaths::ttsCacheRoot());
        if (!cacheDir.exists() && !cacheDir.mkpath(QStringLiteral("."))) {
            reply->deleteLater();
            emit synthesisFailed(requestId, QStringLiteral("无法创建 TTS 音频缓存目录。"));
            return;
        }

        QSaveFile output(cachePath);
        if (!output.open(QIODevice::WriteOnly) || output.write(body) != body.size()
            || !output.commit()) {
            reply->deleteLater();
            emit synthesisFailed(requestId, QStringLiteral("无法保存 TTS 音频缓存。"));
            return;
        }

        reply->deleteLater();
        emit audioReady(requestId, cachePath);
    });

    return requestId;
}

void TtsClient::cancel()
{
    ++latestRequestId_;
    if (!activeReply_)
        return;

    disconnect(activeReply_, nullptr, this, nullptr);
    activeReply_->abort();
    activeReply_->deleteLater();
    activeReply_ = nullptr;
}
