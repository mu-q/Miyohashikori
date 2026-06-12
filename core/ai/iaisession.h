#pragma once

#include <QObject>
#include <QString>

/// AI 对话扩展点：后续可替换为 HTTP 客户端实现，当前原型使用 `NullAiSession`。
class IAiSession : public QObject
{
    Q_OBJECT

public:
    explicit IAiSession(QObject *parent = nullptr) : QObject(parent) {}

    virtual void submit(const QString &userText) = 0;

signals:
    void assistantMessage(const QString &text);
    void assistantEmotion(const QString &token);
    void sessionStatus(const QString &message);
    void sessionError(const QString &message);
};
