SUMMARY = "AutoDiag Linux ISO-TP diagnostic test applications"
DESCRIPTION = "Linux ISO-TP socket clients for testing UDS communication"
LICENSE = "CLOSED"

SRC_URI = " \
    file://autodiag-isotp-test.c \
    file://autodiag-session-test.c \
    file://autodiag-dtc-test.c \
"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
        ${S}/autodiag-isotp-test.c \
        -o autodiag-isotp-test

    ${CC} ${CFLAGS} ${LDFLAGS} \
        ${S}/autodiag-session-test.c \
        -o autodiag-session-test

    ${CC} ${CFLAGS} ${LDFLAGS} \
        ${S}/autodiag-dtc-test.c \
        -o autodiag-dtc-test
}

do_install() {
    install -d ${D}${bindir}

    install -m 0755 ${B}/autodiag-isotp-test \
        ${D}${bindir}/autodiag-isotp-test

    install -m 0755 ${B}/autodiag-session-test \
        ${D}${bindir}/autodiag-session-test

    install -m 0755 ${B}/autodiag-dtc-test \
        ${D}${bindir}/autodiag-dtc-test
}
