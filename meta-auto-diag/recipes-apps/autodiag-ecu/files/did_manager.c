#define _POSIX_C_SOURCE 200809L

#include "did_manager.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

static const uint8_t initial_calibration[DID_WRITABLE_DATA_LENGTH] =
    "AUTODIAG-CAL-001";

static uint8_t writable_calibration[DID_WRITABLE_DATA_LENGTH] =
    "AUTODIAG-CAL-001";

static char calibration_storage_path[512];

static int save_calibration(const uint8_t *data)
{
    char temporary_path[sizeof(calibration_storage_path) + 16];

    int length = snprintf(
        temporary_path,
        sizeof(temporary_path),
        "%s.tmp.XXXXXX",
        calibration_storage_path
    );

    if (length < 0 || (size_t)length >= sizeof(temporary_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    int fd = mkstemp(temporary_path);

    if (fd < 0)
        return -1;

    int result = 0;
    int saved_errno = 0;
    size_t written = 0;

    if (fchmod(fd, 0600) < 0)
        goto failure;

    while (written < DID_WRITABLE_DATA_LENGTH) {
        ssize_t count = write(
            fd,
            data + written,
            DID_WRITABLE_DATA_LENGTH - written
        );

        if (count < 0) {
            if (errno == EINTR)
                continue;

            goto failure;
        }

        if (count == 0) {
            errno = EIO;
            goto failure;
        }

        written += (size_t)count;
    }

    if (fsync(fd) < 0)
        goto failure;

    if (close(fd) < 0) {
        fd = -1;
        goto failure;
    }

    fd = -1;

    if (rename(temporary_path, calibration_storage_path) < 0)
        goto failure;

    /*
     * Sync the parent directory so the rename is durable too.
     */
    char directory_path[sizeof(calibration_storage_path)];
    snprintf(directory_path, sizeof(directory_path), "%s",
             calibration_storage_path);

    char *last_slash = strrchr(directory_path, '/');

    if (last_slash != NULL) {
        if (last_slash == directory_path)
            last_slash[1] = '\0';
        else
            *last_slash = '\0';
    } else {
        snprintf(directory_path, sizeof(directory_path), ".");
    }

    int directory_fd = open(directory_path, O_RDONLY);

    if (directory_fd < 0)
        return -1;

    int sync_result = fsync(directory_fd);
    int sync_errno = errno;
    close(directory_fd);

    if (sync_result < 0) {
        errno = sync_errno;
        return -1;
    }

    return 0;

failure:
    saved_errno = errno;

    if (fd >= 0)
        close(fd);

    unlink(temporary_path);

    result = -1;
    errno = saved_errno;

    return result;
}

int did_manager_init(const char *storage_path)
{
    if (!storage_path || storage_path[0] == '\0') {
        errno = EINVAL;
        return -1;
    }

    int length = snprintf(
        calibration_storage_path,
        sizeof(calibration_storage_path),
        "%s",
        storage_path
    );

    if (length < 0 || (size_t)length >= sizeof(calibration_storage_path)) {
        calibration_storage_path[0] = '\0';
        errno = ENAMETOOLONG;
        return -1;
    }

    int fd = open(calibration_storage_path, O_RDONLY);

    if (fd < 0) {
        if (errno != ENOENT)
            return -1;

        if (save_calibration(initial_calibration) != 0)
            return -1;

        memcpy(
            writable_calibration,
            initial_calibration,
            DID_WRITABLE_DATA_LENGTH
        );

        printf("Initialized persistent DID F1A1 with default value at %s\n",
               calibration_storage_path);

        return 0;
    }

    uint8_t saved_data[DID_WRITABLE_DATA_LENGTH];
    size_t received = 0;

    while (received < sizeof(saved_data)) {
        ssize_t count = read(
            fd,
            saved_data + received,
            sizeof(saved_data) - received
        );

        if (count < 0) {
            if (errno == EINTR)
                continue;

            goto load_failure;
        }

        if (count == 0) {
            errno = EINVAL;
            goto load_failure;
        }

        received += (size_t)count;
    }

    uint8_t extra_byte;
    ssize_t extra_count;

    do {
        extra_count = read(fd, &extra_byte, 1);
    } while (extra_count < 0 && errno == EINTR);

    if (extra_count != 0) {
        if (extra_count > 0)
            errno = EINVAL;

        goto load_failure;
    }

    if (close(fd) < 0)
        return -1;

    memcpy(writable_calibration, saved_data, sizeof(saved_data));

    printf("Loaded persistent DID F1A1 from %s\n",
           calibration_storage_path);

    return 0;

load_failure:
    {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }
}

void did_manager_reset(void)
{
    memcpy(
        writable_calibration,
        initial_calibration,
        DID_WRITABLE_DATA_LENGTH
    );

    /* Keep unit tests independent of the ECU's persistent storage. */
    calibration_storage_path[0] = '\0';
}

int did_manager_read(
    uint16_t did,
    uint8_t *data,
    size_t capacity,
    size_t *data_len)
{
    if (!data || !data_len)
        return -1;

    *data_len = 0;

    if (did != DID_WRITABLE_CALIBRATION)
        return -1;

    if (capacity < DID_WRITABLE_DATA_LENGTH)
        return -1;

    memcpy(data, writable_calibration, DID_WRITABLE_DATA_LENGTH);
    *data_len = DID_WRITABLE_DATA_LENGTH;

    return 0;
}

int did_manager_write(
    uint16_t did,
    const uint8_t *data,
    size_t data_len)
{
    if (!data)
        return -1;

    if (did != DID_WRITABLE_CALIBRATION)
        return -1;

    if (data_len != DID_WRITABLE_DATA_LENGTH)
        return -1;

    /* Persist first. Never acknowledge a write that was not saved. */
    if (calibration_storage_path[0] != '\0') {
        if (save_calibration(data) != 0)
            return -1;
    }

    memcpy(writable_calibration, data, DID_WRITABLE_DATA_LENGTH);

    return 0;
}
