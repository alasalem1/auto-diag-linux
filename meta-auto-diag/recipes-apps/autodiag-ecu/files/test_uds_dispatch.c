#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#include "uds_protocol.h"
#include "uds_session.h"
#include "uds_security.h"
#include "dtc_manager.h"

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

static int run_session_test(
    const char *name,
    const uint8_t *request,
    size_t request_len,
    const uint8_t *expected,
    size_t expected_len,
    uint8_t expected_session)
{
    uint8_t response[64] = {0};
    size_t response_len = 0;

    int result = uds_dispatch_request(
        request, request_len,
        response, sizeof(response),
        &response_len
    );

    int response_ok = result == 0 &&
                      response_len == expected_len &&
                      memcmp(response, expected, expected_len) == 0;

    int state_ok = uds_session_get() == expected_session;

    int passed = response_ok && state_ok;

    printf("[%s] %s\n", passed ? "PASS" : "FAIL", name);

    if (!passed) {
        printf("  Return: %d, response length: %zu\n",
               result, response_len);
        printf("  Expected session: 0x%02X, actual: 0x%02X\n",
               expected_session, uds_session_get());
    }

    return passed ? 0 : 1;
}

static int test_session_insufficient_capacity(void)
{
    const uint8_t request[] = {0x10, 0x03};
    uint8_t response[5];
    size_t response_len = 99;

    memset(response, 0xAA, sizeof(response));
    uds_session_reset();

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
                 unchanged &&
                 uds_session_get() == UDS_SESSION_DEFAULT;

    printf("[%s] Session response buffer too small\n",
           passed ? "PASS" : "FAIL");

    return passed ? 0 : 1;
}

static int test_insufficient_vin_capacity(void)
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

    printf("[%s] VIN response buffer too small\n",
           passed ? "PASS" : "FAIL");

    return passed ? 0 : 1;
}



static int test_protected_did_access(void)
{
    int failures = 0;

    const uint8_t default_session[] = {0x10, 0x01};
    const uint8_t extended_session[] = {0x10, 0x03};
    const uint8_t protected_did[] = {0x22, 0xF1, 0xA0};
    const uint8_t seed_request[] = {0x27, 0x01};
    const uint8_t correct_key[] = {
        0x27, 0x02, 0xB7, 0xF7, 0xC8, 0x09
    };

    const uint8_t denied_default[] = {0x7F, 0x22, 0x7E};
    const uint8_t denied_locked[] = {0x7F, 0x22, 0x33};
    const uint8_t seed_response[] = {
        0x67, 0x01, 0x12, 0x34, 0x56, 0x78
    };
    const uint8_t unlock_response[] = {0x67, 0x02};
    const uint8_t calibration_response[] = {
        0x62, 0xF1, 0xA0,
        'A', 'U', 'T', 'O', 'D', 'I', 'A', 'G',
        '-', 'C', 'A', 'L', '-', '0', '0', '1'
    };

    printf("\n===== PROTECTED DID ACCESS TESTS =====\n");

    uds_session_reset();
    uds_security_reset();

    failures += run_test(
        "Protected DID denied in default session",
        protected_did, sizeof(protected_did),
        denied_default, sizeof(denied_default)
    );

    failures += run_session_test(
        "Enter extended session for protected DID",
        extended_session, sizeof(extended_session),
        (const uint8_t[]){0x50, 0x03, 0x00, 0x32, 0x01, 0xF4},
        6,
        UDS_SESSION_EXTENDED
    );

    failures += run_test(
        "Protected DID denied while SecurityAccess is locked",
        protected_did, sizeof(protected_did),
        denied_locked, sizeof(denied_locked)
    );

    failures += run_test(
        "Request seed for protected DID access",
        seed_request, sizeof(seed_request),
        seed_response, sizeof(seed_response)
    );

    failures += run_test(
        "Unlock SecurityAccess for protected DID",
        correct_key, sizeof(correct_key),
        unlock_response, sizeof(unlock_response)
    );

    failures += run_test(
        "Protected DID accessible after successful unlock",
        protected_did, sizeof(protected_did),
        calibration_response, sizeof(calibration_response)
    );

    failures += run_session_test(
        "Return to default session after protected access",
        default_session, sizeof(default_session),
        (const uint8_t[]){0x50, 0x01, 0x00, 0x32, 0x01, 0xF4},
        6,
        UDS_SESSION_DEFAULT
    );

    failures += run_test(
        "Protected DID denied again after session reset",
        protected_did, sizeof(protected_did),
        denied_default, sizeof(denied_default)
    );

    return failures;
}

