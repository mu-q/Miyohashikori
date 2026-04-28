QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    core/apppaths.cpp \
    core/spritecatalog.cpp \
    core/ai/nullaisession.cpp \
    ui/characterspriteview.cpp

HEADERS += \
    mainwindow.h \
    core/apppaths.h \
    core/spritecatalog.h \
    core/ai/iaisession.h \
    core/ai/nullaisession.h \
    ui/characterspriteview.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
