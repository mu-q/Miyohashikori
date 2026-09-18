#include "todowindow.h"

#include "../core/data/todorepository.h"
#include "../core/theme.h"

#include <QCheckBox>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QKeyEvent>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QString itemCaption(const TodoItem &item)
{
    QString state;
    if (item.status == TodoStatus::Done)
        state = QStringLiteral("已完成");
    else if (item.status == TodoStatus::InProgress)
        state = QStringLiteral("进行中");
    else
        state = QStringLiteral("待处理");

    QString detail = state;
    if (item.priority == TodoPriority::High)
        detail += QStringLiteral(" · 高优先级");
    else if (item.priority == TodoPriority::Low)
        detail += QStringLiteral(" · 低优先级");
    if (item.dueAt.isValid())
        detail += QStringLiteral(" · %1")
                      .arg(item.dueAt.toLocalTime().toString(QStringLiteral("MM-dd HH:mm")));
    return QStringLiteral("%1\n%2").arg(item.title, detail);
}

QIcon statusStripeIcon(TodoStatus status)
{
    QColor color(QStringLiteral("#7CAAF8"));
    if (status == TodoStatus::InProgress)
        color = QColor(QStringLiteral("#FFB27D"));
    else if (status == TodoStatus::Done)
        color = QColor(QStringLiteral("#75C7AD"));

    QPixmap pixmap(8, 48);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(QRectF(1, 3, 5, 42), 2.5, 2.5);
    return QIcon(pixmap);
}

} // namespace

