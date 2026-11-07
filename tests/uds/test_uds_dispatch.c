#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "uds_protocol.h"

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

int main(void)
{
    int failures = 0;

    const uint8_t valid_vin[] = {0x22, 0xF1, 0x90};
    const uint8_t unsupported_did[] = {0x22, 0xF1, 0x87};
    const uint8_t invalid_length[] = {0x22, 0xF1};
    const uint8_t unsupported_service[] = {0x10, 0x01};
    const uint8_t empty_request[] = {0x00};

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
        0x7F, 0x10, 0x11
    };

    const uint8_t expected_empty_request[] = {
        0x7F, 0x00, 0x13
    };

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

    failures += test_insufficient_capacity();

    printf("------------------------------------\n");
    printf("Failures: %d\n", failures);
    printf("%s\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED");
    printf("====================================\n");

    return failures == 0 ? 0 : 1;
}
