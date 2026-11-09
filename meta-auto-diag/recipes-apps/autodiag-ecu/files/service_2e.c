#include "uds_protocol.h"
#include "uds_session.h"
#include "uds_security.h"
#include "did_manager.h"

#define UDS_NRC_INCORRECT_MESSAGE_LENGTH  0x13
#define UDS_NRC_REQUEST_OUT_OF_RANGE      0x31
#define UDS_NRC_SECURITY_ACCESS_DENIED    0x33
#define UDS_NRC_GENERAL_PROGRAMMING_FAILURE 0x72
#define UDS_NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION 0x7E

int uds_service_2e(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len)
{
    if (!request || !response || !response_len)
        return -1;

    *response_len = 0;

    if (request_len != 3 + DID_WRITABLE_DATA_LENGTH) {
        if (response_capacity < 3)
            return -1;

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = 0x2E;
        response[2] = UDS_NRC_INCORRECT_MESSAGE_LENGTH;
        *response_len = 3;
        return 0;
    }

    uint16_t did = ((uint16_t)request[1] << 8) | request[2];

    if (did != DID_WRITABLE_CALIBRATION) {
        if (response_capacity < 3)
            return -1;

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = 0x2E;
        response[2] = UDS_NRC_REQUEST_OUT_OF_RANGE;
        *response_len = 3;
        return 0;
    }

    if (uds_session_get() != UDS_SESSION_EXTENDED) {
        if (response_capacity < 3)
            return -1;

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = 0x2E;
        response[2] = UDS_NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION;
        *response_len = 3;
        return 0;
    }

    if (!uds_security_is_unlocked()) {
        if (response_capacity < 3)
            return -1;

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = 0x2E;
        response[2] = UDS_NRC_SECURITY_ACCESS_DENIED;
        *response_len = 3;
        return 0;
    }

    if (response_capacity < 3)
        return -1;

    if (did_manager_write(
            did,
            &request[3],
            DID_WRITABLE_DATA_LENGTH) != 0) {
        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = 0x2E;
        response[2] = UDS_NRC_GENERAL_PROGRAMMING_FAILURE;
        *response_len = 3;
        return 0;
    }

    response[0] = 0x6E;
    response[1] = (uint8_t)(did >> 8);
    response[2] = (uint8_t)did;
    *response_len = 3;

    return 0;
}
