#include "uds_protocol.h"
#include "dtc_manager.h"

#define UDS_SID_CLEAR_DIAGNOSTIC_INFORMATION 0x14
#define UDS_SID_CLEAR_DIAGNOSTIC_INFORMATION_RESPONSE 0x54

#define UDS_NRC_INCORRECT_MESSAGE_LENGTH 0x13
#define UDS_NRC_REQUEST_OUT_OF_RANGE 0x31

int uds_service_14(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len)
{
    if (!request || !response || !response_len)
        return -1;

    *response_len = 0;

    if (request_len != 4) {
        if (response_capacity < 3)
            return -1;

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = UDS_SID_CLEAR_DIAGNOSTIC_INFORMATION;
        response[2] = UDS_NRC_INCORRECT_MESSAGE_LENGTH;
        *response_len = 3;
        return 0;
    }

    /*
     * This implementation supports the all-DTC group only:
     * groupOfDTC = 0xFFFFFF.
     */
    if (request[1] != 0xFF ||
        request[2] != 0xFF ||
        request[3] != 0xFF) {
        if (response_capacity < 3)
            return -1;

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = UDS_SID_CLEAR_DIAGNOSTIC_INFORMATION;
        response[2] = UDS_NRC_REQUEST_OUT_OF_RANGE;
        *response_len = 3;
        return 0;
    }

    if (response_capacity < 1)
        return -1;

    dtc_manager_clear_all();

    response[0] = UDS_SID_CLEAR_DIAGNOSTIC_INFORMATION_RESPONSE;
    *response_len = 1;

    return 0;
}
