QT       += core gui widgets sql

TARGET    = IndustrialMonitor
TEMPLATE  = app

# -------------------------------------------------------------
# 0. 编译器洁癖级检查基线 (0 Warning 硬性门槛)
# -------------------------------------------------------------
CONFIG += c++17
QMAKE_CXXFLAGS += -Wall -Wextra

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
    $$PWD/src/hardware \
    $$PWD/src/dao \
    $$PWD/src/hardware/mock \
    $$PWD/src/alarm \
    $$PWD/src/db

# -------------------------------------------------------------
# 3. 源码与头文件映射
# -------------------------------------------------------------
SOURCES += \
    src/alarm/alarmmanager.cpp \
    src/common/validatorutils.cpp \
    src/core/acquisitioncontroller.cpp \
    src/dao/devicedao.cpp \
    src/db/databasemanager.cpp \
    src/hardware/deviceconfig.cpp \
    src/hardware/devicefactory.cpp \
    src/hardware/idevice.cpp \
    src/hardware/mock/mockdevice.cpp \
    src/main.cpp \
    src/common/thememanager.cpp \
    src/models/deviceproxymodel.cpp \
    src/models/devicetablemodel.cpp \
    src/views/adddevicedialog.cpp \
    src/views/editdevicedialog.cpp \
    src/views/logindialog.cpp \
    src/views/mainwindow.cpp \
    src/views/progressbardelegate.cpp \
    src/views/rowglowdelegate.cpp \
    src/views/statusdelegate.cpp \
    src/views/widgets/AlarmBanner.cpp \
    src/core/acquisitionworker.cpp

HEADERS += \
    src/alarm/AlarmDef.h \
    src/alarm/alarmmanager.h \
    src/common/DeviceDef.h \
    src/common/TelemetryDef.h \
    src/common/thememanager.h \
    src/common/validatorutils.h \
    src/core/acquisitioncontroller.h \
    src/dao/devicedao.h \
    src/db/databasemanager.h \
    src/hardware/deviceconfig.h \
    src/hardware/devicefactory.h \
    src/hardware/idevice.h \
    src/hardware/mock/mockdevice.h \
    src/models/deviceproxymodel.h \
    src/models/devicetablemodel.h \
    src/views/adddevicedialog.h \
    src/views/editdevicedialog.h \
    src/views/logindialog.h \
    src/views/mainwindow.h \
    src/views/progressbardelegate.h \
    src/views/rowglowdelegate.h \
    src/views/statusdelegate.h  \
    src/views/widgets/AlarmBanner.h \
    src/core/acquisitionworker.h

FORMS += \
    src/views/logindialog.ui \
    src/views/mainwindow.ui

# -------------------------------------------------------------
# 4. 资源文件
# -------------------------------------------------------------
RESOURCES += \
    res/res.qrc