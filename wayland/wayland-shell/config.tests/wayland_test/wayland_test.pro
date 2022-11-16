TEMPLATE = app

CONFIG += qt

QT += core
QT += gui
QT += core-private
QT += gui-private
QT += waylandclient
QT += waylandclient-private

TARGET = wayland_test

isEmpty(D_DEEPIN_IS_DWAYLAND) {
    QT       += KWaylandClient
    LIBS     += -lKF5WaylandClient
} else {
    QT       += DWaylandClient
    LIBS     += -lDWaylandClient
}
CONFIG += link_pkgconfig
PKGCONFIG += Qt5WaylandClient
# xkb
QT += xkbcommon_support-private

CONFIG += c++17

SOURCES += main.cpp

