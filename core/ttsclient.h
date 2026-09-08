#pragma once

#include <QObject>
#include <QtGlobal>

class AppConfig;
class QNetworkAccessManager;
class QNetworkReply;

class TtsClient : public QObject
{
    Q_OBJECT

public:
    explicit TtsClient(QObject *parent = nullptr);

    bool isConfigured(const AppConfig &config) const;
    quint64 synthesize(const QString &text, const AppConfig &config);
    void cancel();

signals:
    void audioReady(quint64 requestId, const QString &filePath);
    void synthesisFailed(quint64 requestId, const QString &reason);

private:
    QNetworkAccessManager *network_ = nullptr;
    QNetworkReply *activeReply_ = nullptr;
    quint64 latestRequestId_ = 0;
};
