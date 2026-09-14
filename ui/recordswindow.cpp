#include "recordswindow.h"

#include "../core/data/journalrepository.h"
#include "../core/data/noterepository.h"

#include <QButtonGroup>
#include <QCloseEvent>
#include <QDateEdit>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QString journalCaption(const JournalEntry &entry)
{
    const QString title = entry.title.trimmed().isEmpty() ? QStringLiteral("未命名日记")
                                                           : entry.title.trimmed();
    return QStringLiteral("%1\n%2")
        .arg(entry.entryDate.toString(QStringLiteral("MM 月 dd 日  dddd")), title);
}

QString noteCaption(const Note &note)
{
    const QString title = note.title.trimmed().isEmpty() ? QStringLiteral("未命名笔记")
                                                          : note.title.trimmed();
    const QString detail = note.tags.isEmpty()
        ? note.updatedAt.toLocalTime().toString(QStringLiteral("MM-dd  HH:mm"))
        : QStringLiteral("# %1").arg(note.tags.join(QStringLiteral("  # ")));
    return QStringLiteral("%1\n%2").arg(title, detail);
}

} // namespace

RecordsWindow::RecordsWindow(JournalRepository *journalRepository,
                             NoteRepository *noteRepository, QWidget *parent)
    : QWidget(parent)
    , journalRepository_(journalRepository)
    , noteRepository_(noteRepository)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowTitle(QStringLiteral("冰织手账 · 日记与笔记"));
    setWindowIcon(QIcon(QStringLiteral(":/resources/icons/hyori_chibi.png")));
    setMinimumSize(780, 520);
    resize(940, 640);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(18, 18, 18, 18);

    auto *shell = new QFrame(this);
    shell->setObjectName(QStringLiteral("recordsShell"));
    auto *shadow = new QGraphicsDropShadowEffect(shell);
    shadow->setBlurRadius(34);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(5, 8, 20, 150));
    shell->setGraphicsEffect(shadow);
    outer->addWidget(shell);

    auto *shellLayout = new QVBoxLayout(shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    titleBar_ = new QWidget(shell);
    titleBar_->setObjectName(QStringLiteral("recordsTitleBar"));
    titleBar_->setFixedHeight(58);
    titleBar_->installEventFilter(this);
    auto *titleLayout = new QHBoxLayout(titleBar_);
    titleLayout->setContentsMargins(24, 0, 14, 0);
    auto *title = new QLabel(QStringLiteral("冰织手账"), titleBar_);
    title->setObjectName(QStringLiteral("recordsWindowTitle"));
    auto *subtitle = new QLabel(QStringLiteral("把今天留在这里"), titleBar_);
    subtitle->setObjectName(QStringLiteral("recordsWindowSubtitle"));
    auto *closeButton = new QToolButton(titleBar_);
    closeButton->setObjectName(QStringLiteral("recordsCloseButton"));
    closeButton->setText(QStringLiteral("×"));
    closeButton->setToolTip(QStringLiteral("关闭手账"));
    closeButton->setFixedSize(34, 34);
    titleLayout->addWidget(title);
    titleLayout->addSpacing(12);
    titleLayout->addWidget(subtitle);
    titleLayout->addStretch();
    titleLayout->addWidget(closeButton);
    shellLayout->addWidget(titleBar_);

    auto *content = new QHBoxLayout;
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(0);
    shellLayout->addLayout(content, 1);

    auto *rail = new QFrame(shell);
    rail->setObjectName(QStringLiteral("recordsRail"));
    rail->setFixedWidth(268);
    auto *railLayout = new QVBoxLayout(rail);
    railLayout->setContentsMargins(18, 20, 18, 22);
    railLayout->setSpacing(14);

    auto *modeRow = new QHBoxLayout;
    modeRow->setSpacing(6);
    journalModeButton_ = new QToolButton(rail);
    notesModeButton_ = new QToolButton(rail);
    journalModeButton_->setText(QStringLiteral("日记"));
    notesModeButton_->setText(QStringLiteral("笔记"));
    for (QToolButton *button : {journalModeButton_, notesModeButton_}) {
        button->setObjectName(QStringLiteral("recordsModeButton"));
        button->setCheckable(true);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setFixedHeight(38);
        modeRow->addWidget(button);
    }
    auto *modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(true);
    modeGroup->addButton(journalModeButton_);
    modeGroup->addButton(notesModeButton_);
    journalModeButton_->setChecked(true);
    railLayout->addLayout(modeRow);

    newButton_ = new QPushButton(QStringLiteral("＋  写今天的日记"), rail);
    newButton_->setObjectName(QStringLiteral("recordsNewButton"));
    newButton_->setToolTip(QStringLiteral("新建一条记录"));
    newButton_->setFixedHeight(42);
    railLayout->addWidget(newButton_);

    auto *historyLabel = new QLabel(QStringLiteral("过往记录"), rail);
    historyLabel->setObjectName(QStringLiteral("recordsRailCaption"));
    railLayout->addWidget(historyLabel);

    recordList_ = new QListWidget(rail);
    recordList_->setObjectName(QStringLiteral("recordsList"));
    recordList_->setSpacing(5);
    recordList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    railLayout->addWidget(recordList_, 1);
    content->addWidget(rail);

    auto *editor = new QFrame(shell);
    editor->setObjectName(QStringLiteral("recordsEditor"));
    auto *editorLayout = new QVBoxLayout(editor);
    editorLayout->setContentsMargins(34, 24, 34, 28);
    editorLayout->setSpacing(12);

    sectionLabel_ = new QLabel(QStringLiteral("今日 · 日记"), editor);
    sectionLabel_->setObjectName(QStringLiteral("recordsSectionLabel"));
    editorLayout->addWidget(sectionLabel_);

    dateEdit_ = new QDateEdit(QDate::currentDate(), editor);
    dateEdit_->setObjectName(QStringLiteral("recordsDateEdit"));
    dateEdit_->setCalendarPopup(true);
    dateEdit_->setDisplayFormat(QStringLiteral("yyyy 年 MM 月 dd 日  dddd"));
    editorLayout->addWidget(dateEdit_);

    titleEdit_ = new QLineEdit(editor);
    titleEdit_->setObjectName(QStringLiteral("recordsTitleEdit"));
    titleEdit_->setPlaceholderText(QStringLiteral("给这段记忆一个标题…"));
    editorLayout->addWidget(titleEdit_);

    tagsEdit_ = new QLineEdit(editor);
    tagsEdit_->setObjectName(QStringLiteral("recordsTagsEdit"));
    tagsEdit_->setPlaceholderText(QStringLiteral("标签，用逗号分隔，例如：灵感, 项目"));
    tagsEdit_->hide();
    editorLayout->addWidget(tagsEdit_);

    bodyEdit_ = new QTextEdit(editor);
    bodyEdit_->setObjectName(QStringLiteral("recordsBodyEdit"));
    bodyEdit_->setPlaceholderText(QStringLiteral("今天发生了什么？慢慢写下来吧…"));
    bodyEdit_->setAcceptRichText(false);
    editorLayout->addWidget(bodyEdit_, 1);

    auto *actions = new QHBoxLayout;
    statusLabel_ = new QLabel(QStringLiteral("内容保存在本机"), editor);
    statusLabel_->setObjectName(QStringLiteral("recordsStatus"));
    deleteButton_ = new QPushButton(QStringLiteral("删除"), editor);
    deleteButton_->setObjectName(QStringLiteral("recordsDeleteButton"));
    saveButton_ = new QPushButton(QStringLiteral("保存日记"), editor);
    saveButton_->setObjectName(QStringLiteral("recordsSaveButton"));
    deleteButton_->setToolTip(QStringLiteral("删除当前记录"));
    saveButton_->setToolTip(QStringLiteral("保存当前记录"));
    actions->addWidget(statusLabel_, 1);
    actions->addWidget(deleteButton_);
    actions->addWidget(saveButton_);
    editorLayout->addLayout(actions);
    content->addWidget(editor, 1);

    setStyleSheet(QStringLiteral(R"(
        #recordsShell {
            background: rgba(22, 27, 43, 242);
            border: 1px solid rgba(184, 205, 238, 88);
            border-radius: 22px;
        }
        #recordsTitleBar {
            background: rgba(32, 39, 61, 225);
            border-top-left-radius: 22px;
            border-top-right-radius: 22px;
            border-bottom: 1px solid rgba(185, 207, 239, 40);
        }
        #recordsWindowTitle { color: #f6f7ff; font: 700 19px "Microsoft YaHei UI"; }
        #recordsWindowSubtitle { color: #8f9ab8; font-size: 12px; }
        #recordsCloseButton {
            color: #aeb8cf; background: transparent; border: none;
            border-radius: 17px; font-size: 24px;
        }
        #recordsCloseButton:hover { color: white; background: rgba(235, 128, 151, 95); }
        #recordsRail {
            background: rgba(29, 35, 55, 205);
            border-bottom-left-radius: 22px;
            border-right: 1px solid rgba(177, 201, 234, 42);
        }
        #recordsModeButton {
            color: #9ba8c5; background: transparent; border: none;
            border-radius: 11px; font: 600 13px "Microsoft YaHei UI";
        }
        #recordsModeButton:hover { color: #eff4ff; background: rgba(139, 165, 215, 35); }
        #recordsModeButton:checked {
            color: #192039; background: #bed7ff;
        }
        #recordsNewButton {
            color: #f7f3ff; background: rgba(119, 105, 166, 180);
            border: 1px solid rgba(211, 200, 255, 75); border-radius: 13px;
            font: 600 13px "Microsoft YaHei UI";
        }
        #recordsNewButton:hover { background: rgba(142, 124, 195, 230); border-color: #d7ccff; }
        #recordsNewButton:pressed { background: rgba(94, 82, 137, 240); }
        #recordsRailCaption {
            color: #7f8ba9; font: 600 11px "Microsoft YaHei UI";
            letter-spacing: 1px; padding-left: 5px;
        }
        #recordsList {
            color: #ced7e9; background: transparent; border: none; outline: none;
        }
        #recordsList::item {
            min-height: 48px; padding: 8px 10px 8px 14px;
            border-radius: 10px; border-left: 3px solid transparent;
        }
        #recordsList::item:hover { background: rgba(125, 151, 196, 32); color: white; }
        #recordsList::item:selected {
            color: #f9fbff; background: rgba(115, 140, 187, 68);
            border-left: 3px solid #bca9ef;
        }
        #recordsEditor { background: rgba(17, 22, 36, 160); border-bottom-right-radius: 22px; }
        #recordsSectionLabel { color: #93a4c8; font: 600 12px "Microsoft YaHei UI"; }
        #recordsDateEdit, #recordsTitleEdit, #recordsTagsEdit {
            color: #eef2fb; background: rgba(52, 61, 84, 150);
            border: 1px solid rgba(166, 190, 225, 54); border-radius: 10px;
            padding: 9px 12px; min-height: 20px;
        }
        #recordsDateEdit:hover, #recordsTitleEdit:hover, #recordsTagsEdit:hover,
        #recordsDateEdit:focus, #recordsTitleEdit:focus, #recordsTagsEdit:focus {
            border-color: rgba(190, 215, 255, 145);
        }
        #recordsBodyEdit {
            color: #e8edf8; background: rgba(39, 47, 67, 105);
            border: 1px solid rgba(166, 190, 225, 43); border-radius: 14px;
            padding: 16px; selection-background-color: #7188b7;
            font: 14px "Microsoft YaHei UI";
        }
        #recordsBodyEdit:focus { border-color: rgba(190, 215, 255, 115); }
        #recordsStatus { color: #7f8ca8; font-size: 11px; }
        #recordsDeleteButton, #recordsSaveButton {
            min-width: 76px; min-height: 36px; border-radius: 11px;
            padding: 0 15px; font-weight: 600;
        }
        #recordsDeleteButton {
            color: #c7cfde; background: transparent;
            border: 1px solid rgba(179, 191, 214, 55);
        }
        #recordsDeleteButton:hover { color: #ffdce3; border-color: rgba(242, 146, 165, 150); }
        #recordsDeleteButton:disabled { color: #596175; border-color: rgba(100, 110, 130, 35); }
        #recordsSaveButton { color: #192039; background: #bed7ff; border: 1px solid #dceaff; }
        #recordsSaveButton:hover { background: #d1e3ff; }
    )"));

    connect(closeButton, &QToolButton::clicked, this, &QWidget::close);
    connect(journalModeButton_, &QToolButton::clicked, this,
            [this] { switchMode(Mode::Journal); });
    connect(notesModeButton_, &QToolButton::clicked, this,
            [this] { switchMode(Mode::Notes); });
    connect(newButton_, &QPushButton::clicked, this, [this] {
        if (confirmDiscardOrSave())
            beginNewRecord();
    });
    connect(recordList_, &QListWidget::itemClicked, this, &RecordsWindow::recordClicked);
    connect(saveButton_, &QPushButton::clicked, this, [this] { saveCurrent(); });
    connect(deleteButton_, &QPushButton::clicked, this, &RecordsWindow::removeCurrent);
    connect(titleEdit_, &QLineEdit::textEdited, this, [this] { markDirty(); });
    connect(tagsEdit_, &QLineEdit::textEdited, this, [this] { markDirty(); });
    connect(bodyEdit_, &QTextEdit::textChanged, this, [this] { markDirty(); });
    connect(dateEdit_, &QDateEdit::dateChanged, this, [this] { markDirty(); });

    switchMode(Mode::Journal);
}

