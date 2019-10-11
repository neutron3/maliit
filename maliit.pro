TARGET = maliitplatforminputcontextplugin

PLUGIN_TYPE = platforminputcontexts
PLUGIN_CLASS_NAME = QMaliitPlatformInputContextPlugin
load(qt_plugin)

QT += dbus gui-private
SOURCES += $$PWD/qmaliitplatforminputcontext.cpp \
           $$PWD/qmcontextadaptor.cpp \
           $$PWD/qmserverdbusaddress.cpp \
           $$PWD/qmserverproxy.cpp \
           $$PWD/main.cpp

HEADERS += $$PWD/qmaliitplatforminputcontext.h \
           $$PWD/qmcontextadaptor.h \
           $$PWD/qmnamespace.h \
           $$PWD/qmserverdbusaddress.h \
           $$PWD/qmserverproxy.h

OTHER_FILES += $$PWD/maliit.json
