#define _POSIX_C_SOURCE 200809L

#include <time.h>

#include "uds_session.h"
#include "uds_security.h"

static uint8_t current_session = UDS_SESSION_DEFAULT;
static uint32_t s3_timeout_ms = UDS_S3_SERVER_TIMEOUT_MS;
static struct timespec last_activity;
static int s3_timer_active;

static void update_activity_time(void)
{
    if (clock_gettime(CLOCK_MONOTONIC, &last_activity) == 0) {
        s3_timer_active = 1;
    }
}

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

    if (session_type == UDS_SESSION_EXTENDED) {
        update_activity_time();
    } else {
        s3_timer_active = 0;
    }

    return 0;
}

void uds_session_reset(void)
{
    current_session = UDS_SESSION_DEFAULT;
    s3_timer_active = 0;
}

void uds_session_activity(void)
{
    if (current_session == UDS_SESSION_EXTENDED) {
        update_activity_time();
    }
}

int uds_session_check_timeout(void)
{
    struct timespec now;
    int64_t elapsed_ms;

    if (current_session != UDS_SESSION_EXTENDED ||
        !s3_timer_active) {
        return 0;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }

    elapsed_ms =
        (int64_t)(now.tv_sec - last_activity.tv_sec) * 1000 +
        (now.tv_nsec - last_activity.tv_nsec) / 1000000;

    if (elapsed_ms < (int64_t)s3_timeout_ms) {
        return 0;
    }

    current_session = UDS_SESSION_DEFAULT;
    s3_timer_active = 0;
    uds_security_reset();

    return 1;
}

int uds_session_set_s3_timeout_ms(uint32_t timeout_ms)
{
    if (timeout_ms == 0) {
        return -1;
    }

    s3_timeout_ms = timeout_ms;

    if (current_session == UDS_SESSION_EXTENDED) {
        update_activity_time();
    }

    return 0;
}
