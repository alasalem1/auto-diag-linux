#include "uds_protocol.h"
#include "uds_session.h"
#include "uds_security.h"

#define UDS_NRC_INCORRECT_MESSAGE_LENGTH  0x13
#define UDS_NRC_REQUEST_SEQUENCE_ERROR    0x24
#define UDS_NRC_INVALID_KEY               0x35
#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED 0x12
#define UDS_NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION 0x7E

static int send_negative(
    uint8_t nrc,
    uint8_t *response,
    size_t capacity,
    size_t *response_len)
{
    if (capacity < 3)
        return -1;

    response[0] = UDS_NEGATIVE_RESPONSE;
    response[1] = 0x27;
    response[2] = nrc;

    *response_len = 3;
    return 0;
}

int uds_service_27(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len)
{
    if (request_len < 2)
        return send_negative(
            UDS_NRC_INCORRECT_MESSAGE_LENGTH,
            response, response_capacity, response_len);

    uint8_t subfunction = request[1];

    if (subfunction != 0x01 && subfunction != 0x02)
        return send_negative(
            UDS_NRC_SUBFUNCTION_NOT_SUPPORTED,
            response, response_capacity, response_len);

    if ((subfunction == 0x01 && request_len != 2) ||
        (subfunction == 0x02 && request_len != 6))
        return send_negative(
            UDS_NRC_INCORRECT_MESSAGE_LENGTH,
            response, response_capacity, response_len);

    if (uds_session_get() != UDS_SESSION_EXTENDED)
        return send_negative(
            UDS_NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION,
            response, response_capacity, response_len);

    if (subfunction == 0x01) {
        if (response_capacity < 6)
            return -1;

        uint32_t seed = uds_security_generate_seed();

        response[0] = 0x67;
        response[1] = 0x01;
        response[2] = (uint8_t)(seed >> 24);
        response[3] = (uint8_t)(seed >> 16);
        response[4] = (uint8_t)(seed >> 8);
        response[5] = (uint8_t)seed;

        *response_len = 6;
        return 0;
    }

    if (!uds_security_has_valid_seed())
        return send_negative(
            UDS_NRC_REQUEST_SEQUENCE_ERROR,
            response, response_capacity, response_len);

    uint32_t key =
        ((uint32_t)request[2] << 24) |
        ((uint32_t)request[3] << 16) |
        ((uint32_t)request[4] << 8) |
        (uint32_t)request[5];

    /* Ensure the positive response can be constructed before unlocking. */
    if (response_capacity < 2)
        return -1;

    int key_result = uds_security_validate_key(key);

    if (key_result == -2)
        return send_negative(
            UDS_NRC_INVALID_KEY,
            response, response_capacity, response_len);

    if (key_result != 0)
        return send_negative(
            UDS_NRC_REQUEST_SEQUENCE_ERROR,
            response, response_capacity, response_len);

    response[0] = 0x67;
    response[1] = 0x02;

    *response_len = 2;
    return 0;
}
