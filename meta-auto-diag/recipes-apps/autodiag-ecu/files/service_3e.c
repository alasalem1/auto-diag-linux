#include "uds_protocol.h"

int uds_service_3e(
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
        response[1] = UDS_SID_TESTER_PRESENT;
        response[2] = 0x13;

        *response_len = 3;
        return 0;
    }

    /*
     * Bit 7 indicates suppress-positive-response.
     * The remaining bits contain the subfunction.
     */
    uint8_t subfunction = request[1] & 0x7F;
    uint8_t suppress_response = request[1] & 0x80;

    if (subfunction != 0x00) {
        if (response_capacity < 3)
            return -1;

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = UDS_SID_TESTER_PRESENT;
        response[2] = 0x12;

        *response_len = 3;
        return 0;
    }

    /* Tester Present with suppress-positive-response (3E 80). */
    if (suppress_response) {
        *response_len = 0;
        return 0;
    }

    /* Normal Tester Present response (3E 00 -> 7E 00). */
    if (response_capacity < 2)
        return -1;

    response[0] = 0x7E;
    response[1] = 0x00;

    *response_len = 2;
    return 0;
}
