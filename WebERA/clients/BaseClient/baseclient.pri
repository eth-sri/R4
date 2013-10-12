
TEMPLATE = app

OBJECTS_DIR = build
MOC_DIR = build
DESTDIR = bin
RCC_DIR = build

INCLUDEPATH += \
    ../../../WebKitBuild/Release/include/QtWebKit/ \
    ../../../Source/ \
    ../../../Source/WebKit/qt/WebCoreSupport/ \
    ../../../Source/WTF/ \
    ../../../Source/WebCore/ \
    ../BaseClient/

LIBS += \
    ../../../WebKitBuild/Release/lib/libQtWebKit.so

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
