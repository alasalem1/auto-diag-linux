#ifndef DID_MANAGER_H
#define DID_MANAGER_H

#include <stddef.h>
#include <stdint.h>

#define DID_WRITABLE_CALIBRATION  0xF1A1
#define DID_WRITABLE_DATA_LENGTH  16

int did_manager_init(const char *storage_path);

void did_manager_reset(void);

int did_manager_read(
    uint16_t did,
    uint8_t *data,
    size_t capacity,
    size_t *data_len
);

int did_manager_write(
    uint16_t did,
    const uint8_t *data,
    size_t data_len
);

#endif
