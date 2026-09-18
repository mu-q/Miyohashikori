#pragma once

#include <QWidget>

class TodoRepository;
class QCheckBox;
class QCloseEvent;
class QComboBox;
class QDateTimeEdit;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTextEdit;

class TodoWindow : public QWidget
{
    Q_OBJECT

public:
    explicit TodoWindow(TodoRepository *repository, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void reload(qint64 selectId = -1);
    void beginNew();
    void loadItem(qint64 id);
    void saveCurrent();
    void removeCurrent();
    bool confirmDiscard();
    void setStatus(const QString &message, bool error = false);
    void markDirty();

    TodoRepository *repository_ = nullptr;
    qint64 currentId_ = -1;
    bool dirty_ = false;
    bool loading_ = false;
    QListWidget *list_ = nullptr;
    QLineEdit *titleEdit_ = nullptr;
    QTextEdit *descriptionEdit_ = nullptr;
    QComboBox *statusBox_ = nullptr;
    QComboBox *priorityBox_ = nullptr;
    QCheckBox *dueEnabled_ = nullptr;
    QDateTimeEdit *dueEdit_ = nullptr;
    QLabel *messageLabel_ = nullptr;
    QPushButton *deleteButton_ = nullptr;
};
