#include "uds_security.h"

#define UDS_SECURITY_SECRET 0xA5C39E71U

static uint32_t current_seed;
static int seed_valid;
static int security_unlocked;

void uds_security_reset(void)
{
    current_seed = 0;
    seed_valid = 0;
    security_unlocked = 0;
}

int uds_security_is_unlocked(void)
{
    return security_unlocked;
}

uint32_t uds_security_generate_seed(void)
{
    /*
     * Deterministic educational seed for repeatable testing.
     * This is NOT cryptographically secure.
     */
    current_seed = 0x12345678U;
    seed_valid = 1;

    return current_seed;
}

uint32_t uds_security_calculate_key(uint32_t seed)
{
    /* Educational seed/key transformation only. */
    return seed ^ UDS_SECURITY_SECRET;
}

int uds_security_has_valid_seed(void)
{
    return seed_valid;
}

int uds_security_validate_key(uint32_t key)
{
    if (!seed_valid)
        return -1;

    if (key != uds_security_calculate_key(current_seed)) {
        seed_valid = 0;
        return -2;
    }

    seed_valid = 0;
    security_unlocked = 1;

    return 0;
}
