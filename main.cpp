#include "mainwindow.h"
#include "core/apppaths.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QMessageBox>
#include <QMutex>
#include <QTextStream>

namespace {

void writeLogMessage(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    static QMutex mutex;
    const QMutexLocker locker(&mutex);
    QFile file(QDir(AppPaths::logsRoot()).filePath(QStringLiteral("application.log")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    QString level;
    switch (type) {
    case QtDebugMsg: level = QStringLiteral("DEBUG"); break;
    case QtInfoMsg: level = QStringLiteral("INFO"); break;
    case QtWarningMsg: level = QStringLiteral("WARN"); break;
    case QtCriticalMsg: level = QStringLiteral("ERROR"); break;
    case QtFatalMsg: level = QStringLiteral("FATAL"); break;
    }
    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << " [" << level
           << "] " << message << '\n';
}

void initializeLogging()
{
    QDir logDirectory(AppPaths::logsRoot());
    if (!logDirectory.exists() && !logDirectory.mkpath(QStringLiteral(".")))
        return;
    const QString current = logDirectory.filePath(QStringLiteral("application.log"));
    if (QFileInfo(current).size() > 2 * 1024 * 1024) {
        const QString previous = logDirectory.filePath(QStringLiteral("application.previous.log"));
        QFile::remove(previous);
        QFile::rename(current, previous);
    }
    qInstallMessageHandler(writeLogMessage);
}

} // namespace

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication a(argc, argv);
    a.setApplicationName(QStringLiteral("Miyohashikori"));
    a.setOrganizationName(QStringLiteral("Miyohashikori"));
    a.setWindowIcon(QIcon(QStringLiteral(":/resources/icons/hyori_chibi.png")));

    QDir dataDirectory(AppPaths::appDataRoot());
    if (!dataDirectory.exists())
        dataDirectory.mkpath(QStringLiteral("."));
    QLockFile instanceLock(dataDirectory.filePath(QStringLiteral("Miyohashikori.lock")));
    if (!instanceLock.tryLock(100)) {
        QMessageBox::information(nullptr, QStringLiteral("冰织已经在运行"),
                                 QStringLiteral("桌面上已经有一个冰织实例了。"));
        return 0;
    }
    initializeLogging();

    MainWindow w;
    w.show();
    return a.exec();
}
