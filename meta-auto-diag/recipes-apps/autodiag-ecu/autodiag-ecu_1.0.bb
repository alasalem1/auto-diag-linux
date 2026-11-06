SUMMARY = "AutoDiag Linux simulated UDS ECU"
DESCRIPTION = "A Linux ISO-TP ECU simulator supporting UDS ReadDataByIdentifier"
LICENSE = "CLOSED"

SRC_URI = "file://autodiag-ecu.c"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
        ${S}/autodiag-ecu.c \
        -o autodiag-ecu
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 autodiag-ecu ${D}${bindir}/autodiag-ecu
}
