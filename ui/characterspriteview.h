#pragma once

#include <QWidget>

class SpriteCatalog;

/// 绘制当前立绘（PNG/JPEG/WebP 透明底）；无资源时显示占位提示。
class CharacterSpriteView : public QWidget
{
    Q_OBJECT

public:
    explicit CharacterSpriteView(SpriteCatalog *catalog, QWidget *parent = nullptr);

    qreal scale() const { return scale_; }
    void setScale(qreal s);

    QRect imageRect() const;
    QSize sizeHint() const override;

signals:
    void rightClicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void refreshFromCatalog();

    SpriteCatalog *catalog_ = nullptr;
    qreal scale_ = 1.0;
};
