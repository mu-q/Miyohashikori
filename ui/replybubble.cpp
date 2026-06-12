#include "replybubble.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPaintEvent>

namespace {
constexpr int kMaxTextWidth = 228;
constexpr int kHorizontalPadding = 32;
constexpr int kVerticalPadding = 24;
constexpr int kMinBubbleWidth = 200;
constexpr int kMinBubbleHeight = 52;
} // namespace

ReplyBubble::ReplyBubble(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    QFont bubbleFont = font();
    bubbleFont.setPointSize(10);
    setFont(bubbleFont);
}

void ReplyBubble::setText(const QString &text)
{
    if (text_ == text)
        return;
    text_ = text;
    updateGeometry();
    update();
}

void ReplyBubble::setTone(Tone tone)
{
    if (tone_ == tone)
        return;
    tone_ = tone;
    update();
}

QSize ReplyBubble::sizeHint() const
{
    if (text_.isEmpty())
        return QSize(kMinBubbleWidth, kMinBubbleHeight);

    const QFontMetrics metrics(font());
    const QRect bounds =
        metrics.boundingRect(0, 0, kMaxTextWidth, 10000, Qt::AlignCenter | Qt::TextWordWrap, text_);

    const int width = qBound(kMinBubbleWidth, bounds.width() + kHorizontalPadding, 296);
    const int height = qMax(kMinBubbleHeight, bounds.height() + kVerticalPadding);
    return QSize(width, height);
}

QRect ReplyBubble::textRect() const
{
    return rect().adjusted(18, 14, -18, -14);
}

QColor ReplyBubble::textColor() const
{
    switch (tone_) {
    case Tone::Message:
        return QColor(72, 156, 255);
    case Tone::Status:
        return QColor(110, 180, 255);
    case Tone::Error:
        return QColor(255, 120, 120);
    }
    return QColor(72, 156, 255);
}

QColor ReplyBubble::backgroundColor() const
{
    switch (tone_) {
    case Tone::Message:
        return QColor(18, 28, 48, 215);
    case Tone::Status:
        return QColor(18, 32, 56, 205);
    case Tone::Error:
        return QColor(48, 20, 28, 215);
    }
    return QColor(18, 28, 48, 215);
}

QColor ReplyBubble::borderColor() const
{
    switch (tone_) {
    case Tone::Message:
        return QColor(72, 156, 255, 140);
    case Tone::Status:
        return QColor(110, 180, 255, 120);
    case Tone::Error:
        return QColor(255, 120, 120, 140);
    }
    return QColor(72, 156, 255, 140);
}

void ReplyBubble::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, true);

    const QRect bubbleRect = rect().adjusted(1, 1, -2, -2);
    painter.setPen(QPen(borderColor(), 1.5));
    painter.setBrush(backgroundColor());
    painter.drawEllipse(bubbleRect);

    painter.setPen(textColor());
    painter.setFont(font());
    painter.drawText(textRect(), Qt::AlignCenter | Qt::TextWordWrap, text_);
}
