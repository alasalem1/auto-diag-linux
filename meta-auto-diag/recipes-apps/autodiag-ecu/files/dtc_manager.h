#ifndef DTC_MANAGER_H
#define DTC_MANAGER_H

#include <stddef.h>
#include <stdint.h>

#define DTC_MANAGER_MAX_RECORDS 16

struct dtc_record {
    uint32_t dtc;
    uint8_t status;
};

uint8_t dtc_manager_get_status_availability_mask(void);

void dtc_manager_set_setting_enabled(int enabled);
int dtc_manager_is_setting_enabled(void);

void dtc_manager_clear_all(void);

size_t dtc_manager_get_by_status_mask(
    uint8_t status_mask,
    struct dtc_record *records,
    size_t capacity
);

#endif
