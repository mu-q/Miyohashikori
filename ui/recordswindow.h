#pragma once

#include <QWidget>
#include <QString>

class JournalRepository;
class NoteRepository;
class QCloseEvent;
class QDateEdit;
class QEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTextEdit;
class QToolButton;

class RecordsWindow : public QWidget
{
    Q_OBJECT

public:
    explicit RecordsWindow(JournalRepository *journalRepository,
                           NoteRepository *noteRepository,
                           QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    enum class Mode { Journal, Notes };

    void switchMode(Mode mode);
    void reloadList(qint64 selectId = -1);
    void beginNewRecord();
    void loadJournal(qint64 id);
    void loadNote(qint64 id);
    void recordClicked(QListWidgetItem *item);
    bool saveCurrent();
    void removeCurrent();
    bool confirmDiscardOrSave();
    void selectRecord(qint64 id);
    void setEditingEnabled(bool enabled);
    void setStatus(const QString &message, bool error = false);
    void markDirty();
    void applyTheme();

    JournalRepository *journalRepository_ = nullptr;
    NoteRepository *noteRepository_ = nullptr;
    Mode mode_ = Mode::Journal;
    qint64 currentId_ = -1;
    bool dirty_ = false;
    bool loading_ = false;
    bool dataAvailable_ = true;
    bool dragging_ = false;
    QPoint dragOffset_;

    QWidget *titleBar_ = nullptr;
    QToolButton *journalModeButton_ = nullptr;
    QToolButton *notesModeButton_ = nullptr;
    QPushButton *newButton_ = nullptr;
    QListWidget *recordList_ = nullptr;
    QLabel *sectionLabel_ = nullptr;
    QDateEdit *dateEdit_ = nullptr;
    QLineEdit *titleEdit_ = nullptr;
    QLineEdit *tagsEdit_ = nullptr;
    QTextEdit *bodyEdit_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QPushButton *deleteButton_ = nullptr;
    QPushButton *saveButton_ = nullptr;
    QString nightStyle_;
};
