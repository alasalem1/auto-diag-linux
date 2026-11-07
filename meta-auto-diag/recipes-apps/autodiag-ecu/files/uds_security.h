#ifndef UDS_SECURITY_H
#define UDS_SECURITY_H

#include <stdint.h>

#define UDS_SECURITY_LEVEL_LOCKED    0
#define UDS_SECURITY_LEVEL_1         1

void uds_security_reset(void);
int uds_security_is_unlocked(void);

uint32_t uds_security_generate_seed(void);
uint32_t uds_security_calculate_key(uint32_t seed);

int uds_security_has_valid_seed(void);
int uds_security_validate_key(uint32_t key);

#endif
