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

class TodoOverlayTests : public QObject
{
    Q_OBJECT

private slots:
    void savesAndClosesInsideHostWindow();
};

void TodoOverlayTests::savesAndClosesInsideHostWindow()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DatabaseManager manager(directory.filePath(QStringLiteral("todo-ui.db")));
    QVERIFY2(manager.initialize(), qPrintable(manager.errorString()));
    TodoRepository repository(&manager);

    QWidget focusHost;
    focusHost.resize(1280, 780);
    TodoWindow overlay(&repository, &focusHost);
    overlay.setGeometry(focusHost.rect());
    focusHost.show();
    overlay.present();
    QVERIFY(QTest::qWaitForWindowExposed(&focusHost));

    QCOMPARE(overlay.parentWidget(), &focusHost);
    QVERIFY(overlay.isVisible());
    QCOMPARE(overlay.geometry(), focusHost.rect());

    ThemeManager::instance()->apply(QStringLiteral("mist"));
    QVERIFY(overlay.styleSheet().contains(QStringLiteral("rgba(244, 248, 255, 214)")));
    ThemeManager::instance()->apply(QStringLiteral("night"));
    QVERIFY(!overlay.styleSheet().contains(QStringLiteral("rgba(244, 248, 255, 214)")));

    auto *title = overlay.findChild<QLineEdit *>(QStringLiteral("todoTitleEdit"));
    auto *save = overlay.findChild<QPushButton *>(QStringLiteral("saveTodo"));
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
    QTRY_VERIFY(!overlay.isVisible());
    QVERIFY(focusHost.isVisible());
}

QTEST_MAIN(TodoOverlayTests)

#include "test_todo_overlay.moc"
