# -------------------------------------------------------------------
# Project file for the QtTestBrowser binary
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

include(../BaseClient/baseclient.pri)

SOURCES += \
    main.cpp \
    network.cpp \
    specificationscheduler.cpp \
    datalog.cpp \
    autoexplorer.cpp

HEADERS += \
    network.h \
    specificationscheduler.h \
    datalog.h \
    autoexplorer.h