static int test_security_access_sequence(void)
{
    int failures = 0;

    const uint8_t default_session[] = {0x10, 0x01};
    const uint8_t extended_session[] = {0x10, 0x03};

    const uint8_t seed_request[] = {0x27, 0x01};
    const uint8_t key_before_seed[] = {
        0x27, 0x02, 0xB7, 0xF7, 0xC8, 0x09
    };
    const uint8_t wrong_key[] = {
        0x27, 0x02, 0x00, 0x00, 0x00, 0x00
    };
    const uint8_t correct_key[] = {
        0x27, 0x02, 0xB7, 0xF7, 0xC8, 0x09
    };

    const uint8_t denied_default[] = {0x7F, 0x27, 0x7E};
    const uint8_t sequence_error[] = {0x7F, 0x27, 0x24};
    const uint8_t expected_seed[] = {
        0x67, 0x01, 0x12, 0x34, 0x56, 0x78
    };
    const uint8_t invalid_key[] = {0x7F, 0x27, 0x35};
    const uint8_t unlocked_response[] = {0x67, 0x02};

    printf("\n===== SECURITY ACCESS TESTS =====\n");

    uds_session_reset();
    uds_security_reset();

    failures += run_test(
        "SecurityAccess denied in default session",
        seed_request, sizeof(seed_request),
        denied_default, sizeof(denied_default)
    );

    failures += run_session_test(
        "Enter extended session for SecurityAccess",
        extended_session, sizeof(extended_session),
        (const uint8_t[]){0x50, 0x03, 0x00, 0x32, 0x01, 0xF4},
        6,
        UDS_SESSION_EXTENDED
    );

    failures += run_test(
        "Key before seed returns sequence error",
        key_before_seed, sizeof(key_before_seed),
        sequence_error, sizeof(sequence_error)
    );

    failures += run_test(
        "Request SecurityAccess seed",
        seed_request, sizeof(seed_request),
        expected_seed, sizeof(expected_seed)
    );

    failures += run_test(
        "Incorrect key is rejected",
        wrong_key, sizeof(wrong_key),
        invalid_key, sizeof(invalid_key)
    );

    failures += run_test(
        "Request a new seed after incorrect key",
        seed_request, sizeof(seed_request),
        expected_seed, sizeof(expected_seed)
    );

    failures += run_test(
        "Correct key unlocks SecurityAccess",
        correct_key, sizeof(correct_key),
        unlocked_response, sizeof(unlocked_response)
    );

    int unlocked = uds_security_is_unlocked();
    printf("[%s] Security state is unlocked after valid key\n",
           unlocked ? "PASS" : "FAIL");
    failures += unlocked ? 0 : 1;

    failures += run_session_test(
        "Return to default session after unlocking",
        default_session, sizeof(default_session),
        (const uint8_t[]){0x50, 0x01, 0x00, 0x32, 0x01, 0xF4},
        6,
        UDS_SESSION_DEFAULT
    );

    int locked = !uds_security_is_unlocked();
    printf("[%s] Session change resets security state\n",
           locked ? "PASS" : "FAIL");
    failures += locked ? 0 : 1;

    return failures;
}



