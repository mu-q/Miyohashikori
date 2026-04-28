#include "mainwindow.h"

#include "core/ai/iaisession.h"
#include "core/ai/nullaisession.h"
#include "core/spritecatalog.h"
#include "ui/characterspriteview.h"

#include <QApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QScreen>
#include <QShowEvent>
#include <QStyleHints>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , catalog_(new SpriteCatalog(this))
    , sprite_(new CharacterSpriteView(catalog_, this))
    , replyLabel_(new QLabel(this))
    , inputLine_(new QLineEdit(this))
    , ai_(new NullAiSession(this))
{
    applyWindowChrome();

    replyLabel_->setWordWrap(true);
    replyLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    replyLabel_->setStyleSheet(QStringLiteral("QLabel { color: #e8eaf0; font-size: 12px; }"));
    replyLabel_->setMinimumWidth(240);
    replyLabel_->setMaximumHeight(96);
    replyLabel_->setText(QStringLiteral("原型：回车发送，占位 AI 将回显。"));

    inputLine_->setPlaceholderText(QStringLiteral("输入对话…（回车发送）"));
    inputLine_->setClearButtonEnabled(true);
    inputLine_->setStyleSheet(
        QStringLiteral("QLineEdit { background: rgba(40,44,56,0.92); color: #e8eaf0; "
                       "border: 1px solid #5c6370; border-radius: 6px; padding: 6px; }"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);
    root->addWidget(sprite_, 0, Qt::AlignHCenter);
    root->addWidget(replyLabel_);
    root->addWidget(inputLine_);

    setLayout(root);

    sprite_->setCursor(Qt::OpenHandCursor);
    sprite_->installEventFilter(this);

    connect(sprite_, &CharacterSpriteView::rightClicked, this, [this] {
        showPetMenu(QCursor::pos());
    });

    connect(catalog_, &SpriteCatalog::frameChanged, this, [this](int) { syncChromeToSprite(); });
    connect(catalog_, &SpriteCatalog::modeChanged, this, [this](const QString &) {
        syncChromeToSprite();
    });

    connect(inputLine_, &QLineEdit::returnPressed, this, [this] {
        const QString t = inputLine_->text().trimmed();
        if (t.isEmpty())
            return;
        replyLabel_->setText(QStringLiteral("…"));
        ai_->submit(t);
        inputLine_->clear();
    });

    wireAiSession();

    QTimer::singleShot(0, this, [this] { syncChromeToSprite(); });
}

MainWindow::~MainWindow() = default;

void MainWindow::applyWindowChrome()
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setStyleSheet(QStringLiteral("#MainWindowPet { background: transparent; }"));
    setObjectName(QStringLiteral("MainWindowPet"));
}

void MainWindow::wireAiSession()
{
    connect(ai_, &IAiSession::assistantMessage, this, [this](const QString &text) {
        replyLabel_->setText(text);
    });
    connect(ai_, &IAiSession::sessionError, this, [this](const QString &e) {
        replyLabel_->setText(e);
    });
    connect(ai_, &IAiSession::assistantEmotion, this,
            [](const QString &token) { Q_UNUSED(token); });
}

void MainWindow::syncChromeToSprite()
{
    sprite_->adjustSize();
    layout()->activate();
    adjustSize();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
    move(avail.bottomRight() - QPoint(width() + 24, height() + 24));
    syncChromeToSprite();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    const bool onSprite = (obj == sprite_);

    if (onSprite && event->type() == QEvent::Wheel) {
        auto *we = static_cast<QWheelEvent *>(event);
        if (we->modifiers() & Qt::AltModifier) {
            int delta = we->angleDelta().y();
            if (delta == 0)
                delta = we->angleDelta().x();
            const int dir = (delta > 0) ? 1 : (delta < 0 ? -1 : 0);
            if (dir != 0) {
                const qreal s = sprite_->scale() + dir * 0.05;
                sprite_->setScale(s);
                syncChromeToSprite();
            }
            we->accept();
            return true;
        }
    }

    if (onSprite) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton && (me->modifiers() & Qt::AltModifier)) {
                startDrag(me->globalPosition().toPoint());
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            if (dragging_) {
                auto *me = static_cast<QMouseEvent *>(event);
                updateDrag(me->globalPosition().toPoint());
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            if (dragging_) {
                endDrag();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    return QWidget::eventFilter(obj, event);
}

void MainWindow::startDrag(const QPoint &globalPress)
{
    dragging_ = true;
    draggingStarted_ = false;
    dragOffset_ = globalPress - frameGeometry().topLeft();
    sprite_->setCursor(Qt::ClosedHandCursor);
    sprite_->grabMouse();
}

void MainWindow::updateDrag(const QPoint &globalPos)
{
    const int thresh = QGuiApplication::styleHints()->startDragDistance();
    if (!draggingStarted_) {
        const QPoint cur = globalPos;
        const QPoint delta = cur - (dragOffset_ + frameGeometry().topLeft());
        if (delta.manhattanLength() < thresh)
            return;
        draggingStarted_ = true;
    }
    move(globalPos - dragOffset_);
}

void MainWindow::endDrag()
{
    sprite_->releaseMouse();
    dragging_ = false;
    draggingStarted_ = false;
    sprite_->setCursor(Qt::OpenHandCursor);
}

void MainWindow::showPetMenu(const QPoint &globalPos)
{
    QMenu menu;
    QMenu *modesMenu = menu.addMenu(QStringLiteral("切换模式"));
    const QStringList names = catalog_->modeNames();
    const QString current = catalog_->currentMode();
    if (names.isEmpty()) {
        modesMenu->addAction(QStringLiteral("（无）"))->setEnabled(false);
    } else {
        for (const QString &name : names) {
            QAction *a = modesMenu->addAction(name);
            a->setCheckable(true);
            a->setChecked(name == current);
            connect(a, &QAction::triggered, this, [this, name] { catalog_->setMode(name); });
        }
    }

    menu.addSeparator();
    menu.addAction(QStringLiteral("退出"), qApp, &QApplication::quit);
    menu.exec(globalPos);
}
