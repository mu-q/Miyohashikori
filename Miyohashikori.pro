QT       += core gui network multimedia multimediawidgets sql
win32: QT += axcontainer

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
    core/ttsclient.cpp \
    core/voiceplayer.cpp \
    core/conversationlog.cpp \
    core/data/databasemanager.cpp \
    core/data/journalrepository.cpp \
    core/data/noterepository.cpp \
    core/data/schedulerepository.cpp \
    core/data/todorepository.cpp \
    core/schedule/courseremindercontroller.cpp \
    core/schedule/spreadsheetscheduleimporter.cpp \
    core/pomodorocontroller.cpp \
    core/ai/chathistory.cpp \
    core/ai/emotionparser.cpp \
    core/ai/nullaisession.cpp \
    core/ai/openaichatsession.cpp \
    ui/characterspriteview.cpp \
    ui/replybubble.cpp \
    ui/chatlogwindow.cpp \
    ui/coursesidebar.cpp \
    ui/focuswindow.cpp

RESOURCES += \
    hyori_assets.qrc

HEADERS += \
    mainwindow.h \
    core/apppaths.h \
    core/config/appconfig.h \
    core/config/configmanager.h \
    core/spritecatalog.h \
    core/ttsclient.h \
    core/voiceplayer.h \
    core/conversationlog.h \
    core/data/databasemanager.h \
    core/data/dataresult.h \
    core/data/models.h \
    core/data/sqlhelpers.h \
    core/data/journalrepository.h \
    core/data/noterepository.h \
    core/data/schedulerepository.h \
    core/data/todorepository.h \
    core/schedule/courseremindercontroller.h \
    core/schedule/spreadsheetscheduleimporter.h \
    core/pomodorocontroller.h \
    core/ai/chathistory.h \
    core/ai/emotionparser.h \
    core/ai/iaisession.h \
    core/ai/nullaisession.h \
    core/ai/openaichatsession.h \
    ui/characterspriteview.h \
    ui/replybubble.h \
    ui/chatlogwindow.h \
    ui/coursesidebar.h \
    ui/focuswindow.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
