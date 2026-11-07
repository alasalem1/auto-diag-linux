#include "uds_protocol.h"
#include "dtc_manager.h"

#define UDS_SID_READ_DTC_INFORMATION 0x19
#define UDS_SID_READ_DTC_INFORMATION_RESPONSE 0x59

#define UDS_NRC_INCORRECT_MESSAGE_LENGTH 0x13
#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED 0x12

int uds_service_19(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len)
{
    if (!request || !response || !response_len)
        return -1;

    *response_len = 0;

    if (request_len != 3) {
        if (response_capacity < 3)
            return -1;

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = UDS_SID_READ_DTC_INFORMATION;
        response[2] = UDS_NRC_INCORRECT_MESSAGE_LENGTH;
        *response_len = 3;
        return 0;
    }

    uint8_t subfunction = request[1];
    uint8_t status_mask = request[2];

    if (subfunction != 0x02) {
        if (response_capacity < 3)
            return -1;

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = UDS_SID_READ_DTC_INFORMATION;
        response[2] = UDS_NRC_SUBFUNCTION_NOT_SUPPORTED;
        *response_len = 3;
        return 0;
    }

    struct dtc_record records[DTC_MANAGER_MAX_RECORDS];

    size_t count = dtc_manager_get_by_status_mask(
        status_mask,
        records,
        DTC_MANAGER_MAX_RECORDS
    );

    size_t required_capacity = 3 + (count * 4);

    if (response_capacity < required_capacity)
        return -1;

    response[0] = UDS_SID_READ_DTC_INFORMATION_RESPONSE;
    response[1] = subfunction;
    response[2] = dtc_manager_get_status_availability_mask();

    size_t offset = 3;

    for (size_t i = 0; i < count; i++) {
        response[offset++] = (uint8_t)(records[i].dtc >> 16);
        response[offset++] = (uint8_t)(records[i].dtc >> 8);
        response[offset++] = (uint8_t)records[i].dtc;
        response[offset++] = records[i].status;
    }

    *response_len = offset;

    return 0;
}
