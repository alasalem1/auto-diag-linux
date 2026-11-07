SUMMARY = "AutoDiag Linux modular UDS ECU server"
DESCRIPTION = "Modular UDS diagnostic server using Linux ISO-TP sockets"
LICENSE = "CLOSED"

inherit ptest systemd

SYSTEMD_SERVICE:${PN} = "autodiag-can-setup.service autodiag-ecu.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

SRC_URI = " \
    file://autodiag-ecu.c \
    file://uds_protocol.h \
    file://uds_dispatch.c \
    file://service_10.c \
    file://service_14.c \
    file://service_19.c \
    file://dtc_manager.h \
    file://dtc_manager.c \
    file://service_22.c \
    file://service_27.c \
    file://uds_security.h \
    file://uds_security.c \
    file://uds_session.h \
    file://uds_session.c \
    file://test_uds_dispatch.c \
    file://run-ptest \
    file://autodiag-can-setup \
    file://autodiag-can-setup.service \
    file://autodiag-ecu.service \
"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
        ${S}/autodiag-ecu.c \
        ${S}/uds_dispatch.c \
        ${S}/service_10.c \
        ${S}/service_14.c \
        ${S}/service_19.c \
        ${S}/dtc_manager.c \
        ${S}/service_22.c \
        ${S}/service_27.c \
        ${S}/uds_security.c \
        ${S}/uds_session.c \
        -o autodiag-ecu

    ${CC} ${CFLAGS} ${LDFLAGS} \
        ${S}/test_uds_dispatch.c \
        ${S}/uds_dispatch.c \
        ${S}/service_10.c \
        ${S}/service_14.c \
        ${S}/service_19.c \
        ${S}/dtc_manager.c \
        ${S}/service_22.c \
        ${S}/service_27.c \
        ${S}/uds_security.c \
        ${S}/uds_session.c \
        -o test-uds-dispatch
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${B}/autodiag-ecu \
        ${D}${bindir}/autodiag-ecu

    install -d ${D}${sbindir}
    install -m 0755 ${WORKDIR}/autodiag-can-setup \
        ${D}${sbindir}/autodiag-can-setup

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/autodiag-can-setup.service \
        ${D}${systemd_system_unitdir}/autodiag-can-setup.service
    install -m 0644 ${WORKDIR}/autodiag-ecu.service \
        ${D}${systemd_system_unitdir}/autodiag-ecu.service
}

do_install_ptest() {
    install -d ${D}${PTEST_PATH}

    install -m 0755 ${B}/test-uds-dispatch \
        ${D}${PTEST_PATH}/test-uds-dispatch

    install -m 0755 ${WORKDIR}/run-ptest \
        ${D}${PTEST_PATH}/run-ptest
}
