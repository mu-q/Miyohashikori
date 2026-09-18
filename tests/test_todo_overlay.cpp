#include "core/data/databasemanager.h"
#include "core/data/todorepository.h"
#include "core/theme.h"
#include "ui/todowindow.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QWidget>

class TodoWindowTests : public QObject
{
    Q_OBJECT

private slots:
    void savesAndClosesAsIndependentWindow();
};

void TodoWindowTests::savesAndClosesAsIndependentWindow()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DatabaseManager manager(directory.filePath(QStringLiteral("todo-ui.db")));
    QVERIFY2(manager.initialize(), qPrintable(manager.errorString()));
    TodoRepository repository(&manager);

    TodoWindow window(&repository);
    window.present();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QVERIFY(window.isWindow());
    QCOMPARE(window.parentWidget(), nullptr);
    QVERIFY(window.isVisible());

    ThemeManager::instance()->apply(QStringLiteral("mist"));
    QVERIFY(window.styleSheet().contains(QStringLiteral("rgba(244, 248, 255, 214)")));
    ThemeManager::instance()->apply(QStringLiteral("night"));
    QVERIFY(!window.styleSheet().contains(QStringLiteral("rgba(244, 248, 255, 214)")));

    auto *title = window.findChild<QLineEdit *>(QStringLiteral("todoTitleEdit"));
    auto *save = window.findChild<QPushButton *>(QStringLiteral("saveTodo"));
    QVERIFY(title);
    QVERIFY(save);
    QVERIFY(title->isEnabled());
    QVERIFY(save->isEnabled());

    QTest::keyClicks(title, QStringLiteral("focus window todo interaction"));
    QTest::mouseClick(save, Qt::LeftButton);

    const auto stored = repository.list();
    QVERIFY2(stored.success, qPrintable(stored.error));
    QCOMPARE(stored.value.size(), 1);
    QCOMPARE(stored.value.first().title, QStringLiteral("focus window todo interaction"));

    QKeyEvent escapePress(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(title, &escapePress);
    QTRY_VERIFY(!window.isVisible());
}

QTEST_MAIN(TodoWindowTests)

#include "test_todo_overlay.moc"
