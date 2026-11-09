#include "did_manager.h"

#include <string.h>

static const uint8_t initial_calibration[] = "AUTODIAG-CAL-001";

static uint8_t writable_calibration[DID_WRITABLE_DATA_LENGTH] = "AUTODIAG-CAL-001";

void did_manager_reset(void)
{
    memcpy(
        writable_calibration,
        initial_calibration,
        DID_WRITABLE_DATA_LENGTH
    );
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

    memcpy(writable_calibration, data, DID_WRITABLE_DATA_LENGTH);

    return 0;
}
