#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "uds_protocol.h"
#include "uds_session.h"
#include "uds_security.h"
#include "dtc_manager.h"
#include "did_manager.h"

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

    /* Communication Control requires Extended Diagnostic Session. */
    failures += run_test(
        "Communication Control rejected in Default Session",
        wrong_session, sizeof(wrong_session),
        expected_wrong_session, sizeof(expected_wrong_session)
    );

    if (uds_communication_control_get() != 0x00)
        failures++;

    /* Enter Extended Session for the remaining tests. */
    if (uds_session_set(UDS_SESSION_EXTENDED) != 0) {
        printf("[FAIL] Enter Extended Session for Communication Control tests\n");
        failures++;
    }

    /* Test all four supported communication-control modes. */
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

    /* Suppress the positive response while applying control type 0x02. */
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

    /* A response buffer smaller than three bytes must be rejected. */
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


static int test_control_dtc_setting(void)
{
    int failures = 0;

    const uint8_t disable[] = {0x85, 0x02};
    const uint8_t enable[] = {0x85, 0x01};
    const uint8_t invalid_subfunction[] = {0x85, 0x03};
    const uint8_t invalid_length[] = {0x85};
    const uint8_t suppressed_disable[] = {0x85, 0x82};
    const uint8_t suppressed_enable[] = {0x85, 0x81};

    const uint8_t expected_disable[] = {0xC5, 0x02};
    const uint8_t expected_enable[] = {0xC5, 0x01};
    const uint8_t expected_unsupported[] = {0x7F, 0x85, 0x12};
    const uint8_t expected_length[] = {0x7F, 0x85, 0x13};

    printf("\n===== CONTROL DTC SETTING TESTS =====\n");

    int passed = dtc_manager_is_setting_enabled();
    printf("[%s] DTC setting enabled by default\n",
           passed ? "PASS" : "FAIL");
    failures += passed ? 0 : 1;

    failures += run_test("Disable DTC setting",
                         disable, sizeof(disable),
                         expected_disable, sizeof(expected_disable));

    passed = !dtc_manager_is_setting_enabled();
    printf("[%s] DTC setting state becomes disabled\n",
           passed ? "PASS" : "FAIL");
    failures += passed ? 0 : 1;

    struct dtc_record records[DTC_MANAGER_MAX_RECORDS];
    size_t count = dtc_manager_get_by_status_mask(
        0xFF, records, DTC_MANAGER_MAX_RECORDS);

    passed = count == 3;
    printf("[%s] Existing DTC records remain while setting is disabled\n",
           passed ? "PASS" : "FAIL");
    failures += passed ? 0 : 1;

    failures += run_test("Enable DTC setting",
                         enable, sizeof(enable),
                         expected_enable, sizeof(expected_enable));

    passed = dtc_manager_is_setting_enabled();
    printf("[%s] DTC setting state becomes enabled\n",
           passed ? "PASS" : "FAIL");
    failures += passed ? 0 : 1;

    failures += run_test("Unsupported ControlDTCSetting subfunction",
                         invalid_subfunction, sizeof(invalid_subfunction),
                         expected_unsupported, sizeof(expected_unsupported));

    failures += run_test("Invalid ControlDTCSetting request length",
                         invalid_length, sizeof(invalid_length),
                         expected_length, sizeof(expected_length));

    passed = dtc_manager_is_setting_enabled();
    printf("[%s] Invalid requests preserve the enabled state\n",
           passed ? "PASS" : "FAIL");
    failures += passed ? 0 : 1;

    uint8_t response[2] = {0xAA, 0xAA};
    size_t response_len = 99;

    int result = uds_dispatch_request(
        disable, sizeof(disable), response, 1, &response_len);

    passed = result == -1 && response_len == 0 &&
             response[0] == 0xAA && response[1] == 0xAA &&
             dtc_manager_is_setting_enabled();

    printf("[%s] Insufficient response capacity preserves state\n",
           passed ? "PASS" : "FAIL");
    failures += passed ? 0 : 1;

    result = uds_dispatch_request(
        suppressed_disable, sizeof(suppressed_disable),
        response, sizeof(response), &response_len);

    passed = result == 0 && response_len == 0 &&
             !dtc_manager_is_setting_enabled();

    printf("[%s] Suppressed response while disabling\n",
           passed ? "PASS" : "FAIL");
    failures += passed ? 0 : 1;

    result = uds_dispatch_request(
        suppressed_enable, sizeof(suppressed_enable),
        response, sizeof(response), &response_len);

    passed = result == 0 && response_len == 0 &&
             dtc_manager_is_setting_enabled();

    printf("[%s] Suppressed response while enabling\n",
           passed ? "PASS" : "FAIL");
    failures += passed ? 0 : 1;

    return failures;
}