bool RecordsWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == titleBar_) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                dragging_ = true;
                dragOffset_ = mouse->globalPosition().toPoint() - frameGeometry().topLeft();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && dragging_) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            move(mouse->globalPosition().toPoint() - dragOffset_);
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && dragging_) {
            dragging_ = false;
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void RecordsWindow::closeEvent(QCloseEvent *event)
{
    if (confirmDiscardOrSave())
        event->accept();
    else
        event->ignore();
}

void RecordsWindow::switchMode(Mode mode)
{
    if (mode_ != mode && !confirmDiscardOrSave()) {
        loading_ = true;
        journalModeButton_->setChecked(mode_ == Mode::Journal);
        notesModeButton_->setChecked(mode_ == Mode::Notes);
        loading_ = false;
        return;
    }

    mode_ = mode;
    loading_ = true;
    journalModeButton_->setChecked(mode_ == Mode::Journal);
    notesModeButton_->setChecked(mode_ == Mode::Notes);
    dateEdit_->setVisible(mode_ == Mode::Journal);
    tagsEdit_->setVisible(mode_ == Mode::Notes);
    newButton_->setText(mode_ == Mode::Journal ? QStringLiteral("＋  写今天的日记")
                                               : QStringLiteral("＋  新建笔记"));
    saveButton_->setText(mode_ == Mode::Journal ? QStringLiteral("保存日记")
                                                : QStringLiteral("保存笔记"));
    bodyEdit_->setPlaceholderText(mode_ == Mode::Journal
        ? QStringLiteral("今天发生了什么？慢慢写下来吧…")
        : QStringLiteral("记下灵感、资料，或任何不想忘记的事情…"));
    loading_ = false;
    currentId_ = -1;
    dirty_ = false;
    reloadList();
    beginNewRecord();
}

void RecordsWindow::reloadList(qint64 selectId)
{
    loading_ = true;
    dataAvailable_ = true;
    recordList_->clear();

    if (mode_ == Mode::Journal) {
        const auto result = journalRepository_->list(QDate(1900, 1, 1), QDate(2999, 12, 31));
        if (!result.success) {
            dataAvailable_ = false;
            setStatus(result.error, true);
            setEditingEnabled(false);
        } else {
            for (const JournalEntry &entry : result.value) {
                auto *item = new QListWidgetItem(journalCaption(entry), recordList_);
                item->setData(Qt::UserRole, entry.id);
                item->setToolTip(entry.entryDate.toString(Qt::ISODate));
            }
        }
    } else {
        const auto result = noteRepository_->list(0, 1000);
        if (!result.success) {
            dataAvailable_ = false;
            setStatus(result.error, true);
            setEditingEnabled(false);
        } else {
            for (const Note &note : result.value) {
                auto *item = new QListWidgetItem(noteCaption(note), recordList_);
                item->setData(Qt::UserRole, note.id);
                item->setToolTip(note.body.left(160));
            }
        }
    }
    loading_ = false;
    if (selectId >= 0)
        selectRecord(selectId);
}

void RecordsWindow::beginNewRecord()
{
    loading_ = true;
    recordList_->clearSelection();
    currentId_ = -1;
    titleEdit_->clear();
    bodyEdit_->clear();
    tagsEdit_->clear();
    dateEdit_->setDate(QDate::currentDate());
    dateEdit_->setEnabled(true);
    deleteButton_->setEnabled(false);
    if (!dataAvailable_) {
        setEditingEnabled(false);
        loading_ = false;
        dirty_ = false;
        return;
    }
    setEditingEnabled(true);
    sectionLabel_->setText(mode_ == Mode::Journal ? QStringLiteral("今天 · 新日记")
                                                  : QStringLiteral("灵感 · 新笔记"));
    setStatus(QStringLiteral("尚未保存"));
    loading_ = false;
    dirty_ = false;

    if (mode_ == Mode::Journal) {
        const auto result = journalRepository_->findByDate(QDate::currentDate());
        if (result.success && result.value.has_value()) {
            loadJournal(result.value->id);
            setStatus(QStringLiteral("今天的日记已经在这里了"));
        }
    }
    titleEdit_->setFocus();
}

void RecordsWindow::loadJournal(qint64 id)
{
    const auto all = journalRepository_->list(QDate(1900, 1, 1), QDate(2999, 12, 31));
    if (!all.success) {
        setStatus(all.error, true);
        return;
    }
    for (const JournalEntry &entry : all.value) {
        if (entry.id != id)
            continue;
        loading_ = true;
        currentId_ = entry.id;
        dateEdit_->setDate(entry.entryDate);
        dateEdit_->setEnabled(false);
        titleEdit_->setText(entry.title);
        bodyEdit_->setPlainText(entry.body);
        deleteButton_->setEnabled(true);
        sectionLabel_->setText(entry.entryDate == QDate::currentDate()
            ? QStringLiteral("今天 · 日记")
            : entry.entryDate.toString(QStringLiteral("yyyy 年 MM 月 dd 日 · 日记")));
        setStatus(QStringLiteral("上次更新 %1")
                      .arg(entry.updatedAt.toLocalTime().toString(QStringLiteral("MM-dd HH:mm"))));
        loading_ = false;
        dirty_ = false;
        selectRecord(id);
        return;
    }
    setStatus(QStringLiteral("这篇日记已经不存在"), true);
    reloadList();
    beginNewRecord();
}

void RecordsWindow::loadNote(qint64 id)
{
    const auto result = noteRepository_->findById(id);
    if (!result.success) {
        setStatus(result.error, true);
        return;
    }
    if (!result.value.has_value()) {
        setStatus(QStringLiteral("这条笔记已经不存在"), true);
        reloadList();
        beginNewRecord();
        return;
    }
    const Note &note = *result.value;
    loading_ = true;
    currentId_ = note.id;
    titleEdit_->setText(note.title);
    bodyEdit_->setPlainText(note.body);
    tagsEdit_->setText(note.tags.join(QStringLiteral(", ")));
    deleteButton_->setEnabled(true);
    sectionLabel_->setText(QStringLiteral("收藏 · 笔记"));
    setStatus(QStringLiteral("上次更新 %1")
                  .arg(note.updatedAt.toLocalTime().toString(QStringLiteral("MM-dd HH:mm"))));
    loading_ = false;
    dirty_ = false;
    selectRecord(id);
}

void RecordsWindow::recordClicked(QListWidgetItem *item)
{
    if (!item || loading_)
        return;
    const qint64 id = item->data(Qt::UserRole).toLongLong();
    if (id == currentId_)
        return;
    if (!confirmDiscardOrSave()) {
        selectRecord(currentId_);
        return;
    }
    if (mode_ == Mode::Journal)
        loadJournal(id);
    else
        loadNote(id);
}

bool RecordsWindow::saveCurrent()
{
    const QString title = titleEdit_->text().trimmed();
    const QString body = bodyEdit_->toPlainText();

    if (mode_ == Mode::Journal) {
        if (!dateEdit_->date().isValid()) {
            setStatus(QStringLiteral("请选择有效日期"), true);
            return false;
        }
        if (currentId_ < 0) {
            const auto result = journalRepository_->create(dateEdit_->date(), title, body);
            if (!result.success) {
                setStatus(result.error, true);
                return false;
            }
            currentId_ = result.value.id;
        } else {
            const auto result = journalRepository_->update(currentId_, title, body);
            if (!result.success || !result.value) {
                setStatus(result.success ? QStringLiteral("日记已经不存在") : result.error, true);
                return false;
            }
        }
    } else {
        const QStringList tags = tagsEdit_->text().split(QRegularExpression(QStringLiteral("[,，]")),
                                                        Qt::SkipEmptyParts);
        if (currentId_ < 0) {
            const auto result = noteRepository_->create(title, body, tags);
            if (!result.success) {
                setStatus(result.error, true);
                return false;
            }
            currentId_ = result.value.id;
        } else {
            const auto result = noteRepository_->update(currentId_, title, body, tags);
            if (!result.success || !result.value) {
                setStatus(result.success ? QStringLiteral("笔记已经不存在") : result.error, true);
                return false;
            }
        }
    }

    dirty_ = false;
    const qint64 savedId = currentId_;
    reloadList(savedId);
    if (mode_ == Mode::Journal)
        loadJournal(savedId);
    else
        loadNote(savedId);
    setStatus(QStringLiteral("已保存到本机"));
    return true;
}

void RecordsWindow::removeCurrent()
{
    if (currentId_ < 0)
        return;
    const QString kind = mode_ == Mode::Journal ? QStringLiteral("日记") : QStringLiteral("笔记");
    QMessageBox message(QMessageBox::Question, QStringLiteral("删除%1").arg(kind),
                        QStringLiteral("确定删除当前%1吗？删除后无法恢复。").arg(kind),
                        QMessageBox::NoButton, this);
    QPushButton *deleteAction = message.addButton(QStringLiteral("删除"), QMessageBox::DestructiveRole);
    message.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    message.exec();
    if (message.clickedButton() != deleteAction)
        return;

    const auto result = mode_ == Mode::Journal ? journalRepository_->remove(currentId_)
                                                : noteRepository_->remove(currentId_);
    if (!result.success || !result.value) {
        setStatus(result.success ? QStringLiteral("记录已经不存在") : result.error, true);
        return;
    }
    dirty_ = false;
    currentId_ = -1;
    reloadList();
    beginNewRecord();
    setStatus(QStringLiteral("已删除%1").arg(kind));
}

bool RecordsWindow::confirmDiscardOrSave()
{
    if (!dirty_)
        return true;
    QMessageBox message(QMessageBox::Question, QStringLiteral("保存修改"),
                        QStringLiteral("当前内容还没有保存。"), QMessageBox::NoButton, this);
    QPushButton *saveAction = message.addButton(QStringLiteral("保存"), QMessageBox::AcceptRole);
    QPushButton *discardAction = message.addButton(QStringLiteral("不保存"), QMessageBox::DestructiveRole);
    message.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    message.exec();
    if (message.clickedButton() == saveAction)
        return saveCurrent();
    return message.clickedButton() == discardAction;
}

void RecordsWindow::selectRecord(qint64 id)
{
    loading_ = true;
    recordList_->clearSelection();
    for (int row = 0; row < recordList_->count(); ++row) {
        QListWidgetItem *item = recordList_->item(row);
        if (item->data(Qt::UserRole).toLongLong() == id) {
            recordList_->setCurrentItem(item);
            recordList_->scrollToItem(item);
            break;
        }
    }
    loading_ = false;
}

void RecordsWindow::setEditingEnabled(bool enabled)
{
    dateEdit_->setEnabled(enabled);
    titleEdit_->setEnabled(enabled);
    tagsEdit_->setEnabled(enabled);
    bodyEdit_->setEnabled(enabled);
    saveButton_->setEnabled(enabled);
}

void RecordsWindow::setStatus(const QString &message, bool error)
{
    statusLabel_->setText(message);
    statusLabel_->setStyleSheet(error ? QStringLiteral("color:#f0a4b3;") : QString());
}

void RecordsWindow::markDirty()
{
    if (loading_)
        return;
    dirty_ = true;
    setStatus(QStringLiteral("有尚未保存的修改"));
}
