#pragma once

#include <QWidget>

/// 椭圆形对话气泡，用于展示 AI 回复与状态提示。
class ReplyBubble : public QWidget
{
    Q_OBJECT

public:
    enum class Tone
    {
        Message,
        Status,
        Error,
    };

    explicit ReplyBubble(QWidget *parent = nullptr);

    void setText(const QString &text);
    QString text() const { return text_; }

    void setTone(Tone tone);
    Tone tone() const { return tone_; }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor textColor() const;
    QColor backgroundColor() const;
    QColor borderColor() const;
    QRect textRect() const;

    QString text_;
    Tone tone_ = Tone::Message;
};
