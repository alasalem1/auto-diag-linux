#define _POSIX_C_SOURCE 200809L

#include "did_manager.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


static int write_test_file(
    const char *path,
    const uint8_t *data,
    size_t length)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (fd < 0)
        return -1;

    size_t written = 0;

    while (written < length) {
        ssize_t count = write(fd, data + written, length - written);

        if (count < 0) {
            if (errno == EINTR)
                continue;

            close(fd);
            return -1;
        }

        if (count == 0) {
            close(fd);
            errno = EIO;
            return -1;
        }

        written += (size_t)count;
    }

    return close(fd);
}

static int file_matches(
    const char *path,
    const uint8_t *expected,
    size_t expected_length)
{
    uint8_t actual[32];

    int fd = open(path, O_RDONLY);

    if (fd < 0)
        return 0;

    ssize_t count = read(fd, actual, sizeof(actual));
    close(fd);

    return count == (ssize_t)expected_length &&
           memcmp(actual, expected, expected_length) == 0;
}

static void test_malformed_file(
    const char *path,
    const uint8_t *malformed_data,
    size_t malformed_length,
    const char *description,
    int *failures)
{
    if (write_test_file(path, malformed_data, malformed_length) != 0) {
        perror("Writing malformed test file");
        (*failures)++;
        return;
    }

    did_manager_reset();
    errno = 0;

    if (did_manager_init(path) == 0) {
        printf("[FAIL] %s was accepted\n", description);
        (*failures)++;
        did_manager_reset();
        return;
    }

    if (errno != EINVAL) {
        printf("[FAIL] %s returned unexpected error: %d\n",
               description, errno);
        (*failures)++;
    } else {
        printf("[PASS] %s rejected with EINVAL\n", description);
    }

    if (!file_matches(path, malformed_data, malformed_length)) {
        printf("[FAIL] %s was modified after rejection\n", description);
        (*failures)++;
    } else {
        printf("[PASS] %s remains unchanged\n", description);
    }

    did_manager_reset();
}


static void test_failed_write_preserves_memory(
    const char *storage_path,
    const uint8_t *original_value,
    const uint8_t *new_value,
    int *failures)
{
    char backup_path[600];
    uint8_t readback[DID_WRITABLE_DATA_LENGTH];
    size_t readback_length = 0;

    snprintf(backup_path, sizeof(backup_path), "%s.backup", storage_path);

    if (rename(storage_path, backup_path) != 0) {
        perror("Backing up calibration file");
        (*failures)++;
        return;
    }

    if (mkdir(storage_path, 0700) != 0) {
        perror("Creating destination directory");
        rename(backup_path, storage_path);
        (*failures)++;
        return;
    }

    int write_result = did_manager_write(
        DID_WRITABLE_CALIBRATION,
        new_value,
        DID_WRITABLE_DATA_LENGTH
    );

    if (write_result != 0) {
        printf("[PASS] Persistent write failure reported\n");
    } else {
        printf("[FAIL] Persistent write failure was not reported\n");
        (*failures)++;
    }

    if (did_manager_read(
            DID_WRITABLE_CALIBRATION,
            readback,
            sizeof(readback),
            &readback_length) == 0 &&
        readback_length == DID_WRITABLE_DATA_LENGTH &&
        memcmp(readback, original_value, sizeof(readback)) == 0) {
        printf("[PASS] Failed write preserves in-memory calibration\n");
    } else {
        printf("[FAIL] Failed write changed in-memory calibration\n");
        (*failures)++;
    }

    if (rmdir(storage_path) != 0) {
        perror("Removing destination directory");
        (*failures)++;
        return;
    }

    if (rename(backup_path, storage_path) != 0) {
        perror("Restoring calibration file");
        (*failures)++;
        return;
    }

    printf("[PASS] Original persistent calibration restored\n");
}


static void test_invalid_writes_preserve_calibration(
    const uint8_t *expected_value,
    int *failures)
{
    const uint8_t invalid_data[DID_WRITABLE_DATA_LENGTH - 1] =
        "INVALID-LENGTH";

    uint8_t readback[DID_WRITABLE_DATA_LENGTH];
    size_t readback_length = 0;

    if (did_manager_write(0x1234, expected_value,
                          DID_WRITABLE_DATA_LENGTH) == 0) {
        printf("[FAIL] Unsupported DID write was accepted\n");
        (*failures)++;
    } else {
        printf("[PASS] Unsupported DID write rejected\n");
    }

    if (did_manager_write(DID_WRITABLE_CALIBRATION,
                          invalid_data, sizeof(invalid_data)) == 0) {
        printf("[FAIL] Incorrect data length was accepted\n");
        (*failures)++;
    } else {
        printf("[PASS] Incorrect data length rejected\n");
    }

    if (did_manager_read(DID_WRITABLE_CALIBRATION,
                         readback, sizeof(readback),
                         &readback_length) == 0 &&
        readback_length == DID_WRITABLE_DATA_LENGTH &&
        memcmp(readback, expected_value, sizeof(readback)) == 0) {
        printf("[PASS] Invalid writes preserve calibration\n");
    } else {
        printf("[FAIL] Invalid writes changed calibration\n");
        (*failures)++;
    }
}

int main(void)
{
    char directory[] = "/tmp/autodiag-did-test-XXXXXX";
    char storage_path[512];

    const uint8_t expected_default[DID_WRITABLE_DATA_LENGTH] =
        "AUTODIAG-CAL-001";

    const uint8_t updated_value[DID_WRITABLE_DATA_LENGTH] =
        "NEW-CAL-DATA-001";

    uint8_t readback[DID_WRITABLE_DATA_LENGTH];

    const uint8_t truncated_data[DID_WRITABLE_DATA_LENGTH - 1] =
        "AUTODIAG-CAL-01";

    const uint8_t oversized_data[DID_WRITABLE_DATA_LENGTH + 1] = {
        'A', 'U', 'T', 'O', 'D', 'I', 'A', 'G',
        '-', 'C', 'A', 'L', '-', '0', '0', '1', 'X'
    };
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

    test_invalid_writes_preserve_calibration(
        updated_value,
        &failures
    );

    test_failed_write_preserves_memory(
        storage_path,
        updated_value,
        expected_default,
        &failures
    );

    test_malformed_file(
        storage_path,
        truncated_data,
        sizeof(truncated_data),
        "Truncated calibration file",
        &failures
    );

    test_malformed_file(
        storage_path,
        oversized_data,
        sizeof(oversized_data),
        "Oversized calibration file",
        &failures
    );

cleanup:
    did_manager_reset();
    unlink(storage_path);
    rmdir(directory);

    printf("Persistence test failures: %d\n", failures);

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
