# -------------------------------------------------------------------
# Project file for the QtTestBrowser binary
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

include(../BaseClient/baseclient.pri)

SOURCES += \
    main.cpp \
    replayscheduler.cpp \
    network.cpp

HEADERS += \
    replayscheduler.h \
    network.h