static int test_persistent_write_failure(
    const uint8_t *request,
    size_t request_len)
{
    int failures = 0;
    int directory_moved = 0;
    int blocker_created = 0;
    int initialized = 0;

    char directory[] = "/tmp/autodiag-uds-write-failure-XXXXXX";
    char backup_directory[256];
    char storage_path[512];

    const uint8_t expected_original[DID_WRITABLE_DATA_LENGTH] =
        "AUTODIAG-CAL-001";

    const uint8_t expected_nrc[] = {0x7F, 0x2E, 0x72};

    uint8_t response[64] = {0};
    uint8_t readback[DID_WRITABLE_DATA_LENGTH];
    size_t response_len = 0;
    size_t readback_len = 0;

    printf("\n===== UDS PERSISTENCE FAILURE TEST =====\n");

    if (!mkdtemp(directory)) {
        perror("Creating temporary test directory");
        return 1;
    }

    if (snprintf(backup_directory, sizeof(backup_directory),
                 "%s.backup", directory) >=
        (int)sizeof(backup_directory) ||
        snprintf(storage_path, sizeof(storage_path),
                 "%s/calibration-f1a1.bin", directory) >=
        (int)sizeof(storage_path)) {
        printf("[FAIL] Test storage path is too long\n");
        failures++;
        goto cleanup;
    }

    if (did_manager_init(storage_path) != 0) {
        perror("Initializing persistent DID manager");
        failures++;
        goto cleanup;
    }

    initialized = 1;

    if (rename(directory, backup_directory) != 0) {
        perror("Temporarily moving storage directory");
        failures++;
        goto cleanup;
    }

    directory_moved = 1;

    int fd = open(directory, O_CREAT | O_EXCL | O_WRONLY, 0600);

    if (fd < 0) {
        perror("Creating storage-path blocker");
        failures++;
        goto cleanup;
    }

    close(fd);
    blocker_created = 1;

    int result = uds_dispatch_request(
        request, request_len,
        response, sizeof(response),
        &response_len
    );

    if (result == 0 &&
        response_len == sizeof(expected_nrc) &&
        memcmp(response, expected_nrc, sizeof(expected_nrc)) == 0) {
        printf("[PASS] UDS returns NRC 0x72 on storage failure\n");
    } else {
        printf("[FAIL] UDS did not return the expected NRC 0x72\n");
        failures++;
    }

    if (did_manager_read(
            DID_WRITABLE_CALIBRATION,
            readback,
            sizeof(readback),
            &readback_len) == 0 &&
        readback_len == DID_WRITABLE_DATA_LENGTH &&
        memcmp(readback, expected_original, sizeof(readback)) == 0) {
        printf("[PASS] Failed UDS write preserves in-memory calibration\n");
    } else {
        printf("[FAIL] Failed UDS write changed in-memory calibration\n");
        failures++;
    }

cleanup:
    if (blocker_created && unlink(directory) != 0) {
        perror("Removing storage-path blocker");
        failures++;
    }

    if (directory_moved) {
        if (rename(backup_directory, directory) != 0) {
            perror("Restoring storage directory");
            failures++;
            directory_moved = 0;
        } else {
            directory_moved = 0;

            if (did_manager_init(storage_path) != 0) {
                perror("Reloading restored persistent calibration");
                failures++;
            } else if (did_manager_read(
                           DID_WRITABLE_CALIBRATION,
                           readback,
                           sizeof(readback),
                           &readback_len) == 0 &&
                       readback_len == DID_WRITABLE_DATA_LENGTH &&
                       memcmp(readback, expected_original,
                              sizeof(readback)) == 0) {
                printf("[PASS] Original calibration survives storage recovery\n");
            } else {
                printf("[FAIL] Original calibration changed on disk\n");
                failures++;
            }
        }
    }

    if (initialized)
        unlink(storage_path);

    if (!directory_moved)
        rmdir(directory);

    if (directory_moved)
        rmdir(backup_directory);

    did_manager_reset();

    return failures;
}

