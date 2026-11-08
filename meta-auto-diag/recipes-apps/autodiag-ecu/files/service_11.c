#include "uds_protocol.h"

/*
 * ECU Reset (UDS service 0x11).
 *
 * Supported reset types:
 * 0x01: Hard Reset
 * 0x02: Key Off On Reset
 * 0x03: Soft Reset
 *
 * The service records a pending reset request.
 * The ECU main process is responsible for executing it
 * after the diagnostic response has been transmitted.
 */

static uint8_t pending_reset_type = 0x00;

uint8_t uds_ecu_reset_get_pending(void)
{
    return pending_reset_type;
}

void uds_ecu_reset_clear_pending(void)
{
    pending_reset_type = 0x00;
}

int uds_service_11(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len)
{
    if (!request || !response || !response_len)
        return -1;

    *response_len = 0;

    if (request_len != 2) {
        if (response_capacity < 3)
            return -1;

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = UDS_SID_ECU_RESET;
        response[2] = 0x13;

        *response_len = 3;
        return 0;
    }

    uint8_t reset_type = request[1] & 0x7F;
    uint8_t suppress_response = request[1] & 0x80;

    if (reset_type < 0x01 || reset_type > 0x03) {
        if (response_capacity < 3)
            return -1;

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = UDS_SID_ECU_RESET;
        response[2] = 0x12;

        *response_len = 3;
        return 0;
    }

    if (!suppress_response && response_capacity < 2)
        return -1;

    pending_reset_type = reset_type;

    if (suppress_response)
        return 0;

    response[0] = 0x51;
    response[1] = reset_type;

    *response_len = 2;

    return 0;
}
