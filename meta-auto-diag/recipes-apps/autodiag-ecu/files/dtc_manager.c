#include "dtc_manager.h"

/*
 * Simulated diagnostic trouble code database.
 * DTC identifiers use the 24-bit format.
 */

static struct dtc_record dtc_database[] = {
    {0x123456, 0x09},
    {0x234567, 0x0B},
    {0x345678, 0x01}
};

static size_t dtc_database_count =
    sizeof(dtc_database) / sizeof(dtc_database[0]);

/* DTC setting is enabled by default. */
static int dtc_setting_enabled = 1;

uint8_t dtc_manager_get_status_availability_mask(void)
{
    return 0xFF;
}

size_t dtc_manager_get_by_status_mask(
    uint8_t status_mask,
    struct dtc_record *records,
    size_t capacity)
{
    if (!records || capacity == 0 || status_mask == 0)
        return 0;

    size_t count = 0;

    for (size_t i = 0; i < dtc_database_count; i++) {
        if ((dtc_database[i].status & status_mask) == 0)
            continue;

        if (count >= capacity)
            break;

        records[count++] = dtc_database[i];
    }

    return count;
}

void dtc_manager_clear_all(void)
{
    dtc_database_count = 0;
}

void dtc_manager_set_setting_enabled(int enabled)
{
    dtc_setting_enabled = enabled ? 1 : 0;
}

int dtc_manager_is_setting_enabled(void)
{
    return dtc_setting_enabled;
}
