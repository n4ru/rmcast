QT     += core qml quick quickcontrols2

TARGET = vncast
TEMPLATE = lib
CONFIG += shared plugin no_plugin_name_prefix

# C++ entry, QML-callable launcher singleton, persisted settings, runtime
# device + framebuffer detection, qtfb server skeleton (compiled but not
# yet wired into Connect — keeping today's behavior while scaffolding lands).
SOURCES += \
    src/main.cpp \
    src/launcher.cpp \
    src/settings.cpp \
    src/detect.cpp \
    src/qtfb/server.cpp \
    xovi.cpp
HEADERS += \
    src/launcher.h \
    src/settings.h \
    src/detect.h \
    src/qtfb/server.h \
    src/qtfb/protocol.h

LIBS += -lrt
