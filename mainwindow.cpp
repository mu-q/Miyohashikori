#include "mainwindow.h"

#include "core/ai/iaisession.h"
#include "core/ai/openaichatsession.h"
#include "core/config/appconfig.h"
#include "core/config/configmanager.h"
#include "core/spritecatalog.h"
#include "ui/characterspriteview.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QGuiApplication>
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
    , configManager_(new ConfigManager(this))
    , ai_(nullptr)
{
    applyWindowChrome();
    configManager_->load();
    ai_ = new OpenAiChatSession(configManager_, this);

    replyLabel_->setWordWrap(true);
    replyLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    replyLabel_->setStyleSheet(QStringLiteral("QLabel { color: #e8eaf0; font-size: 12px; }"));
    replyLabel_->setMinimumWidth(240);
    replyLabel_->setMaximumHeight(112);
    replyLabel_->setText(QStringLiteral("原型阶段：请先在 ~/.hyori/config.json 填写 LLM 配置。"));

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
        const QString text = inputLine_->text().trimmed();
        if (text.isEmpty())
            return;

        replyLabel_->setText(QStringLiteral("正在等待冰织回复…"));
        ai_->submit(text);
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

void MainWindow::persistWindowPosition() const
{
    AppConfig config = configManager_->config();
    config.windowPos = pos();
    configManager_->setConfig(config);
    configManager_->save();
}

void MainWindow::wireAiSession()
{
    connect(ai_, &IAiSession::assistantMessage, this, [this](const QString &text) {
        replyLabel_->setText(text);
    });
    connect(ai_, &IAiSession::sessionError, this, [this](const QString &error) {
        replyLabel_->setText(error);
    });
    connect(ai_, &IAiSession::assistantEmotion, this, [this](const QString &token) {
        replyLabel_->setText(replyLabel_->text() + QStringLiteral("\n[emotion:%1]").arg(token));
    });
}

void MainWindow::syncChromeToSprite()
{
    sprite_->adjustSize();
    layout()->activate();
    adjustSize();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    persistWindowPosition();
    QWidget::closeEvent(event);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    const QPoint storedPos = configManager_->config().windowPos;
    if (!storedPos.isNull()) {
        move(storedPos);
    } else {
        const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
        move(avail.bottomRight() - QPoint(width() + 24, height() + 24));
    }

    syncChromeToSprite();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    const bool onSprite = (obj == sprite_);

    if (onSprite && event->type() == QEvent::Wheel) {
        auto *wheelEvent = static_cast<QWheelEvent *>(event);
        if (wheelEvent->modifiers() & Qt::AltModifier) {
            int delta = wheelEvent->angleDelta().y();
            if (delta == 0)
                delta = wheelEvent->angleDelta().x();
            const int dir = (delta > 0) ? 1 : (delta < 0 ? -1 : 0);
            if (dir != 0) {
                const qreal nextScale = sprite_->scale() + dir * 0.05;
                sprite_->setScale(nextScale);
                syncChromeToSprite();
            }
            wheelEvent->accept();
            return true;
        }
    }

    if (onSprite) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton
                && (mouseEvent->modifiers() & Qt::AltModifier)) {
                startDrag(mouseEvent->globalPosition().toPoint());
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            if (dragging_) {
                auto *mouseEvent = static_cast<QMouseEvent *>(event);
                updateDrag(mouseEvent->globalPosition().toPoint());
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
    const int threshold = QGuiApplication::styleHints()->startDragDistance();
    if (!draggingStarted_) {
        const QPoint delta = globalPos - (dragOffset_ + frameGeometry().topLeft());
        if (delta.manhattanLength() < threshold)
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
    persistWindowPosition();
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
            QAction *action = modesMenu->addAction(name);
            action->setCheckable(true);
            action->setChecked(name == current);
            connect(action, &QAction::triggered, this, [this, name] { catalog_->setMode(name); });
        }
    }

    menu.addSeparator();
    menu.addAction(QStringLiteral("退出"), qApp, &QApplication::quit);
    menu.exec(globalPos);
}
