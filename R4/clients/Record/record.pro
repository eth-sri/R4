# -------------------------------------------------------------------
# Project file for the QtTestBrowser binary
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

include(../BaseClient/baseclient.pri)

SOURCES += \
    main.cpp \
    specificationscheduler.cpp \
    datalog.cpp \
    autoexplorer.cpp

HEADERS += \
    specificationscheduler.h \
    autoexplorer.h \
    datalog.h