static int test_control_dtc_setting(void)
{
    int failures = 0;

    const uint8_t disable[] = {0x85, 0x02};
    const uint8_t enable[] = {0x85, 0x01};
    const uint8_t invalid_subfunction[] = {0x85, 0x03};
    const uint8_t invalid_length[] = {0x85};
    const uint8_t suppressed_disable[] = {0x85, 0x82};
    const uint8_t suppressed_enable[] = {0x85, 0x81};
    const uint8_t read_all[] = {0x19, 0x02, 0xFF};

    const uint8_t expected_disable[] = {0xC5, 0x02};
    const uint8_t expected_enable[] = {0xC5, 0x01};
    const uint8_t expected_invalid_subfunction[] = {
        0x7F, 0x85, 0x12
    };
    const uint8_t expected_invalid_length[] = {
        0x7F, 0x85, 0x13
    };
    const uint8_t expected_all_dtcs[] = {
        0x59, 0x02, 0xFF,
        0x12, 0x34, 0x56, 0x09,
        0x23, 0x45, 0x67, 0x0B,
        0x34, 0x56, 0x78, 0x01
    };

    printf("\n===== CONTROL DTC SETTING TESTS =====\n");

    int initially_enabled = dtc_manager_is_setting_enabled();
    printf("[%s] DTC setting enabled by default\n",
           initially_enabled ? "PASS" : "FAIL");
    failures += initially_enabled ? 0 : 1;

    failures += run_test(
        "Disable DTC setting",
        disable, sizeof(disable),
        expected_disable, sizeof(expected_disable)
    );

    int disabled = !dtc_manager_is_setting_enabled();
    printf("[%s] DTC manager state becomes disabled\n",
           disabled ? "PASS" : "FAIL");
    failures += disabled ? 0 : 1;

    failures += run_test(
        "Existing DTCs remain readable while setting is disabled",
        read_all, sizeof(read_all),
        expected_all_dtcs, sizeof(expected_all_dtcs)
    );

    failures += run_test(
        "Enable DTC setting",
        enable, sizeof(enable),
        expected_enable, sizeof(expected_enable)
    );

    int enabled = dtc_manager_is_setting_enabled();
    printf("[%s] DTC manager state becomes enabled\n",
           enabled ? "PASS" : "FAIL");
    failures += enabled ? 0 : 1;

    failures += run_test(
        "Unsupported ControlDTCSetting subfunction",
        invalid_subfunction, sizeof(invalid_subfunction),
        expected_invalid_subfunction,
        sizeof(expected_invalid_subfunction)
    );

    failures += run_test(
        "Invalid ControlDTCSetting request length",
        invalid_length, sizeof(invalid_length),
        expected_invalid_length, sizeof(expected_invalid_length)
    );

    int state_unchanged = dtc_manager_is_setting_enabled();
    printf("[%s] Invalid requests do not change DTC setting state\n",
           state_unchanged ? "PASS" : "FAIL");
    failures += state_unchanged ? 0 : 1;

    uint8_t response[2] = {0xAA, 0xAA};
    size_t response_len = 99;

    int result = uds_dispatch_request(
        disable, sizeof(disable),
        response, 1,
        &response_len
    );

    int capacity_passed =
        result == -1 &&
        response_len == 0 &&
        response[0] == 0xAA &&
        response[1] == 0xAA &&
        dtc_manager_is_setting_enabled();

    printf("[%s] Insufficient response capacity preserves state\n",
           capacity_passed ? "PASS" : "FAIL");
    failures += capacity_passed ? 0 : 1;

    result = uds_dispatch_request(
        suppressed_disable, sizeof(suppressed_disable),
        response, sizeof(response),
        &response_len
    );

    int suppress_disable_passed =
        result == 0 &&
        response_len == 0 &&
        !dtc_manager_is_setting_enabled();

    printf("[%s] Suppress positive response while disabling\n",
           suppress_disable_passed ? "PASS" : "FAIL");
    failures += suppress_disable_passed ? 0 : 1;

    result = uds_dispatch_request(
        suppressed_enable, sizeof(suppressed_enable),
        response, sizeof(response),
        &response_len
    );

    int suppress_enable_passed =
        result == 0 &&
        response_len == 0 &&
        dtc_manager_is_setting_enabled();

    printf("[%s] Suppress positive response while enabling\n",
           suppress_enable_passed ? "PASS" : "FAIL");
    failures += suppress_enable_passed ? 0 : 1;

    return failures;
}

