#include "uds_protocol.h"

static int negative_response(
    uint8_t request_sid,
    uint8_t nrc,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len)
{
    if (response_capacity < 3)
        return -1;

    response[0] = UDS_NEGATIVE_RESPONSE;
    response[1] = request_sid;
    response[2] = nrc;

    *response_len = 3;

    return 0;
}

int uds_dispatch_request(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len)
{
    if (!request || !response || !response_len)
        return -1;

    *response_len = 0;

    if (request_len == 0)
        return negative_response(0x00, 0x13,
                                 response, response_capacity,
                                 response_len);

    switch (request[0]) {

    case UDS_SID_ECU_RESET:
        return uds_service_11(request, request_len,
                              response, response_capacity,
                              response_len);

    case UDS_SID_DIAGNOSTIC_SESSION_CONTROL:
        return uds_service_10(request, request_len,
                              response, response_capacity,
                              response_len);

    case UDS_SID_READ_DATA_BY_IDENTIFIER:
        return uds_service_22(request, request_len,
                              response, response_capacity,
                              response_len);

    case UDS_SID_SECURITY_ACCESS:
        return uds_service_27(request, request_len,
                              response, response_capacity,
                              response_len);

    case UDS_SID_CLEAR_DIAGNOSTIC_INFORMATION:
        return uds_service_14(request, request_len,
                              response, response_capacity,
                              response_len);

    case UDS_SID_READ_DTC_INFORMATION:
        return uds_service_19(request, request_len,
                              response, response_capacity,
                              response_len);

    case UDS_SID_TESTER_PRESENT:
        return uds_service_3e(request, request_len,
                              response, response_capacity,
                              response_len);

    case UDS_SID_COMMUNICATION_CONTROL:
        return uds_service_28(request, request_len,
                              response, response_capacity,
                              response_len);

    default:
        return negative_response(request[0], 0x11,
                                 response, response_capacity,
                                 response_len);
    }
}
