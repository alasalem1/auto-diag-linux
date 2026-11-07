#include "uds_protocol.h"
#include "uds_session.h"
#include "uds_security.h"

#define UDS_NRC_INCORRECT_MESSAGE_LENGTH  0x13
#define UDS_NRC_REQUEST_OUT_OF_RANGE      0x31
#define UDS_NRC_SECURITY_ACCESS_DENIED     0x33
#define UDS_NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION 0x7E

#define DID_VIN                 0xF190
#define DID_PROTECTED_CALIBRATION 0xF1A0

static const uint8_t simulated_vin[] = "AUTODIAG000000001";
static const uint8_t protected_calibration[] = "AUTODIAG-CAL-001";

static int send_negative_response(
    uint8_t nrc,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len)
{
    if (response_capacity < 3)
        return -1;

    response[0] = UDS_NEGATIVE_RESPONSE;
    response[1] = UDS_SID_READ_DATA_BY_IDENTIFIER;
    response[2] = nrc;

    *response_len = 3;
    return 0;
}

int uds_service_22(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len)
{
    if (request_len != 3) {
        return send_negative_response(
            UDS_NRC_INCORRECT_MESSAGE_LENGTH,
            response, response_capacity, response_len);
    }

    uint16_t did = ((uint16_t)request[1] << 8) | request[2];

    /* Existing VIN DID: accessible only in Default Session. */
    if (did == DID_VIN) {
        if (uds_session_get() != UDS_SESSION_DEFAULT) {
            return send_negative_response(
                UDS_NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION,
                response, response_capacity, response_len);
        }

        if (response_capacity < 20)
            return -1;

        response[0] = 0x62;
        response[1] = 0xF1;
        response[2] = 0x90;

        for (size_t i = 0; i < sizeof(simulated_vin) - 1; i++)
            response[i + 3] = simulated_vin[i];

        *response_len = 20;
        return 0;
    }

    /* Protected calibration DID: Extended Session + SecurityAccess. */
    if (did == DID_PROTECTED_CALIBRATION) {
        if (uds_session_get() != UDS_SESSION_EXTENDED) {
            return send_negative_response(
                UDS_NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION,
                response, response_capacity, response_len);
        }

        if (!uds_security_is_unlocked()) {
            return send_negative_response(
                UDS_NRC_SECURITY_ACCESS_DENIED,
                response, response_capacity, response_len);
        }

        const size_t data_len = sizeof(protected_calibration) - 1;

        if (response_capacity < data_len + 3)
            return -1;

        response[0] = 0x62;
        response[1] = 0xF1;
        response[2] = 0xA0;

        for (size_t i = 0; i < data_len; i++)
            response[i + 3] = protected_calibration[i];

        *response_len = data_len + 3;
        return 0;
    }

    return send_negative_response(
        UDS_NRC_REQUEST_OUT_OF_RANGE,
        response, response_capacity, response_len);
}
