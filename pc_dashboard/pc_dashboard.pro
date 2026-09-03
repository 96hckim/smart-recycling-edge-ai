# -----------------------------------------------------
# Smart Recycle Kiosk Dashboard - Project Configuration
# -----------------------------------------------------

QT += core gui network widgets

CONFIG += c++17
CONFIG += no_include_pwd
TEMPLATE = app
TARGET = pc_dashboard

CONFIG(release, debug|release) {
    CONFIG += windows
} else {
    CONFIG += console
}

INCLUDEPATH += . configs network controllers ui ui/pages utils

# 빌드 중간 파일 격리 (루트 디렉토리 오염 방지)
win32 {
    MOC_DIR = build/moc
    OBJECTS_DIR = build/obj
    UI_DIR = build/ui
    RCC_DIR = build/rcc
}

# 런타임 및 컴파일러 최적화
CONFIG(release, debug|release) {
    # MSVC 컴파일러 최적화 (VS 2019 / 2022)
    win32-msvc* {
        QMAKE_CXXFLAGS_RELEASE += /O2 /Oi /Ot /GL /MP /D_CRT_SECURE_NO_WARNINGS
        QMAKE_LFLAGS_RELEASE += /LTCG
    }
    # MinGW GCC 컴파일러 최적화
    win32-g++* {
        QMAKE_CXXFLAGS_RELEASE += -O3 -pipe -march=native
        QMAKE_LFLAGS_RELEASE += -Wl,-s
    }
}

# 소스 파일 목록
SOURCES += \
    main.cpp \
    network/jetson_client.cpp \
    network/server_client.cpp \
    utils/qrcodegen.cpp \
    controllers/eco_tree_controller.cpp \
    ui/mainwindow.cpp \
    ui/pages/idle_page.cpp \
    ui/pages/recycle_page.cpp \
    ui/pages/result_page.cpp

# 헤더 파일 목록
HEADERS += \
    configs/app_config.h \
    configs/theme_constants.h \
    network/jetson_client.h \
    network/server_client.h \
    utils/qrcodegen.hpp \
    controllers/eco_tree_controller.h \
    ui/mainwindow.h \
    ui/pages/idle_page.h \
    ui/pages/recycle_page.h \
    ui/pages/result_page.h

# UI 폼 파일 목록
FORMS += \
    ui/mainwindow.ui \
    ui/pages/idle_page.ui \
    ui/pages/recycle_page.ui \
    ui/pages/result_page.ui

# 리소스 파일 목록
RESOURCES += \
    resources.qrc
