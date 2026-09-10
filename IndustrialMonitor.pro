QT       += core gui widgets

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
    $$PWD/src/views

# -------------------------------------------------------------
# 3. 源码与头文件映射
# -------------------------------------------------------------
SOURCES += \
    src/main.cpp \
    src/common/ThemeManager.cpp \
    src/views/mainwindow.cpp

HEADERS += \
    src/common/ThemeManager.h \
    src/views/mainwindow.h

FORMS += \
    src/views/mainwindow.ui

# -------------------------------------------------------------
# 4. 资源文件
# -------------------------------------------------------------
RESOURCES += \
    res/res.qrc