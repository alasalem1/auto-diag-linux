SUMMARY = "AutoDiag Linux ISO-TP diagnostic test application"
DESCRIPTION = "A native Linux ISO-TP socket client for testing UDS communication"
LICENSE = "CLOSED"

SRC_URI = "file://autodiag-isotp-test.c"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
        ${S}/autodiag-isotp-test.c \
        -o autodiag-isotp-test
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 autodiag-isotp-test ${D}${bindir}/autodiag-isotp-test
}