static int test_read_dtc_information(void)
{
    int failures = 0;

    const uint8_t all_dtcs[] = {0x19, 0x02, 0xFF};
    const uint8_t confirmed_dtcs[] = {0x19, 0x02, 0x08};
    const uint8_t no_matching_dtcs[] = {0x19, 0x02, 0x04};
    const uint8_t unsupported_subfunction[] = {0x19, 0x01, 0xFF};
    const uint8_t invalid_length[] = {0x19, 0x02};
    const uint8_t insufficient_capacity[] = {0x19, 0x02, 0xFF};

    const uint8_t expected_all[] = {
        0x59, 0x02, 0xFF,
        0x12, 0x34, 0x56, 0x09,
        0x23, 0x45, 0x67, 0x0B,
        0x34, 0x56, 0x78, 0x01
    };

    const uint8_t expected_confirmed[] = {
        0x59, 0x02, 0xFF,
        0x12, 0x34, 0x56, 0x09,
        0x23, 0x45, 0x67, 0x0B
    };

    const uint8_t expected_empty[] = {
        0x59, 0x02, 0xFF
    };

    const uint8_t expected_unsupported[] = {
        0x7F, 0x19, 0x12
    };

    const uint8_t expected_invalid_length[] = {
        0x7F, 0x19, 0x13
    };

    printf("\n===== READ DTC INFORMATION TESTS =====\n");

    failures += run_test(
        "Read all DTCs with status mask FF",
        all_dtcs, sizeof(all_dtcs),
        expected_all, sizeof(expected_all)
    );

    failures += run_test(
        "Filter confirmed DTCs with status mask 08",
        confirmed_dtcs, sizeof(confirmed_dtcs),
        expected_confirmed, sizeof(expected_confirmed)
    );

    failures += run_test(
        "No matching DTCs with status mask 04",
        no_matching_dtcs, sizeof(no_matching_dtcs),
        expected_empty, sizeof(expected_empty)
    );

    failures += run_test(
        "Unsupported ReadDTCInformation subfunction",
        unsupported_subfunction, sizeof(unsupported_subfunction),
        expected_unsupported, sizeof(expected_unsupported)
    );

    failures += run_test(
        "Invalid ReadDTCInformation request length",
        invalid_length, sizeof(invalid_length),
        expected_invalid_length, sizeof(expected_invalid_length)
    );

    uint8_t response[14];
    size_t response_len = 99;

    memset(response, 0xAA, sizeof(response));

    int result = uds_dispatch_request(
        insufficient_capacity,
        sizeof(insufficient_capacity),
        response,
        sizeof(response),
        &response_len
    );

    int unchanged = 1;

    for (size_t i = 0; i < sizeof(response); i++) {
        if (response[i] != 0xAA)
            unchanged = 0;
    }

    int capacity_passed = result == -1 &&
                          response_len == 0 &&
                          unchanged;

    printf("[%s] DTC response buffer too small\n",
           capacity_passed ? "PASS" : "FAIL");

    failures += capacity_passed ? 0 : 1;

    return failures;
}


