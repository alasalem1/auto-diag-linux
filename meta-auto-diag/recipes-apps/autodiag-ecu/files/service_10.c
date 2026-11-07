#include "uds_protocol.h"
#include "uds_session.h"
#include "uds_security.h"

int uds_service_10(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len)
{
    if (request_len != 2) {
        if (response_capacity < 3)
            return -1;

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = UDS_SID_DIAGNOSTIC_SESSION_CONTROL;
        response[2] = 0x13;

        *response_len = 3;
        return 0;
    }

    uint8_t session_type = request[1];

    if (session_type != UDS_SESSION_DEFAULT &&
        session_type != UDS_SESSION_EXTENDED) {
        if (response_capacity < 3)
            return -1;

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = UDS_SID_DIAGNOSTIC_SESSION_CONTROL;
        response[2] = 0x12;

        *response_len = 3;
        return 0;
    }

    if (response_capacity < 6)
        return -1;

    response[0] = 0x50;
    response[1] = session_type;

    /* P2Server_max = 50 ms */
    response[2] = 0x00;
    response[3] = 0x32;

    /* P2*Server_max = 5000 ms, encoded in 10 ms units */
    response[4] = 0x01;
    response[5] = 0xF4;

    *response_len = 6;

    /* Reset security only when the diagnostic session changes. */
    if (uds_session_get() != session_type) {
        if (uds_session_set(session_type) != 0)
            return -1;

        uds_security_reset();
    }

    return 0;
}
