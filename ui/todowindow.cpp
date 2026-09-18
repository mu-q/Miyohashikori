#include "todowindow.h"

#include "../core/data/todorepository.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

QString itemCaption(const TodoItem &item)
{
    QString state;
    if (item.status == TodoStatus::Done)
        state = QStringLiteral("✓");
    else if (item.status == TodoStatus::InProgress)
        state = QStringLiteral("进行中");
    else
        state = QStringLiteral("待处理");
    QString detail = state;
    if (item.priority == TodoPriority::High)
        detail += QStringLiteral(" · 高优先级");
    if (item.dueAt.isValid())
        detail += QStringLiteral(" · %1").arg(item.dueAt.toLocalTime().toString(QStringLiteral("MM-dd HH:mm")));
    return QStringLiteral("%1\n%2").arg(item.title, detail);
}

} // namespace

TodoWindow::TodoWindow(TodoRepository *repository, QWidget *parent)
    : QWidget(parent), repository_(repository)
{
    setWindowTitle(QStringLiteral("冰织待办"));
    setMinimumSize(760, 500);
    resize(880, 580);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(14);

    auto *left = new QVBoxLayout;
    auto *newButton = new QPushButton(QStringLiteral("＋ 新建待办"), this);
    list_ = new QListWidget(this);
    list_->setMinimumWidth(285);
    left->addWidget(newButton);
    left->addWidget(list_, 1);
    root->addLayout(left);

    auto *right = new QVBoxLayout;
    auto *form = new QFormLayout;
    titleEdit_ = new QLineEdit(this);
    titleEdit_->setPlaceholderText(QStringLiteral("要完成什么？"));
    statusBox_ = new QComboBox(this);
    statusBox_->addItem(QStringLiteral("待处理"), static_cast<int>(TodoStatus::Pending));
    statusBox_->addItem(QStringLiteral("进行中"), static_cast<int>(TodoStatus::InProgress));
    statusBox_->addItem(QStringLiteral("已完成"), static_cast<int>(TodoStatus::Done));
    priorityBox_ = new QComboBox(this);
    priorityBox_->addItem(QStringLiteral("低"), static_cast<int>(TodoPriority::Low));
    priorityBox_->addItem(QStringLiteral("普通"), static_cast<int>(TodoPriority::Normal));
    priorityBox_->addItem(QStringLiteral("高"), static_cast<int>(TodoPriority::High));
    dueEnabled_ = new QCheckBox(QStringLiteral("设置截止时间"), this);
    dueEdit_ = new QDateTimeEdit(QDateTime::currentDateTime().addDays(1), this);
    dueEdit_->setCalendarPopup(true);
    dueEdit_->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    dueEdit_->setEnabled(false);
    auto *dueRow = new QWidget(this);
    auto *dueLayout = new QHBoxLayout(dueRow);
    dueLayout->setContentsMargins(0, 0, 0, 0);
    dueLayout->addWidget(dueEnabled_);
    dueLayout->addWidget(dueEdit_, 1);
    form->addRow(QStringLiteral("标题"), titleEdit_);
    form->addRow(QStringLiteral("状态"), statusBox_);
    form->addRow(QStringLiteral("优先级"), priorityBox_);
    form->addRow(QStringLiteral("截止"), dueRow);
    right->addLayout(form);

    descriptionEdit_ = new QTextEdit(this);
    descriptionEdit_->setAcceptRichText(false);
    descriptionEdit_->setPlaceholderText(QStringLiteral("补充说明、步骤或备注…"));
    right->addWidget(descriptionEdit_, 1);

    auto *actions = new QHBoxLayout;
    messageLabel_ = new QLabel(QStringLiteral("待办保存在本机"), this);
    deleteButton_ = new QPushButton(QStringLiteral("删除"), this);
    auto *saveButton = new QPushButton(QStringLiteral("保存"), this);
    actions->addWidget(messageLabel_, 1);
    actions->addWidget(deleteButton_);
    actions->addWidget(saveButton);
    right->addLayout(actions);
    root->addLayout(right, 1);

    setStyleSheet(QStringLiteral(
        "TodoWindow { background:#171c2b; color:#edf1fa; }"
        "QListWidget,QLineEdit,QTextEdit,QComboBox,QDateTimeEdit {"
        " background:#252d42; color:#edf1fa; border:1px solid #46536f;"
        " border-radius:8px; padding:7px; }"
        "QListWidget::item { min-height:44px; padding:7px; border-radius:7px; }"
        "QListWidget::item:selected { background:#556b98; }"
        "QPushButton { min-height:34px; padding:0 14px; border-radius:8px;"
        " background:#bed7ff; color:#182039; font-weight:600; }"
        "QLabel,QCheckBox { color:#dce4f4; }"));

    connect(newButton, &QPushButton::clicked, this, [this] {
        if (confirmDiscard())
            beginNew();
    });
    connect(list_, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item || loading_)
            return;
        const qint64 id = item->data(Qt::UserRole).toLongLong();
        if (id == currentId_)
            return;
        if (confirmDiscard())
            loadItem(id);
        else
            reload(currentId_);
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

    reload();
    beginNew();
}

