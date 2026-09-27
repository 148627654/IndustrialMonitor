QT       += core gui widgets sql

TARGET    = IndustrialMonitor
TEMPLATE  = app

# -------------------------------------------------------------
# 1. 编译产物重定向 (将 bin/ 和 build/ 彻底隔离)
# -------------------------------------------------------------
DESTDIR     = $$PWD/bin
MOC_DIR     = $$PWD/build/moc
OBJECTS_DIR = $$PWD/build/obj
RCC_DIR     = $$PWD/build/rcc
UI_DIR      = $$PWD/build/ui

# -------------------------------------------------------------
# 2. 全局包含路径 (避免深层相对路径 ../../include)
# -------------------------------------------------------------
INCLUDEPATH += \
    $$PWD/src \
    $$PWD/src/common \
    $$PWD/src/core \
    $$PWD/src/models \
    $$PWD/src/views \
    $$PWD/src/db

# -------------------------------------------------------------
# 3. 源码与头文件映射
# -------------------------------------------------------------
SOURCES += \
    src/db/databasemanager.cpp \
    src/main.cpp \
    src/common/thememanager.cpp \
    src/models/deviceproxymodel.cpp \
    src/models/devicetablemodel.cpp \
    src/views/adddevicedialog.cpp \
    src/views/logindialog.cpp \
    src/views/mainwindow.cpp \
    src/views/progressbardelegate.cpp \
    src/views/statusdelegate.cpp

HEADERS += \
    src/common/DeviceDef.h \
    src/common/thememanager.h \
    src/db/databasemanager.h \
    src/models/deviceproxymodel.h \
    src/models/devicetablemodel.h \
    src/views/adddevicedialog.h \
    src/views/logindialog.h \
    src/views/mainwindow.h \
    src/views/progressbardelegate.h \
    src/views/statusdelegate.h

FORMS += \
    src/views/logindialog.ui \
    src/views/mainwindow.ui

# -------------------------------------------------------------
# 4. 资源文件
# -------------------------------------------------------------
RESOURCES += \
    res/res.qrc