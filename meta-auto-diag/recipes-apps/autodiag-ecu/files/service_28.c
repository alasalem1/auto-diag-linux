#include "uds_protocol.h"
#include "uds_session.h"

/*
 * Communication Control state.
 *
 * 0x00: Enable Rx and Tx
 * 0x01: Enable Rx, disable Tx
 * 0x02: Disable Rx, enable Tx
 * 0x03: Disable Rx and Tx
 *
 * This state describes normal application communication.
 * It does not disable diagnostic ISO-TP communication.
 */

static uint8_t communication_control_type = 0x00;

uint8_t uds_communication_control_get(void)
{
    return communication_control_type;
}

void uds_communication_control_reset(void)
{
    communication_control_type = 0x00;
}

int uds_service_28(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len)
{
    if (request_len != 3) {
        if (response_capacity < 3)
            return -1;

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = UDS_SID_COMMUNICATION_CONTROL;
        response[2] = 0x13;
        *response_len = 3;
        return 0;
    }

    uint8_t control_type = request[1] & 0x7F;
    uint8_t suppress_response = request[1] & 0x80;
    uint8_t communication_type = request[2];

    if (control_type > 0x03) {
        if (response_capacity < 3)
            return -1;

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = UDS_SID_COMMUNICATION_CONTROL;
        response[2] = 0x12;
        *response_len = 3;
        return 0;
    }

    /*
     * Support subnet number 0 and normal communication (0x01).
     * Other communication types are not implemented yet.
     */
    if (communication_type != 0x01) {
        if (response_capacity < 3)
            return -1;

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = UDS_SID_COMMUNICATION_CONTROL;
        response[2] = 0x31;
        *response_len = 3;
        return 0;
    }

    if (uds_session_get() != UDS_SESSION_EXTENDED) {
        if (response_capacity < 3)
            return -1;

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = UDS_SID_COMMUNICATION_CONTROL;
        response[2] = 0x7E;
        *response_len = 3;
        return 0;
    }

    if (!suppress_response && response_capacity < 3)
        return -1;

    /* Apply the requested communication-control state. */
    communication_control_type = control_type;

    if (suppress_response) {
        *response_len = 0;
        return 0;
    }

    response[0] = 0x68;
    response[1] = control_type;
    response[2] = communication_type;

    *response_len = 3;
    return 0;
}
