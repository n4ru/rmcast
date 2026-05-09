QT     += core qml quick quickcontrols2

TARGET = vncast
TEMPLATE = lib
CONFIG += shared plugin no_plugin_name_prefix

SOURCES += \
    src/main.cpp \
    src/launcher.cpp \
    src/settings.cpp \
    src/detect.cpp \
    src/epdc_hook.cpp \
    src/qtfb/server.cpp \
    src/qtfb/frame_view.cpp \
    xovi.cpp
HEADERS += \
    src/launcher.h \
    src/settings.h \
    src/detect.h \
    src/epdc_hook.h \
    src/qtfb/server.h \
    src/qtfb/protocol.h \
    src/qtfb/frame_view.h

LIBS += -lrt