static int test_write_data_by_identifier(void)
{
    int failures = 0;

    const uint8_t write_request[] = {
        0x2E, 0xF1, 0xA1,
        'N', 'E', 'W', '-', 'C', 'A', 'L', '-',
        'D', 'A', 'T', 'A', '-', '0', '0', '1'
    };

    const uint8_t read_request[] = {0x22, 0xF1, 0xA1};

    const uint8_t denied_default[] = {0x7F, 0x2E, 0x7E};
    const uint8_t denied_locked[] = {0x7F, 0x2E, 0x33};
    const uint8_t write_response[] = {0x6E, 0xF1, 0xA1};

    const uint8_t read_response[] = {
        0x62, 0xF1, 0xA1,
        'N', 'E', 'W', '-', 'C', 'A', 'L', '-',
        'D', 'A', 'T', 'A', '-', '0', '0', '1'
    };

    const uint8_t invalid_length[18] = {
        0x2E, 0xF1, 0xA1,
        'N', 'E', 'W', '-', 'C', 'A', 'L', '-',
        'D', 'A', 'T', 'A', '-', '0', '0'
    };

    const uint8_t invalid_length_response[] = {
        0x7F, 0x2E, 0x13
    };

    const uint8_t unsupported_did[19] = {
        0x2E, 0xF1, 0xA2,
        'N', 'E', 'W', '-', 'C', 'A', 'L', '-',
        'D', 'A', 'T', 'A', '-', '0', '0', '1'
    };

    const uint8_t unsupported_did_response[] = {
        0x7F, 0x2E, 0x31
    };

    printf("\n===== WRITE DATA BY IDENTIFIER TESTS =====\n");

    did_manager_reset();
    uds_session_reset();
    uds_security_reset();

    failures += run_test(
        "Write DID denied in Default Session",
        write_request, sizeof(write_request),
        denied_default, sizeof(denied_default)
    );

    if (uds_session_set(UDS_SESSION_EXTENDED) != 0) {
        printf("[FAIL] Enter Extended Session for WriteDataByIdentifier\n");
        return failures + 1;
    }

    failures += run_test(
        "Write DID denied while SecurityAccess is locked",
        write_request, sizeof(write_request),
        denied_locked, sizeof(denied_locked)
    );

    uint32_t seed = uds_security_generate_seed();
    uint32_t key = uds_security_calculate_key(seed);

    if (uds_security_validate_key(key) != 0) {
        printf("[FAIL] Unlock SecurityAccess for WriteDataByIdentifier\n");
        return failures + 1;
    }

    failures += test_persistent_write_failure(
        write_request, sizeof(write_request)
    );

    failures += run_test(
        "Write writable calibration DID",
        write_request, sizeof(write_request),
        write_response, sizeof(write_response)
    );

    failures += run_test(
        "Read back updated calibration DID",
        read_request, sizeof(read_request),
        read_response, sizeof(read_response)
    );

    failures += run_test(
        "Reject incorrect WriteDataByIdentifier length",
        invalid_length, sizeof(invalid_length),
        invalid_length_response, sizeof(invalid_length_response)
    );

    failures += run_test(
        "Reject unsupported writable DID",
        unsupported_did, sizeof(unsupported_did),
        unsupported_did_response, sizeof(unsupported_did_response)
    );

    if (uds_session_set(UDS_SESSION_DEFAULT) != 0) {
        printf("[FAIL] Return to Default Session after write tests\n");
        return failures + 1;
    }

    uds_security_reset();

    failures += run_test(
        "Write DID denied again in Default Session",
        write_request, sizeof(write_request),
        denied_default, sizeof(denied_default)
    );

    return failures;
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
    failures += test_control_dtc_setting();
    failures += test_write_data_by_identifier();

    failures += test_communication_control();
    failures += test_ecu_reset();
    failures += test_session_timeout();

    printf("------------------------------------\n");
    printf("Failures: %d\n", failures);
    printf("%s\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED");
    printf("====================================\n");

    return failures == 0 ? 0 : 1;
}