TodoWindow::TodoWindow(TodoRepository *repository, QWidget *parent)
    : QWidget(parent), repository_(repository)
{
    setObjectName(QStringLiteral("todoOverlay"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::StrongFocus);

    auto *overlayLayout = new QVBoxLayout(this);
    overlayLayout->setContentsMargins(48, 38, 48, 38);

    auto *panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("todoPanel"));
    panel->setMinimumSize(720, 480);
    panel->setMaximumSize(1080, 680);
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    overlayLayout->addWidget(panel, 1, Qt::AlignCenter);

    auto *shadow = new QGraphicsDropShadowEffect(panel);
    shadow->setBlurRadius(34);
    shadow->setOffset(0, 12);
    shadow->setColor(QColor(4, 8, 18, 180));
    panel->setGraphicsEffect(shadow);

    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(24, 20, 24, 22);
    panelLayout->setSpacing(16);

    auto *header = new QHBoxLayout;
    auto *heading = new QVBoxLayout;
    heading->setSpacing(3);
    auto *eyebrow = new QLabel(QStringLiteral("FOCUS DESK · 今日清单"), panel);
    eyebrow->setObjectName(QStringLiteral("todoEyebrow"));
    auto *title = new QLabel(QStringLiteral("待办事项"), panel);
    title->setObjectName(QStringLiteral("todoTitle"));
    summaryLabel_ = new QLabel(QStringLiteral("把下一步写清楚，就已经开始了。"), panel);
    summaryLabel_->setObjectName(QStringLiteral("todoSummary"));
    heading->addWidget(eyebrow);
    heading->addWidget(title);
    heading->addWidget(summaryLabel_);
    auto *closeButton = new QToolButton(panel);
    closeButton->setObjectName(QStringLiteral("todoClose"));
    closeButton->setText(QStringLiteral("×"));
    closeButton->setToolTip(QStringLiteral("关闭待办事项"));
    closeButton->setCursor(Qt::PointingHandCursor);
    header->addLayout(heading);
    header->addStretch();
    header->addWidget(closeButton, 0, Qt::AlignTop);
    panelLayout->addLayout(header);

    auto *divider = new QFrame(panel);
    divider->setObjectName(QStringLiteral("todoDivider"));
    divider->setFrameShape(QFrame::HLine);
    panelLayout->addWidget(divider);

    auto *content = new QHBoxLayout;
    content->setSpacing(18);

    auto *rail = new QFrame(panel);
    rail->setObjectName(QStringLiteral("todoRail"));
    rail->setMinimumWidth(300);
    rail->setMaximumWidth(370);
    auto *left = new QVBoxLayout(rail);
    left->setContentsMargins(14, 14, 14, 14);
    left->setSpacing(12);
    auto *newButton = new QPushButton(QStringLiteral("＋ 新建待办"), rail);
    newButton->setObjectName(QStringLiteral("newTodo"));
    newButton->setCursor(Qt::PointingHandCursor);
    list_ = new QListWidget(rail);
    list_->setObjectName(QStringLiteral("todoList"));
    list_->setIconSize(QSize(8, 48));
    list_->setSpacing(6);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    left->addWidget(newButton);
    left->addWidget(list_, 1);
    content->addWidget(rail);

    auto *editor = new QFrame(panel);
    editor->setObjectName(QStringLiteral("todoEditor"));
    auto *right = new QVBoxLayout(editor);
    right->setContentsMargins(20, 18, 20, 18);
    right->setSpacing(13);

    auto *editorTitle = new QLabel(QStringLiteral("任务详情"), editor);
    editorTitle->setObjectName(QStringLiteral("todoEditorTitle"));
    right->addWidget(editorTitle);

    auto *form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(12);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    titleEdit_ = new QLineEdit(editor);
    titleEdit_->setObjectName(QStringLiteral("todoTitleEdit"));
    titleEdit_->setPlaceholderText(QStringLiteral("要完成什么？"));
    statusBox_ = new QComboBox(editor);
    statusBox_->addItem(QStringLiteral("待处理"), static_cast<int>(TodoStatus::Pending));
    statusBox_->addItem(QStringLiteral("进行中"), static_cast<int>(TodoStatus::InProgress));
    statusBox_->addItem(QStringLiteral("已完成"), static_cast<int>(TodoStatus::Done));
    priorityBox_ = new QComboBox(editor);
    priorityBox_->addItem(QStringLiteral("低"), static_cast<int>(TodoPriority::Low));
    priorityBox_->addItem(QStringLiteral("普通"), static_cast<int>(TodoPriority::Normal));
    priorityBox_->addItem(QStringLiteral("高"), static_cast<int>(TodoPriority::High));
    dueEnabled_ = new QCheckBox(QStringLiteral("设置截止时间"), editor);
    dueEdit_ = new QDateTimeEdit(QDateTime::currentDateTime().addDays(1), editor);
    dueEdit_->setCalendarPopup(true);
    dueEdit_->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    dueEdit_->setEnabled(false);
    auto *dueRow = new QWidget(editor);
    auto *dueLayout = new QHBoxLayout(dueRow);
    dueLayout->setContentsMargins(0, 0, 0, 0);
    dueLayout->setSpacing(10);
    dueLayout->addWidget(dueEnabled_);
    dueLayout->addWidget(dueEdit_, 1);
    form->addRow(QStringLiteral("标题"), titleEdit_);
    form->addRow(QStringLiteral("状态"), statusBox_);
    form->addRow(QStringLiteral("优先级"), priorityBox_);
    form->addRow(QStringLiteral("截止"), dueRow);
    right->addLayout(form);

    descriptionEdit_ = new QTextEdit(editor);
    descriptionEdit_->setObjectName(QStringLiteral("todoDescriptionEdit"));
    descriptionEdit_->setAcceptRichText(false);
    descriptionEdit_->setPlaceholderText(QStringLiteral("补充说明、步骤或备注…"));
    right->addWidget(descriptionEdit_, 1);

    auto *actions = new QHBoxLayout;
    messageLabel_ = new QLabel(QStringLiteral("待办保存在本机"), editor);
    messageLabel_->setObjectName(QStringLiteral("todoMessage"));
    deleteButton_ = new QPushButton(QStringLiteral("删除"), editor);
    deleteButton_->setObjectName(QStringLiteral("deleteTodo"));
    auto *saveButton = new QPushButton(QStringLiteral("保存任务"), editor);
    saveButton->setObjectName(QStringLiteral("saveTodo"));
    actions->addWidget(messageLabel_, 1);
    actions->addWidget(deleteButton_);
    actions->addWidget(saveButton);
    right->addLayout(actions);
    content->addWidget(editor, 1);
    panelLayout->addLayout(content, 1);

    nightStyle_ = QStringLiteral(R"(
        QWidget#todoOverlay { background: rgba(5, 9, 20, 108); font-family: "Microsoft YaHei UI"; }
        QFrame#todoPanel { background: rgba(23, 34, 56, 205); border: 1px solid rgba(206, 224, 255, 96); border-radius: 22px; }
        QLabel { color: #edf4ff; background: transparent; }
        QLabel#todoEyebrow { color: #8fb9f4; font-size: 11px; font-weight: 700; letter-spacing: 2px; }
        QLabel#todoTitle { color: #f6f9ff; font-size: 28px; font-weight: 700; }
        QLabel#todoSummary, QLabel#todoMessage { color: #aebfda; font-size: 12px; }
        QLabel#todoEditorTitle { color: #edf4ff; font-size: 17px; font-weight: 700; }
        QFrame#todoDivider { color: rgba(206, 224, 255, 42); border: none; border-top: 1px solid rgba(206, 224, 255, 42); }
        QFrame#todoRail { background: rgba(12, 20, 36, 118); border: 1px solid rgba(206, 224, 255, 44); border-radius: 15px; }
        QFrame#todoEditor { background: rgba(30, 43, 68, 145); border: 1px solid rgba(206, 224, 255, 52); border-radius: 15px; }
        QToolButton#todoClose { background: rgba(220, 233, 255, 18); color: #cad8ed; border: 1px solid rgba(206, 224, 255, 42); border-radius: 18px; min-width: 36px; max-width: 36px; min-height: 36px; max-height: 36px; padding: 0; font-size: 24px; font-weight: 400; }
        QToolButton#todoClose:hover { background: rgba(255, 142, 125, 55); color: #ffd9d2; border-color: #ff9c8c; }
        QPushButton { min-height: 36px; padding: 0 15px; border-radius: 10px; background: rgba(72, 91, 126, 205); color: #edf4ff; border: 1px solid rgba(206, 224, 255, 58); font-weight: 600; }
        QPushButton:hover { background: rgba(103, 126, 170, 225); border-color: #a9c8f5; }
        QPushButton#newTodo, QPushButton#saveTodo { background: #7caaf8; color: #14203a; border: none; }
        QPushButton#newTodo:hover, QPushButton#saveTodo:hover { background: #a2c4ff; }
        QPushButton#deleteTodo { background: rgba(255, 142, 125, 25); color: #ffb4a8; border-color: rgba(255, 142, 125, 80); }
        QPushButton#deleteTodo:hover { background: rgba(255, 142, 125, 55); }
        QListWidget#todoList { background: transparent; color: #eef4ff; border: none; outline: none; }
        QListWidget#todoList::item { min-height: 56px; padding: 8px 9px; margin: 0; background: rgba(72, 94, 132, 78); border: 1px solid rgba(206, 224, 255, 36); border-radius: 10px; }
        QListWidget#todoList::item:hover { background: rgba(91, 118, 163, 112); border-color: rgba(169, 200, 245, 95); }
        QListWidget#todoList::item:selected { background: rgba(124, 170, 248, 112); border-color: #8fb9f4; }
        QLineEdit, QTextEdit, QComboBox, QDateTimeEdit { background: rgba(12, 20, 36, 190); color: #edf4ff; border: 1px solid rgba(174, 198, 234, 68); border-radius: 9px; padding: 8px 10px; selection-background-color: #618acb; }
        QLineEdit:focus, QTextEdit:focus, QComboBox:focus, QDateTimeEdit:focus { border-color: #8fb9f4; }
        QComboBox::drop-down { border: none; width: 25px; }
        QComboBox QAbstractItemView { background: #202d47; color: #edf4ff; selection-background-color: #5278b5; }
        QTextEdit { min-height: 110px; }
        QCheckBox { color: #dbe6f7; spacing: 8px; }
        QCheckBox::indicator { width: 16px; height: 16px; }
    )");
    applyTheme();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &TodoWindow::applyTheme);

    connect(closeButton, &QToolButton::clicked, this, &TodoWindow::requestClose);
    connect(newButton, &QPushButton::clicked, this, [this] { if (confirmDiscard()) beginNew(); });
    connect(list_, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item || loading_) return;
        const qint64 id = item->data(Qt::UserRole).toLongLong();
        if (id == currentId_) return;
        if (confirmDiscard()) loadItem(id); else reload(currentId_);
    });
    connect(saveButton, &QPushButton::clicked, this, &TodoWindow::saveCurrent);
    connect(deleteButton_, &QPushButton::clicked, this, &TodoWindow::removeCurrent);
    connect(dueEnabled_, &QCheckBox::toggled, dueEdit_, &QWidget::setEnabled);
    connect(titleEdit_, &QLineEdit::textEdited, this, &TodoWindow::markDirty);
    connect(descriptionEdit_, &QTextEdit::textChanged, this, &TodoWindow::markDirty);
    connect(statusBox_, &QComboBox::currentIndexChanged, this, &TodoWindow::markDirty);
    connect(priorityBox_, &QComboBox::currentIndexChanged, this, &TodoWindow::markDirty);
    connect(dueEnabled_, &QCheckBox::toggled, this, &TodoWindow::markDirty);
    connect(dueEdit_, &QDateTimeEdit::dateTimeChanged, this, &TodoWindow::markDirty);

    qApp->installEventFilter(this);

    reload();
    beginNew();
}

void TodoWindow::applyTheme()
{
    QString style = nightStyle_;
    if (ThemeManager::instance()->isLight()) {
        style += QStringLiteral(R"(
            QWidget#todoOverlay { background: rgba(35, 51, 81, 68); }
            QFrame#todoPanel { background: rgba(244, 248, 255, 214); border-color: rgba(255,255,255,220); }
            QLabel { color:#253552; }
            QLabel#todoEyebrow { color:#6d5ba6; }
            QLabel#todoTitle, QLabel#todoEditorTitle { color:#1f2e49; }
            QLabel#todoSummary, QLabel#todoMessage { color:#64738c; }
            QFrame#todoDivider { border-top-color:rgba(70,94,130,45); }
            QFrame#todoRail { background:rgba(226,236,251,150); border-color:rgba(93,118,157,42); }
            QFrame#todoEditor { background:rgba(255,255,255,155); border-color:rgba(93,118,157,48); }
            QToolButton#todoClose { background:rgba(255,255,255,90); color:#52627d; border-color:rgba(93,118,157,45); }
            QPushButton { background:rgba(218,229,246,205); color:#253552; border-color:rgba(91,116,154,60); }
            QPushButton:hover { background:rgba(202,215,238,235); border-color:#8b78c5; }
            QPushButton#newTodo, QPushButton#saveTodo { background:#8b78c5; color:white; }
            QPushButton#newTodo:hover, QPushButton#saveTodo:hover { background:#7461ad; }
            QPushButton#deleteTodo { background:rgba(255,142,125,35); color:#a44e48; border-color:rgba(189,83,72,80); }
            QListWidget#todoList { color:#253552; }
            QListWidget#todoList::item { background:rgba(255,255,255,92); border-color:rgba(87,112,149,36); }
            QListWidget#todoList::item:hover { background:rgba(226,236,251,190); }
            QListWidget#todoList::item:selected { background:rgba(139,120,197,70); border-color:#8b78c5; }
            QLineEdit, QTextEdit, QComboBox, QDateTimeEdit { background:rgba(255,255,255,190); color:#253552; border-color:rgba(89,113,151,65); selection-background-color:#8b78c5; }
            QLineEdit:focus, QTextEdit:focus, QComboBox:focus, QDateTimeEdit:focus { border-color:#8b78c5; }
            QComboBox QAbstractItemView { background:#f8fbff; color:#253552; selection-background-color:#d9d0f0; }
            QCheckBox { color:#344561; }
        )");
    }
    setStyleSheet(style);
}

void TodoWindow::present()
{
    reload(currentId_);
    show();
    raise();
    (currentId_ < 0 ? static_cast<QWidget *>(titleEdit_) : static_cast<QWidget *>(list_))->setFocus();
}

bool TodoWindow::requestClose()
{
    if (!confirmDiscard()) return false;
    hide();
    emit dismissed();
    return true;
}

void TodoWindow::closeEvent(QCloseEvent *event)
{
    if (confirmDiscard()) { event->accept(); emit dismissed(); }
    else { event->ignore(); }
}

bool TodoWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (isVisible() && !QApplication::activeModalWidget()
        && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        auto *widget = qobject_cast<QWidget *>(watched);
        if (keyEvent->key() == Qt::Key_Escape && widget
            && (widget == this || isAncestorOf(widget))) {
            requestClose();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TodoWindow::reload(qint64 selectId)
{
    loading_ = true;
    list_->clear();
    const auto result = repository_->list();
    if (!result.success) {
        setStatus(result.error, true);
        summaryLabel_->setText(QStringLiteral("暂时无法读取待办事项"));
        loading_ = false;
        return;
    }
    int pending = 0, inProgress = 0, done = 0;
    for (const TodoItem &todo : result.value) {
        if (todo.status == TodoStatus::Done) ++done;
        else if (todo.status == TodoStatus::InProgress) ++inProgress;
        else ++pending;
        auto *item = new QListWidgetItem(statusStripeIcon(todo.status), itemCaption(todo), list_);
        item->setData(Qt::UserRole, todo.id);
        item->setSizeHint(QSize(250, 72));
        if (todo.status == TodoStatus::Done) item->setForeground(QColor(QStringLiteral("#a5b5c9")));
        if (todo.id == selectId) list_->setCurrentItem(item);
    }
    summaryLabel_->setText(QStringLiteral("%1 项待处理 · %2 项进行中 · %3 项已完成").arg(pending).arg(inProgress).arg(done));
    loading_ = false;
}

void TodoWindow::beginNew()
{
    loading_ = true;
    currentId_ = -1;
    list_->clearSelection();
    titleEdit_->clear();
    descriptionEdit_->clear();
    statusBox_->setCurrentIndex(0);
    priorityBox_->setCurrentIndex(1);
    dueEnabled_->setChecked(false);
    dueEdit_->setDateTime(QDateTime::currentDateTime().addDays(1));
    deleteButton_->setEnabled(false);
    setStatus(QStringLiteral("新任务尚未保存"));
    loading_ = false;
    dirty_ = false;
    titleEdit_->setFocus();
}

void TodoWindow::loadItem(qint64 id)
{
    const auto result = repository_->findById(id);
    if (!result.success || !result.value.has_value()) {
        setStatus(result.success ? QStringLiteral("待办已经不存在") : result.error, true);
        reload(); beginNew(); return;
    }
    const TodoItem &todo = *result.value;
    loading_ = true;
    currentId_ = todo.id;
    titleEdit_->setText(todo.title);
    descriptionEdit_->setPlainText(todo.description);
    statusBox_->setCurrentIndex(statusBox_->findData(static_cast<int>(todo.status)));
    priorityBox_->setCurrentIndex(priorityBox_->findData(static_cast<int>(todo.priority)));
    dueEnabled_->setChecked(todo.dueAt.isValid());
    if (todo.dueAt.isValid()) dueEdit_->setDateTime(todo.dueAt.toLocalTime());
    deleteButton_->setEnabled(true);
    loading_ = false;
    dirty_ = false;
    setStatus(QStringLiteral("上次更新 %1").arg(todo.updatedAt.toLocalTime().toString(QStringLiteral("MM-dd HH:mm"))));
    reload(todo.id);
}

void TodoWindow::saveCurrent()
{
    const QString title = titleEdit_->text().trimmed();
    if (title.isEmpty()) { setStatus(QStringLiteral("请先填写待办标题"), true); titleEdit_->setFocus(); return; }
    const TodoStatus status = static_cast<TodoStatus>(statusBox_->currentData().toInt());
    const TodoPriority priority = static_cast<TodoPriority>(priorityBox_->currentData().toInt());
    const QDateTime dueAt = dueEnabled_->isChecked() ? dueEdit_->dateTime() : QDateTime();
    if (currentId_ < 0) {
        const auto result = repository_->create(title, descriptionEdit_->toPlainText(), priority, dueAt, status);
        if (!result.success) { setStatus(result.error, true); return; }
        currentId_ = result.value.id;
    } else {
        TodoItem item;
        item.id = currentId_; item.title = title; item.description = descriptionEdit_->toPlainText();
        item.status = status; item.priority = priority; item.dueAt = dueAt;
        const auto result = repository_->update(item);
        if (!result.success || !result.value) {
            setStatus(result.success ? QStringLiteral("待办已经不存在") : result.error, true); return;
        }
    }
    dirty_ = false;
    loadItem(currentId_);
    setStatus(QStringLiteral("已保存到本机"));
}

void TodoWindow::removeCurrent()
{
    if (currentId_ < 0) return;
    if (QMessageBox::question(this, QStringLiteral("删除待办"), QStringLiteral("确定删除当前待办吗？")) != QMessageBox::Yes) return;
    const auto result = repository_->remove(currentId_);
    if (!result.success || !result.value) {
        setStatus(result.success ? QStringLiteral("待办已经不存在") : result.error, true); return;
    }
    dirty_ = false;
    reload(); beginNew(); setStatus(QStringLiteral("待办已删除"));
}

bool TodoWindow::confirmDiscard()
{
    if (!dirty_) return true;
    const auto answer = QMessageBox::question(this, QStringLiteral("放弃修改"),
        QStringLiteral("当前待办尚未保存，确定放弃修改吗？"),
        QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
    return answer == QMessageBox::Discard;
}

void TodoWindow::setStatus(const QString &message, bool error)
{
    messageLabel_->setText(message);
    messageLabel_->setStyleSheet(error ? QStringLiteral("color:#ffaaa0;") : QString());
}

void TodoWindow::markDirty()
{
    if (loading_) return;
    dirty_ = true;
    setStatus(QStringLiteral("有尚未保存的修改"));
}