void TodoWindow::closeEvent(QCloseEvent *event)
{
    confirmDiscard() ? event->accept() : event->ignore();
}

void TodoWindow::reload(qint64 selectId)
{
    loading_ = true;
    list_->clear();
    const auto result = repository_->list();
    if (!result.success) {
        setStatus(result.error, true);
        loading_ = false;
        return;
    }
    for (const TodoItem &todo : result.value) {
        auto *item = new QListWidgetItem(itemCaption(todo), list_);
        item->setData(Qt::UserRole, todo.id);
        if (todo.status == TodoStatus::Done)
            item->setForeground(QColor(QStringLiteral("#8792a8")));
        if (todo.id == selectId)
            list_->setCurrentItem(item);
    }
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
    setStatus(QStringLiteral("尚未保存"));
    loading_ = false;
    dirty_ = false;
    titleEdit_->setFocus();
}

void TodoWindow::loadItem(qint64 id)
{
    const auto result = repository_->findById(id);
    if (!result.success || !result.value.has_value()) {
        setStatus(result.success ? QStringLiteral("待办已经不存在") : result.error, true);
        reload();
        beginNew();
        return;
    }
    const TodoItem &todo = *result.value;
    loading_ = true;
    currentId_ = todo.id;
    titleEdit_->setText(todo.title);
    descriptionEdit_->setPlainText(todo.description);
    statusBox_->setCurrentIndex(statusBox_->findData(static_cast<int>(todo.status)));
    priorityBox_->setCurrentIndex(priorityBox_->findData(static_cast<int>(todo.priority)));
    dueEnabled_->setChecked(todo.dueAt.isValid());
    if (todo.dueAt.isValid())
        dueEdit_->setDateTime(todo.dueAt.toLocalTime());
    deleteButton_->setEnabled(true);
    loading_ = false;
    dirty_ = false;
    setStatus(QStringLiteral("上次更新 %1")
                  .arg(todo.updatedAt.toLocalTime().toString(QStringLiteral("MM-dd HH:mm"))));
    reload(todo.id);
}

void TodoWindow::saveCurrent()
{
    const QString title = titleEdit_->text().trimmed();
    if (title.isEmpty()) {
        setStatus(QStringLiteral("待办标题不能为空"), true);
        titleEdit_->setFocus();
        return;
    }
    const TodoStatus status = static_cast<TodoStatus>(statusBox_->currentData().toInt());
    const TodoPriority priority = static_cast<TodoPriority>(priorityBox_->currentData().toInt());
    const QDateTime dueAt = dueEnabled_->isChecked() ? dueEdit_->dateTime() : QDateTime();
    if (currentId_ < 0) {
        const auto result = repository_->create(title, descriptionEdit_->toPlainText(), priority,
                                                dueAt, status);
        if (!result.success) {
            setStatus(result.error, true);
            return;
        }
        currentId_ = result.value.id;
    } else {
        TodoItem item;
        item.id = currentId_;
        item.title = title;
        item.description = descriptionEdit_->toPlainText();
        item.status = status;
        item.priority = priority;
        item.dueAt = dueAt;
        const auto result = repository_->update(item);
        if (!result.success || !result.value) {
            setStatus(result.success ? QStringLiteral("待办已经不存在") : result.error, true);
            return;
        }
    }
    dirty_ = false;
    loadItem(currentId_);
    setStatus(QStringLiteral("已保存到本机"));
}

void TodoWindow::removeCurrent()
{
    if (currentId_ < 0)
        return;
    if (QMessageBox::question(this, QStringLiteral("删除待办"),
                              QStringLiteral("确定删除当前待办吗？")) != QMessageBox::Yes) {
        return;
    }
    const auto result = repository_->remove(currentId_);
    if (!result.success || !result.value) {
        setStatus(result.success ? QStringLiteral("待办已经不存在") : result.error, true);
        return;
    }
    dirty_ = false;
    reload();
    beginNew();
    setStatus(QStringLiteral("待办已删除"));
}

bool TodoWindow::confirmDiscard()
{
    if (!dirty_)
        return true;
    const auto answer = QMessageBox::question(
        this, QStringLiteral("放弃修改"), QStringLiteral("当前待办尚未保存，确定放弃修改吗？"),
        QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
    return answer == QMessageBox::Discard;
}

void TodoWindow::setStatus(const QString &message, bool error)
{
    messageLabel_->setText(message);
    messageLabel_->setStyleSheet(error ? QStringLiteral("color:#f0a4b3;") : QString());
}

void TodoWindow::markDirty()
{
    if (loading_)
        return;
    dirty_ = true;
    setStatus(QStringLiteral("有尚未保存的修改"));
}
