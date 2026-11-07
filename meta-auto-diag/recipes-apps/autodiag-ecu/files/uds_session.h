#ifndef UDS_SESSION_H
#define UDS_SESSION_H

#include <stdint.h>

#define UDS_SESSION_DEFAULT  0x01
#define UDS_SESSION_EXTENDED 0x03

uint8_t uds_session_get(void);
int uds_session_set(uint8_t session_type);
void uds_session_reset(void);

#endif
