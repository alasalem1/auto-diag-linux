#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#include "uds_protocol.h"
#include "uds_session.h"
#include "uds_security.h"

static int run_test(
    const char *name,
    const uint8_t *request,
    size_t request_len,
    const uint8_t *expected,
    size_t expected_len)
{
    uint8_t response[64] = {0};
    size_t response_len = 0;

    int result = uds_dispatch_request(
        request, request_len,
        response, sizeof(response),
        &response_len
    );

    int passed = result == 0 &&
                 response_len == expected_len &&
                 memcmp(response, expected, expected_len) == 0;

    printf("[%s] %s\n", passed ? "PASS" : "FAIL", name);

    if (!passed) {
        printf("  Return: %d, response length: %zu\n",
               result, response_len);
    }

    return passed ? 0 : 1;
}

static int test_insufficient_capacity(void)
{
    const uint8_t request[] = {0x22, 0xF1, 0x90};
    uint8_t response[19];
    size_t response_len = 99;

    memset(response, 0xAA, sizeof(response));

    int result = uds_dispatch_request(
        request, sizeof(request),
        response, sizeof(response),
        &response_len
    );

    int unchanged = 1;

    for (size_t i = 0; i < sizeof(response); i++) {
        if (response[i] != 0xAA)
            unchanged = 0;
    }

    int passed = result == -1 &&
                 response_len == 0 &&
                 unchanged;

    printf("[%s] Insufficient response buffer capacity\n",
           passed ? "PASS" : "FAIL");

    return passed ? 0 : 1;
}


static void sleep_ms(long milliseconds)
{
    struct timespec delay = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000) * 1000000L
    };

    while (nanosleep(&delay, &delay) == -1) {
        /* Continue sleeping if interrupted. */
    }
}

static int test_session_timeout(void)
{
    int passed = 1;

    /* A zero timeout must be rejected. */
    if (uds_session_set_s3_timeout_ms(0) != -1)
        passed = 0;

    /* Use a short timeout for this test. */
    if (uds_session_set_s3_timeout_ms(100) != 0)
        passed = 0;

    uds_session_reset();
    uds_security_reset();

    /* Enter Extended Session and unlock SecurityAccess. */
    if (uds_session_set(UDS_SESSION_EXTENDED) != 0)
        passed = 0;

    uint32_t seed = uds_security_generate_seed();
    uint32_t key = uds_security_calculate_key(seed);

    if (uds_security_validate_key(key) != 0 ||
        !uds_security_is_unlocked())
        passed = 0;

    /* The session should expire after inactivity. */
    sleep_ms(150);

    if (uds_session_check_timeout() != 1)
        passed = 0;

    if (uds_session_get() != UDS_SESSION_DEFAULT)
        passed = 0;

    if (uds_security_is_unlocked())
        passed = 0;

    /* Test that diagnostic activity refreshes the timer. */
    if (uds_session_set(UDS_SESSION_EXTENDED) != 0)
        passed = 0;

    sleep_ms(60);
    uds_session_activity();

    sleep_ms(60);

    if (uds_session_check_timeout() != 0)
        passed = 0;

    sleep_ms(60);

    if (uds_session_check_timeout() != 1)
        passed = 0;

    if (uds_session_get() != UDS_SESSION_DEFAULT)
        passed = 0;

    /* Restore the production timeout for subsequent tests. */
    uds_session_reset();

    if (uds_session_set_s3_timeout_ms(UDS_S3_SERVER_TIMEOUT_MS) != 0)
        passed = 0;

    printf("[%s] S3Server timeout, activity refresh and SecurityAccess reset\n",
           passed ? "PASS" : "FAIL");

    return passed ? 0 : 1;
}

int main(void)
{
    int failures = 0;

    const uint8_t valid_vin[] = {0x22, 0xF1, 0x90};
    const uint8_t unsupported_did[] = {0x22, 0xF1, 0x87};
    const uint8_t invalid_length[] = {0x22, 0xF1};
    const uint8_t unsupported_service[] = {0x99, 0x01};
    const uint8_t empty_request[] = {0x00};
    const uint8_t tester_present[] = {0x3E, 0x00};
    const uint8_t tester_present_suppress[] = {0x3E, 0x80};
    const uint8_t tester_present_unsupported[] = {0x3E, 0x01};
    const uint8_t tester_present_invalid[] = {0x3E};


    const uint8_t expected_vin[] = {
        0x62, 0xF1, 0x90,
        'A', 'U', 'T', 'O', 'D', 'I', 'A', 'G',
        '0', '0', '0', '0', '0', '0', '0', '0', '1'
    };

    const uint8_t expected_unsupported_did[] = {
        0x7F, 0x22, 0x31
    };

    const uint8_t expected_invalid_length[] = {
        0x7F, 0x22, 0x13
    };

    const uint8_t expected_unsupported_service[] = {
        0x7F, 0x99, 0x11
    };

    const uint8_t expected_empty_request[] = {
        0x7F, 0x00, 0x13
    };

    const uint8_t expected_tester_present[] = {0x7E, 0x00};
    const uint8_t expected_tester_present_suppress[] = {0x00};
    const uint8_t expected_tester_present_unsupported[] = {
        0x7F, 0x3E, 0x12
    };
    const uint8_t expected_tester_present_invalid[] = {
        0x7F, 0x3E, 0x13
    };


    uds_session_reset();

    printf("====================================\n");
    printf("   AutoDiag UDS Dispatcher Tests\n");
    printf("====================================\n");

    failures += run_test(
        "Valid VIN request",
        valid_vin, sizeof(valid_vin),
        expected_vin, sizeof(expected_vin)
    );

    failures += run_test(
        "Unsupported DID",
        unsupported_did, sizeof(unsupported_did),
        expected_unsupported_did, sizeof(expected_unsupported_did)
    );

    failures += run_test(
        "Invalid request length",
        invalid_length, sizeof(invalid_length),
        expected_invalid_length, sizeof(expected_invalid_length)
    );

    failures += run_test(
        "Unsupported service",
        unsupported_service, sizeof(unsupported_service),
        expected_unsupported_service, sizeof(expected_unsupported_service)
    );

    failures += run_test(
        "Empty request",
        empty_request, 0,
        expected_empty_request, sizeof(expected_empty_request)
    );

    failures += run_test(
        "Tester Present normal response",
        tester_present, sizeof(tester_present),
        expected_tester_present, sizeof(expected_tester_present)
    );

    failures += run_test(
        "Tester Present suppress positive response",
        tester_present_suppress, sizeof(tester_present_suppress),
        expected_tester_present_suppress, 0
    );

    failures += run_test(
        "Tester Present unsupported subfunction",
        tester_present_unsupported, sizeof(tester_present_unsupported),
        expected_tester_present_unsupported,
        sizeof(expected_tester_present_unsupported)
    );

    failures += run_test(
        "Tester Present invalid request length",
        tester_present_invalid, sizeof(tester_present_invalid),
        expected_tester_present_invalid,
        sizeof(expected_tester_present_invalid)
    );

    failures += test_insufficient_capacity();
    failures += test_session_timeout();

    printf("------------------------------------\n");
    printf("Failures: %d\n", failures);
    printf("%s\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED");
    printf("====================================\n");

    return failures == 0 ? 0 : 1;
}
