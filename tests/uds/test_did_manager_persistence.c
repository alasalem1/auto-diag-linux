#define _POSIX_C_SOURCE 200809L

#include "did_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    char directory[] = "/tmp/autodiag-did-test-XXXXXX";
    char storage_path[512];

    const uint8_t expected_default[DID_WRITABLE_DATA_LENGTH] =
        "AUTODIAG-CAL-001";

    const uint8_t updated_value[DID_WRITABLE_DATA_LENGTH] =
        "NEW-CAL-DATA-001";

    uint8_t readback[DID_WRITABLE_DATA_LENGTH];
    size_t readback_length = 0;
    int failures = 0;

    if (!mkdtemp(directory)) {
        perror("mkdtemp");
        return EXIT_FAILURE;
    }

    snprintf(storage_path, sizeof(storage_path),
             "%s/calibration-f1a1.bin", directory);

    printf("===== DID PERSISTENCE TESTS =====\n");

    if (did_manager_init(storage_path) != 0) {
        perror("Initial DID manager initialization");
        failures++;
        goto cleanup;
    }

    if (did_manager_read(DID_WRITABLE_CALIBRATION,
                         readback, sizeof(readback),
                         &readback_length) != 0 ||
        readback_length != DID_WRITABLE_DATA_LENGTH ||
        memcmp(readback, expected_default, sizeof(readback)) != 0) {
        printf("[FAIL] Default calibration initialization\n");
        failures++;
        goto cleanup;
    }

    printf("[PASS] Default calibration initialized\n");

    if (access(storage_path, F_OK) != 0) {
        printf("[FAIL] Persistent file created\n");
        failures++;
        goto cleanup;
    }

    printf("[PASS] Persistent file created\n");

    if (did_manager_write(DID_WRITABLE_CALIBRATION,
                          updated_value, sizeof(updated_value)) != 0) {
        perror("DID write");
        failures++;
        goto cleanup;
    }

    printf("[PASS] Updated calibration written\n");

    did_manager_reset();

    if (did_manager_init(storage_path) != 0) {
        perror("DID manager reinitialization");
        failures++;
        goto cleanup;
    }

    if (did_manager_read(DID_WRITABLE_CALIBRATION,
                         readback, sizeof(readback),
                         &readback_length) != 0 ||
        readback_length != DID_WRITABLE_DATA_LENGTH ||
        memcmp(readback, updated_value, sizeof(readback)) != 0) {
        printf("[FAIL] Calibration survives application restart\n");
        failures++;
        goto cleanup;
    }

    printf("[PASS] Calibration survives application restart\n");

cleanup:
    did_manager_reset();
    unlink(storage_path);
    rmdir(directory);

    printf("Persistence test failures: %d\n", failures);

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
