
TEMPLATE = app

OBJECTS_DIR = build
MOC_DIR = build
DESTDIR = bin
RCC_DIR = build

INCLUDEPATH += \
    ${WebERA_DIR}/WebKitBuild/Release/include/QtWebKit/ \
    ${WebERA_DIR}/Source/ \
    ${WebERA_DIR}/Source/WebKit/qt/WebCoreSupport/ \
    ${WebERA_DIR}/Source/WTF/ \
    ${WebERA_DIR}/Source/WebCore/ \
    ../BaseClient/

LIBS += \
    ${WebERA_DIR}/WebKitBuild/Release/lib/libQtWebKit.so

QT += network

SOURCES += \
    ../BaseClient/locationedit.cpp \
    ../BaseClient/toolwindow.cpp \
    ../BaseClient/basewindow.cpp \
    ../BaseClient/utils.cpp \
    ../BaseClient/clientapplication.cpp

HEADERS += \
    ../BaseClient/locationedit.h \
    ../BaseClient/toolwindow.h \
    ../BaseClient/basewindow.h \
    ../BaseClient/utils.h \
    ../BaseClient/clientapplication.h

RESOURCES += \
    ../BaseClient/baseclient.qrc

QMAKE_CXXFLAGS += -std=c++11
