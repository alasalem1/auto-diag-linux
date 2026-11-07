#ifndef UDS_PROTOCOL_H
#define UDS_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define UDS_SID_DIAGNOSTIC_SESSION_CONTROL 0x10
#define UDS_SID_READ_DATA_BY_IDENTIFIER    0x22
#define UDS_SID_SECURITY_ACCESS            0x27
#define UDS_SID_READ_DTC_INFORMATION       0x19
#define UDS_SID_CLEAR_DIAGNOSTIC_INFORMATION 0x14
#define UDS_NEGATIVE_RESPONSE              0x7F

int uds_dispatch_request(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len
);

int uds_service_10(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len
);

int uds_service_22(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len
);

int uds_service_27(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len
);

int uds_service_14(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len
);

int uds_service_19(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len
);

#endif
