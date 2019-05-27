#-------------------------------------------------
#
# HyperCalc project file
# Peter Deak (http://hyperprog.com)
#-------------------------------------------------

QT	+= core gui
DEFINES += GSAFETEXT_LANG_ENG DCONSOLE_NO_SQL

isEqual(QT_MAJOR_VERSION, 5): QT += widgets
isEqual(QT_MAJOR_VERSION, 4): DEFINES += COMPILED_WITH_QT4X

TARGET = HyperCalc
TEMPLATE = app

SOURCES += main.cpp\
        hypercalc.cpp \
        ../gSAFE/dconsole.cpp

HEADERS  += hypercalc.h \
        ../gSAFE/dconsole.h


FORMS    +=

INCLUDEPATH += . \
               ../gSAFE/ \
               ../boost_1_69_0/
win32:OBJECTS += hypercalc.res               
RESOURCES += HyperCalc.qrc
TRANSLATIONS = hypercalc_hu.ts
