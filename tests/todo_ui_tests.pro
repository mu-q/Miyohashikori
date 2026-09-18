QT += core gui widgets sql testlib

CONFIG += console testcase c++17
CONFIG -= app_bundle

TARGET = todo_ui_tests
TEMPLATE = app

INCLUDEPATH += ..

SOURCES += \
    test_todo_overlay.cpp \
    ../core/apppaths.cpp \
    ../core/theme.cpp \
    ../core/data/databasemanager.cpp \
    ../core/data/todorepository.cpp \
    ../ui/todowindow.cpp

HEADERS += \
    ../core/apppaths.h \
    ../core/theme.h \
    ../core/data/databasemanager.h \
    ../core/data/dataresult.h \
    ../core/data/models.h \
    ../core/data/sqlhelpers.h \
    ../core/data/todorepository.h \
    ../ui/todowindow.h
