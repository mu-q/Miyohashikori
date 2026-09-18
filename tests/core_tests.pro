QT += core network testlib
QT -= gui

CONFIG += console testcase c++17
CONFIG -= app_bundle

TARGET = core_tests
TEMPLATE = app

INCLUDEPATH += ..

SOURCES += \
    test_core.cpp \
    ../core/apppaths.cpp \
    ../core/config/appconfig.cpp \
    ../core/config/configmanager.cpp \
    ../core/pomodorocontroller.cpp \
    ../core/conversationlog.cpp \
    ../core/ai/chathistory.cpp \
    ../core/ai/emotionparser.cpp \
    ../core/ai/openaichatsession.cpp

HEADERS += \
    ../core/apppaths.h \
    ../core/config/appconfig.h \
    ../core/config/configmanager.h \
    ../core/pomodorocontroller.h \
    ../core/conversationlog.h \
    ../core/ai/chathistory.h \
    ../core/ai/emotionparser.h \
    ../core/ai/iaisession.h \
    ../core/ai/openaichatsession.h
