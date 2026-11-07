SUMMARY = "AutoDiag Linux Qt diagnostic graphical interface"
DESCRIPTION = "Qt 6 Widgets GUI for automotive UDS diagnostics"
LICENSE = "CLOSED"

inherit qt6-cmake

SRC_URI = " \
    file://CMakeLists.txt \
    file://main.cpp \
"

S = "${WORKDIR}"

DEPENDS = "qtbase"

FILES:${PN} += "${bindir}/autodiag-gui"
