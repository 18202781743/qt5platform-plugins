CONFIG += qt
isEmpty(D_DEEPIN_IS_DWAYLAND) {
    QT += KWaylandClient
} else {
    QT += DWaylandClient
}
SOURCES += main.cpp
