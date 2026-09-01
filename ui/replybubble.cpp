#include "replybubble.h"

#include <QPainter>
#include <QPaintEvent>
#include <QTextLayout>
#include <QTextOption>

namespace {
constexpr int kMaxTextWidth = 228;
constexpr int kTextHorizontalInset = 18;
constexpr int kTextVerticalInset = 20;
constexpr int kBubbleWidth = 296;
constexpr int kMinBubbleHeight = 68;

int wrappedTextHeight(const QString &text, const QFont &font)
{
    QTextLayout layout(text, font);
    QTextOption option;
    option.setAlignment(Qt::AlignHCenter);
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);

    qreal height = 0;
    layout.beginLayout();
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(kMaxTextWidth);
        line.setPosition(QPointF(0, height));
        height += line.height();
    }
    layout.endLayout();
    return qCeil(height);
}
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
        return QSize(kBubbleWidth, kMinBubbleHeight);

    // 测量宽度不大于实际绘制宽度，保证绘制时即使中文自动换行也不会多出一行。
    const int contentHeight = wrappedTextHeight(text_, font());
    const int height = qMax(kMinBubbleHeight, contentHeight + kTextVerticalInset * 2 + 12);
    return QSize(kBubbleWidth, height);
}

QRect ReplyBubble::textRect() const
{
    return rect().adjusted(kTextHorizontalInset, kTextVerticalInset,
                           -kTextHorizontalInset, -kTextVerticalInset);
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
