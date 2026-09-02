# ----------------------------------------------------
# Smart Recycle Kiosk Dashboard - Project Configuration
# ----------------------------------------------------

QT += core gui network widgets

# C++17 표준 및 콘솔/GUI 앱 설정
CONFIG += c++17
CONFIG += no_include_pwd
TEMPLATE = app
TARGET = pc_dashboard

# 헤더 참조 경로
INCLUDEPATH += . configs network core ui

# 빌드 및 런타임 성능 최적화
CONFIG(release, debug|release) {
    # MSVC 컴파일러 최적화
    win32-msvc* {
        QMAKE_CXXFLAGS_RELEASE += /O2 /Oi /Ot /GL /MP
        QMAKE_LFLAGS_RELEASE += /LTCG
    }
    # GCC / Clang 컴파일러 최적화 (Linux / MinGW)
    unix|win32-g++* {
        QMAKE_CXXFLAGS_RELEASE += -O3 -pipe
    }
}

# 소스 파일 목록
SOURCES += \
    main.cpp \
    ui/eco_tree_controller.cpp \
    ui/idle_page.cpp \
    ui/mainwindow.cpp \
    network/jetson_client.cpp \
    core/kiosk_client.cpp \
    ui/recycle_page.cpp \
    ui/result_page.cpp

# 헤더 파일 목록
HEADERS += \
    configs/app_config.h \
    ui/eco_tree_controller.h \
    ui/idle_page.h \
    ui/mainwindow.h \
    network/jetson_client.h \
    core/kiosk_client.h \
    ui/recycle_page.h \
    ui/result_page.h \
    ui/theme_constants.h

# UI 폼 파일 목록
FORMS += \
    ui/idle_page.ui \
    ui/mainwindow.ui \
    ui/recycle_page.ui \
    ui/result_page.ui

# 리소스 파일 목록
RESOURCES += \
    resources.qrc
