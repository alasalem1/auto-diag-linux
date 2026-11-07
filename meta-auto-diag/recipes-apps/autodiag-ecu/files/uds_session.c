#include "uds_session.h"

static uint8_t current_session = UDS_SESSION_DEFAULT;

uint8_t uds_session_get(void)
{
    return current_session;
}

int uds_session_set(uint8_t session_type)
{
    if (session_type != UDS_SESSION_DEFAULT &&
        session_type != UDS_SESSION_EXTENDED) {
        return -1;
    }

    current_session = session_type;
    return 0;
}

void uds_session_reset(void)
{
    current_session = UDS_SESSION_DEFAULT;
}