static int test_clear_diagnostic_information(void)
{
    int failures = 0;

    const uint8_t clear_all[] = {
        0x14, 0xFF, 0xFF, 0xFF
    };
    const uint8_t expected_clear[] = {0x54};

    const uint8_t read_all[] = {
        0x19, 0x02, 0xFF
    };
    const uint8_t expected_empty[] = {
        0x59, 0x02, 0xFF
    };

    const uint8_t unsupported_group[] = {
        0x14, 0x00, 0x00, 0x00
    };
    const uint8_t expected_unsupported[] = {
        0x7F, 0x14, 0x31
    };

    const uint8_t invalid_length[] = {
        0x14, 0xFF, 0xFF
    };
    const uint8_t expected_invalid[] = {
        0x7F, 0x14, 0x13
    };

    printf("\n===== CLEAR DIAGNOSTIC INFORMATION TESTS =====\n");

    /*
     * Insufficient capacity must not clear the database.
     */
    uint8_t response[3] = {0xAA, 0xAA, 0xAA};
    size_t response_len = 99;

    int result = uds_dispatch_request(
        clear_all,
        sizeof(clear_all),
        response,
        0,
        &response_len
    );

    int unchanged = response[0] == 0xAA &&
                    response[1] == 0xAA &&
                    response[2] == 0xAA;

    int capacity_passed = result == -1 &&
                          response_len == 0 &&
                          unchanged;

    printf("[%s] Clear response buffer too small does not clear DTCs\n",
           capacity_passed ? "PASS" : "FAIL");
    failures += capacity_passed ? 0 : 1;

    /*
     * Verify that all three original DTC records remain present.
     */
    const uint8_t expected_all[] = {
        0x59, 0x02, 0xFF,
        0x12, 0x34, 0x56, 0x09,
        0x23, 0x45, 0x67, 0x0B,
        0x34, 0x56, 0x78, 0x01
    };

    failures += run_test(
        "DTC database unchanged after insufficient clear response buffer",
        read_all, sizeof(read_all),
        expected_all, sizeof(expected_all)
    );

    failures += run_test(
        "Clear all DTCs with group FFFFFF",
        clear_all, sizeof(clear_all),
        expected_clear, sizeof(expected_clear)
    );

    failures += run_test(
        "ReadDTCInformation returns empty database after clearing",
        read_all, sizeof(read_all),
        expected_empty, sizeof(expected_empty)
    );

    failures += run_test(
        "Unsupported ClearDiagnosticInformation group",
        unsupported_group, sizeof(unsupported_group),
        expected_unsupported, sizeof(expected_unsupported)
    );

    failures += run_test(
        "Invalid ClearDiagnosticInformation request length",
        invalid_length, sizeof(invalid_length),
        expected_invalid, sizeof(expected_invalid)
    );

    return failures;
}

