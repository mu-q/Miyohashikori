QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

DEFINES += HYORI_SOURCE_DIR=\\\"$$replace($$PWD, \\\\, /)\\\"

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    core/apppaths.cpp \
    core/config/appconfig.cpp \
    core/config/configmanager.cpp \
    core/spritecatalog.cpp \
    core/ai/chathistory.cpp \
    core/ai/emotionparser.cpp \
    core/ai/nullaisession.cpp \
    core/ai/openaichatsession.cpp \
    ui/characterspriteview.cpp \
    ui/replybubble.cpp

RESOURCES += \
    hyori_assets.qrc

HEADERS += \
    mainwindow.h \
    core/apppaths.h \
    core/config/appconfig.h \
    core/config/configmanager.h \
    core/spritecatalog.h \
    core/ai/chathistory.h \
    core/ai/emotionparser.h \
    core/ai/iaisession.h \
    core/ai/nullaisession.h \
    core/ai/openaichatsession.h \
    ui/characterspriteview.h \
    ui/replybubble.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
