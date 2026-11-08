#ifndef UDS_SESSION_H
#define UDS_SESSION_H

#include <stdint.h>

#define UDS_SESSION_DEFAULT              0x01
#define UDS_SESSION_EXTENDED             0x03
#define UDS_S3_SERVER_TIMEOUT_MS         5000U

uint8_t uds_session_get(void);
int uds_session_set(uint8_t session_type);
void uds_session_reset(void);

void uds_session_activity(void);
int uds_session_check_timeout(void);
int uds_session_set_s3_timeout_ms(uint32_t timeout_ms);

#endif