static int test_tester_present(void)
{
    int failures = 0;

    const uint8_t normal[] = {0x3E, 0x00};
    const uint8_t suppress[] = {0x3E, 0x80};
    const uint8_t unsupported[] = {0x3E, 0x01};
    const uint8_t invalid_length[] = {0x3E};

    const uint8_t expected_normal[] = {0x7E, 0x00};
    const uint8_t expected_suppress[] = {0x00};
    const uint8_t expected_unsupported[] = {0x7F, 0x3E, 0x12};
    const uint8_t expected_invalid_length[] = {0x7F, 0x3E, 0x13};

    failures += run_test(
        "Tester Present normal response",
        normal, sizeof(normal),
        expected_normal, sizeof(expected_normal)
    );

    failures += run_test(
        "Tester Present suppress positive response",
        suppress, sizeof(suppress),
        expected_suppress, 0
    );

    failures += run_test(
        "Tester Present unsupported subfunction",
        unsupported, sizeof(unsupported),
        expected_unsupported, sizeof(expected_unsupported)
    );

    failures += run_test(
        "Tester Present invalid request length",
        invalid_length, sizeof(invalid_length),
        expected_invalid_length, sizeof(expected_invalid_length)
    );

    return failures;
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

    /* Set Communication Control before the session expires. */
    {
        const uint8_t request[] = {0x28, 0x03, 0x01};
        uint8_t response[3];
        size_t response_len = 0;

        if (uds_dispatch_request(
                request, sizeof(request),
                response, sizeof(response),
                &response_len) != 0 ||
            uds_communication_control_get() != 0x03)
            passed = 0;
    }

    /* The session should expire after inactivity. */
    sleep_ms(150);

    if (uds_session_check_timeout() != 1)
        passed = 0;

    if (uds_session_get() != UDS_SESSION_DEFAULT)
        passed = 0;

    if (uds_security_is_unlocked())
        passed = 0;

    if (uds_communication_control_get() != 0x00)
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


static int test_communication_control(void)
{
    int failures = 0;

    const uint8_t wrong_session[] = {0x28, 0x00, 0x01};
    const uint8_t expected_wrong_session[] = {0x7F, 0x28, 0x7E};

    const uint8_t invalid_control[] = {0x28, 0x04, 0x01};
    const uint8_t expected_invalid_control[] = {0x7F, 0x28, 0x12};

    const uint8_t invalid_communication[] = {0x28, 0x00, 0x00};
    const uint8_t expected_invalid_communication[] = {
        0x7F, 0x28, 0x31
    };

    const uint8_t invalid_length[] = {0x28, 0x00};
    const uint8_t expected_invalid_length[] = {0x7F, 0x28, 0x13};
    const uint8_t suppressed_expected[] = {0x00};

    uds_session_reset();
    uds_communication_control_reset();

    failures += run_test(
        "Communication Control rejected in Default Session",
        wrong_session, sizeof(wrong_session),
        expected_wrong_session, sizeof(expected_wrong_session)
    );

    if (uds_communication_control_get() != 0x00)
        failures++;

    if (uds_session_set(UDS_SESSION_EXTENDED) != 0) {
        printf("[FAIL] Enter Extended Session for Communication Control tests\n");
        failures++;
    }

    for (uint8_t control = 0x00; control <= 0x03; control++) {
        uint8_t request[] = {0x28, control, 0x01};
        uint8_t expected[] = {0x68, control, 0x01};

        failures += run_test(
            "Communication Control supported mode",
            request, sizeof(request),
            expected, sizeof(expected)
        );

        if (uds_communication_control_get() != control) {
            printf("[FAIL] Communication Control state for mode 0x%02X\n",
                   control);
            failures++;
        }
    }

    const uint8_t suppress_response[] = {0x28, 0x82, 0x01};

    failures += run_test(
        "Communication Control suppress positive response",
        suppress_response, sizeof(suppress_response),
        suppressed_expected, 0
    );

    if (uds_communication_control_get() != 0x02) {
        printf("[FAIL] Suppressed request did not update communication state\n");
        failures++;
    }

    failures += run_test(
        "Communication Control unsupported control type",
        invalid_control, sizeof(invalid_control),
        expected_invalid_control, sizeof(expected_invalid_control)
    );

    if (uds_communication_control_get() != 0x02)
        failures++;

    failures += run_test(
        "Communication Control unsupported communication type",
        invalid_communication, sizeof(invalid_communication),
        expected_invalid_communication,
        sizeof(expected_invalid_communication)
    );

    if (uds_communication_control_get() != 0x02)
        failures++;

    failures += run_test(
        "Communication Control invalid request length",
        invalid_length, sizeof(invalid_length),
        expected_invalid_length, sizeof(expected_invalid_length)
    );

    if (uds_communication_control_get() != 0x02)
        failures++;

    {
        const uint8_t request[] = {0x28, 0x00, 0x01};
        uint8_t response[2] = {0xAA, 0xAA};
        size_t response_len = 99;

        int result = uds_dispatch_request(
            request, sizeof(request),
            response, sizeof(response),
            &response_len
        );

        int passed = result == -1 &&
                     response_len == 0 &&
                     uds_communication_control_get() == 0x02;

        printf("[%s] Communication Control insufficient response capacity\n",
               passed ? "PASS" : "FAIL");

        if (!passed)
            failures++;
    }

    /* Leaving Extended Session must restore normal communication. */
    {
        const uint8_t default_session[] = {0x10, UDS_SESSION_DEFAULT};
        const uint8_t expected_default_session[] = {
            0x50, UDS_SESSION_DEFAULT, 0x00, 0x32, 0x01, 0xF4
        };

        const uint8_t set_control[] = {0x28, 0x02, 0x01};
        uint8_t response[3];
        size_t response_len = 0;

        if (uds_dispatch_request(
                set_control, sizeof(set_control),
                response, sizeof(response),
                &response_len) != 0 ||
            uds_communication_control_get() != 0x02) {
            failures++;
        }

        failures += run_test(
            "Communication Control reset on Default Session transition",
            default_session, sizeof(default_session),
            expected_default_session, sizeof(expected_default_session)
        );

        if (uds_communication_control_get() != 0x00) {
            printf("[FAIL] Communication Control was not reset on session transition\n");
            failures++;
        }
    }

    uds_communication_control_reset();
    uds_session_reset();

    return failures;
}

static int test_ecu_reset(void)
{
    int failures = 0;

    const uint8_t hard_reset[] = {0x11, 0x01};
    const uint8_t key_off_on_reset[] = {0x11, 0x02};
    const uint8_t soft_reset[] = {0x11, 0x03};
    const uint8_t unsupported_reset[] = {0x11, 0x04};
    const uint8_t invalid_length[] = {0x11};
    const uint8_t suppress_reset[] = {0x11, 0x81};

    const uint8_t expected_hard[] = {0x51, 0x01};
    const uint8_t expected_key_off_on[] = {0x51, 0x02};
    const uint8_t expected_soft[] = {0x51, 0x03};
    const uint8_t expected_unsupported[] = {0x7F, 0x11, 0x12};
    const uint8_t expected_invalid[] = {0x7F, 0x11, 0x13};

    uds_ecu_reset_clear_pending();

    failures += run_test(
        "ECU Reset hard reset response",
        hard_reset, sizeof(hard_reset),
        expected_hard, sizeof(expected_hard)
    );

    if (uds_ecu_reset_get_pending() != 0x01) {
        printf("[FAIL] Hard Reset pending state\n");
        failures++;
    } else {
        printf("[PASS] Hard Reset pending state\n");
    }

    uds_ecu_reset_clear_pending();

    failures += run_test(
        "ECU Reset key off on reset response",
        key_off_on_reset, sizeof(key_off_on_reset),
        expected_key_off_on, sizeof(expected_key_off_on)
    );

    if (uds_ecu_reset_get_pending() != 0x02) {
        printf("[FAIL] Key Off On Reset pending state\n");
        failures++;
    } else {
        printf("[PASS] Key Off On Reset pending state\n");
    }

    uds_ecu_reset_clear_pending();

    failures += run_test(
        "ECU Reset soft reset response",
        soft_reset, sizeof(soft_reset),
        expected_soft, sizeof(expected_soft)
    );

    if (uds_ecu_reset_get_pending() != 0x03) {
        printf("[FAIL] Soft Reset pending state\n");
        failures++;
    } else {
        printf("[PASS] Soft Reset pending state\n");
    }

    uds_ecu_reset_clear_pending();

    failures += run_test(
        "ECU Reset unsupported reset type",
        unsupported_reset, sizeof(unsupported_reset),
        expected_unsupported, sizeof(expected_unsupported)
    );

    if (uds_ecu_reset_get_pending() != 0x00) {
        printf("[FAIL] Unsupported reset must not become pending\n");
        failures++;
    } else {
        printf("[PASS] Unsupported reset does not become pending\n");
    }

    failures += run_test(
        "ECU Reset invalid request length",
        invalid_length, sizeof(invalid_length),
        expected_invalid, sizeof(expected_invalid)
    );

    if (uds_ecu_reset_get_pending() != 0x00) {
        printf("[FAIL] Invalid request must not become pending\n");
        failures++;
    } else {
        printf("[PASS] Invalid request does not become pending\n");
    }

    failures += run_test(
        "ECU Reset suppress positive response",
        suppress_reset, sizeof(suppress_reset),
        expected_hard, 0
    );

    if (uds_ecu_reset_get_pending() != 0x01) {
        printf("[FAIL] Suppressed Hard Reset pending state\n");
        failures++;
    } else {
        printf("[PASS] Suppressed Hard Reset remains pending\n");
    }

    uds_ecu_reset_clear_pending();

    /* Insufficient capacity must not schedule a reset. */
    {
        const uint8_t request[] = {0x11, 0x01};
        uint8_t response[1] = {0xAA};
        size_t response_len = 99;

        int result = uds_dispatch_request(
            request, sizeof(request),
            response, sizeof(response),
            &response_len
        );

        int passed = result == -1 &&
                     response_len == 0 &&
                     response[0] == 0xAA &&
                     uds_ecu_reset_get_pending() == 0x00;

        printf("[%s] ECU Reset insufficient response capacity\n",
               passed ? "PASS" : "FAIL");

        if (!passed)
            failures++;
    }

    uds_ecu_reset_clear_pending();

    return failures;
}


int main(void)
{
    int failures = 0;

    const uint8_t valid_vin[] = {0x22, 0xF1, 0x90};
    const uint8_t unsupported_did[] = {0x22, 0xF1, 0x87};
    const uint8_t invalid_vin_length[] = {0x22, 0xF1};
    const uint8_t unsupported_service[] = {0x99, 0x01};
    const uint8_t empty_request[] = {0x00};

    const uint8_t default_session[] = {0x10, 0x01};
    const uint8_t extended_session[] = {0x10, 0x03};
    const uint8_t unsupported_session[] = {0x10, 0x02};
    const uint8_t invalid_session_length[] = {0x10};

    const uint8_t expected_vin[] = {
        0x62, 0xF1, 0x90,
        'A', 'U', 'T', 'O', 'D', 'I', 'A', 'G',
        '0', '0', '0', '0', '0', '0', '0', '0', '1'
    };

    const uint8_t expected_unsupported_did[] = {
        0x7F, 0x22, 0x31
    };

    const uint8_t expected_invalid_vin_length[] = {
        0x7F, 0x22, 0x13
    };

    const uint8_t expected_unsupported_service[] = {
        0x7F, 0x99, 0x11
    };

    const uint8_t expected_empty_request[] = {
        0x7F, 0x00, 0x13
    };

    const uint8_t expected_default_session[] = {
        0x50, 0x01, 0x00, 0x32, 0x01, 0xF4
    };

    const uint8_t expected_extended_session[] = {
        0x50, 0x03, 0x00, 0x32, 0x01, 0xF4
    };

    const uint8_t expected_unsupported_session[] = {
        0x7F, 0x10, 0x12
    };

    const uint8_t expected_invalid_session_length[] = {
        0x7F, 0x10, 0x13
    };

    const uint8_t expected_vin_denied_extended[] = {
        0x7F, 0x22, 0x7E
    };

    printf("====================================\n");
    printf("   AutoDiag UDS Dispatcher Tests\n");
    printf("====================================\n");

    /* Session state must start in the default session. */
    uds_session_reset();

    failures += run_session_test(
        "Initial session is default",
        default_session, sizeof(default_session),
        expected_default_session, sizeof(expected_default_session),
        UDS_SESSION_DEFAULT
    );

    failures += run_session_test(
        "Switch to extended session",
        extended_session, sizeof(extended_session),
        expected_extended_session, sizeof(expected_extended_session),
        UDS_SESSION_EXTENDED
    );

    failures += run_session_test(
        "Unsupported session leaves state unchanged",
        unsupported_session, sizeof(unsupported_session),
        expected_unsupported_session, sizeof(expected_unsupported_session),
        UDS_SESSION_EXTENDED
    );

    failures += run_session_test(
        "Invalid session request length leaves state unchanged",
        invalid_session_length, sizeof(invalid_session_length),
        expected_invalid_session_length,
        sizeof(expected_invalid_session_length),
        UDS_SESSION_EXTENDED
    );

    /* VIN access must be denied in the extended session. */
    failures += run_test(
        "VIN access denied in extended session",
        valid_vin, sizeof(valid_vin),
        expected_vin_denied_extended,
        sizeof(expected_vin_denied_extended)
    );

    /* Returning to default session must restore VIN access. */
    failures += run_session_test(
        "Return to default session",
        default_session, sizeof(default_session),
        expected_default_session, sizeof(expected_default_session),
        UDS_SESSION_DEFAULT
    );

    failures += test_session_insufficient_capacity();

    /* Existing ReadDataByIdentifier tests. */
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
        "Invalid VIN request length",
        invalid_vin_length, sizeof(invalid_vin_length),
        expected_invalid_vin_length,
        sizeof(expected_invalid_vin_length)
    );

    failures += run_test(
        "Unsupported service",
        unsupported_service, sizeof(unsupported_service),
        expected_unsupported_service,
        sizeof(expected_unsupported_service)
    );

    failures += run_test(
        "Empty request",
        empty_request, 0,
        expected_empty_request, sizeof(expected_empty_request)
    );

    failures += test_insufficient_vin_capacity();

    failures += test_security_access_sequence();
    failures += test_protected_did_access();
    failures += test_control_dtc_setting();
    failures += test_read_dtc_information();
    failures += test_clear_diagnostic_information();
    failures += test_tester_present();
    failures += test_communication_control();
    failures += test_ecu_reset();
    failures += test_session_timeout();

    printf("------------------------------------\n");
    printf("Failures: %d\n", failures);
    printf("%s\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED");
    printf("====================================\n");

    return failures == 0 ? 0 : 1;
}
