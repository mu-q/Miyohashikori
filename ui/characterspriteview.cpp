#include "characterspriteview.h"

#include "../core/spritecatalog.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

CharacterSpriteView::CharacterSpriteView(SpriteCatalog *catalog, QWidget *parent)
    : QWidget(parent)
    , catalog_(catalog)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    connect(catalog_, &SpriteCatalog::frameChanged, this, [this](int) {
        refreshFromCatalog();
    });
    connect(catalog_, &SpriteCatalog::modeChanged, this, [this](const QString &) {
        refreshFromCatalog();
    });
    refreshFromCatalog();
}

void CharacterSpriteView::setScale(qreal s)
{
    s = std::clamp(s, 0.5, 1.0);
    if (qFuzzyCompare(scale_, s))
        return;
    scale_ = s;
    updateGeometry();
    update();
}

QRect CharacterSpriteView::imageRect() const
{
    const QImage img = catalog_->currentImage();
    if (!img.isNull()) {
        const int w = qMax(1, qRound(img.width() * scale_));
        const int h = qMax(1, qRound(img.height() * scale_));
        const int x = (width() - w) / 2;
        const int y = (height() - h) / 2;
        return QRect(x, y, w, h);
    }
    const QSize hint = sizeHint();
    return QRect((width() - hint.width()) / 2, (height() - hint.height()) / 2, hint.width(),
                 hint.height());
}

QSize CharacterSpriteView::sizeHint() const
{
    const QImage img = catalog_->currentImage();
    if (!img.isNull()) {
        return QSize(qMax(1, qRound(img.width() * scale_)), qMax(1, qRound(img.height() * scale_)));
    }
    return QSize(qRound(280 * scale_), qRound(420 * scale_));
}

void CharacterSpriteView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);

    const QImage img = catalog_->currentImage();
    const QRect r = imageRect();

    if (!img.isNull()) {
        p.drawImage(r, img);
        return;
    }

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(32, 36, 48, 220));
    p.drawRoundedRect(r, 12, 12);
    p.setPen(QColor(220, 224, 232));
    p.drawText(r, Qt::AlignCenter,
               QStringLiteral("将立绘放入\nassets/modes/<模式名>/\n支持 png / jpg / webp"));
}

void CharacterSpriteView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const bool altDrag = event->modifiers() & Qt::AltModifier;
        if (!altDrag)
            catalog_->nextFrame();
    } else if (event->button() == Qt::RightButton) {
        emit rightClicked();
    }
    QWidget::mousePressEvent(event);
}

void CharacterSpriteView::refreshFromCatalog()
{
    updateGeometry();
    update();
}
