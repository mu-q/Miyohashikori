QT += core sql testlib
QT -= gui

CONFIG += console testcase c++17
CONFIG -= app_bundle

TARGET = database_tests
TEMPLATE = app

INCLUDEPATH += ..

SOURCES += \
    test_database.cpp \
    ../core/apppaths.cpp \
    ../core/data/databasemanager.cpp \
    ../core/data/journalrepository.cpp \
    ../core/data/noterepository.cpp \
    ../core/data/todorepository.cpp

HEADERS += \
    ../core/apppaths.h \
    ../core/data/databasemanager.h \
    ../core/data/dataresult.h \
    ../core/data/models.h \
    ../core/data/sqlhelpers.h \
    ../core/data/journalrepository.h \
    ../core/data/noterepository.h \
    ../core/data/todorepository.h
