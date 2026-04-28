#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication a(argc, argv);
    a.setApplicationName(QStringLiteral("Miyohashikori"));
    a.setOrganizationName(QStringLiteral("Miyohashikori"));

    MainWindow w;
    w.show();
    return a.exec();
}
