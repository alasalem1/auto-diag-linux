#include "uds_protocol.h"
#include "dtc_manager.h"

#define UDS_SID_CONTROL_DTC_SETTING_RESPONSE 0xC5

#define UDS_SUBFUNCTION_ON  0x01
#define UDS_SUBFUNCTION_OFF 0x02

#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED 0x12
#define UDS_NRC_INCORRECT_MESSAGE_LENGTH  0x13

int uds_service_85(
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
        response[1] = UDS_SID_CONTROL_DTC_SETTING;
        response[2] = UDS_NRC_INCORRECT_MESSAGE_LENGTH;
        *response_len = 3;
        return 0;
    }

    uint8_t subfunction = request[1] & 0x7F;
    uint8_t suppress_response = request[1] & 0x80;

    if (subfunction != UDS_SUBFUNCTION_ON &&
        subfunction != UDS_SUBFUNCTION_OFF) {
        if (response_capacity < 3)
            return -1;

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = UDS_SID_CONTROL_DTC_SETTING;
        response[2] = UDS_NRC_SUBFUNCTION_NOT_SUPPORTED;
        *response_len = 3;
        return 0;
    }

    if (!suppress_response && response_capacity < 2)
        return -1;

    /* Change only the DTC-setting state; do not erase stored DTCs. */
    dtc_manager_set_setting_enabled(
        subfunction == UDS_SUBFUNCTION_ON
    );

    if (suppress_response)
        return 0;

    response[0] = UDS_SID_CONTROL_DTC_SETTING_RESPONSE;
    response[1] = subfunction;
    *response_len = 2;

    return 0;
}
