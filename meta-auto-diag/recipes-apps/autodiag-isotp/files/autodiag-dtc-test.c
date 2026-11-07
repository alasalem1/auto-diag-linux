#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/isotp.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#define CAN_INTERFACE "vcan0"
#define TESTER_TX_ID  0x700
#define ECU_RX_ID     0x708

static int send_uds_request(
    int sock,
    const unsigned char *request,
    size_t request_len,
    const unsigned char *expected,
    size_t expected_len,
    const char *description)
{
    unsigned char response[4095];

    printf("\n===== %s =====\n", description);

    printf("Request :");
    for (size_t i = 0; i < request_len; i++)
        printf(" %02X", request[i]);
    printf("\n");

    ssize_t sent = write(sock, request, request_len);

    if (sent != (ssize_t)request_len) {
        if (sent < 0)
            perror("ISO-TP write");
        else
            fprintf(stderr, "Incomplete ISO-TP request\n");
        return -1;
    }

    ssize_t received = read(sock, response, sizeof(response));

    if (received < 0) {
        perror("ISO-TP read");
        return -1;
    }

    printf("Response:");
    for (ssize_t i = 0; i < received; i++)
        printf(" %02X", response[i]);
    printf("\n");

    if (received != (ssize_t)expected_len ||
        memcmp(response, expected, expected_len) != 0) {
        fprintf(stderr, "FAIL: Unexpected ECU response\n");
        return -1;
    }

    printf("Validation: PASS\n");
    return 0;
}

int main(void)
{
    const unsigned char read_all_request[] = {
        0x19, 0x02, 0xFF
    };
    const unsigned char read_all_expected[] = {
        0x59, 0x02, 0xFF,
        0x12, 0x34, 0x56, 0x09,
        0x23, 0x45, 0x67, 0x0B,
        0x34, 0x56, 0x78, 0x01
    };

    const unsigned char read_confirmed_request[] = {
        0x19, 0x02, 0x08
    };
    const unsigned char read_confirmed_expected[] = {
        0x59, 0x02, 0xFF,
        0x12, 0x34, 0x56, 0x09,
        0x23, 0x45, 0x67, 0x0B
    };

    const unsigned char unsupported_request[] = {
        0x19, 0x01, 0xFF
    };
    const unsigned char unsupported_expected[] = {
        0x7F, 0x19, 0x12
    };

    const unsigned char invalid_length_request[] = {
        0x19, 0x02
    };
    const unsigned char invalid_length_expected[] = {
        0x7F, 0x19, 0x13
    };

    const unsigned char clear_all_request[] = {
        0x14, 0xFF, 0xFF, 0xFF
    };
    const unsigned char clear_all_expected[] = {
        0x54
    };

    const unsigned char read_empty_expected[] = {
        0x59, 0x02, 0xFF
    };

    const unsigned char unsupported_clear_group[] = {
        0x14, 0x00, 0x00, 0x00
    };
    const unsigned char unsupported_clear_expected[] = {
        0x7F, 0x14, 0x31
    };

    const unsigned char invalid_clear_length[] = {
        0x14, 0xFF, 0xFF
    };
    const unsigned char invalid_clear_expected[] = {
        0x7F, 0x14, 0x13
    };

    int sock;
    int failures = 0;
    struct sockaddr_can addr;
    struct ifreq ifr;

    struct timeval timeout = {
        .tv_sec = 5,
        .tv_usec = 0
    };

    printf("====================================\n");
    printf(" AutoDiag UDS DTC Diagnostics Tester\n");
    printf("====================================\n");

    sock = socket(PF_CAN, SOCK_DGRAM, CAN_ISOTP);

    if (sock < 0) {
        perror("ISO-TP socket");
        return EXIT_FAILURE;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                   &timeout, sizeof(timeout)) < 0) {
        perror("Receive timeout");
        close(sock);
        return EXIT_FAILURE;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, CAN_INTERFACE, IFNAMSIZ - 1);

    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        perror("Interface index");
        close(sock);
        return EXIT_FAILURE;
    }

    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    addr.can_addr.tp.tx_id = TESTER_TX_ID;
    addr.can_addr.tp.rx_id = ECU_RX_ID;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("ISO-TP bind");
        close(sock);
        return EXIT_FAILURE;
    }

    printf("Interface    : %s\n", CAN_INTERFACE);
    printf("Tester TX ID : 0x%03X\n", TESTER_TX_ID);
    printf("ECU RX ID    : 0x%03X\n", ECU_RX_ID);

    if (send_uds_request(
            sock,
            read_all_request, sizeof(read_all_request),
            read_all_expected, sizeof(read_all_expected),
            "Read all DTCs (status mask FF)") != 0)
        failures++;

    if (send_uds_request(
            sock,
            read_confirmed_request, sizeof(read_confirmed_request),
            read_confirmed_expected, sizeof(read_confirmed_expected),
            "Read confirmed DTCs (status mask 08)") != 0)
        failures++;

    if (send_uds_request(
            sock,
            unsupported_request, sizeof(unsupported_request),
            unsupported_expected, sizeof(unsupported_expected),
            "Unsupported ReadDTCInformation subfunction") != 0)
        failures++;

    if (send_uds_request(
            sock,
            invalid_length_request, sizeof(invalid_length_request),
            invalid_length_expected, sizeof(invalid_length_expected),
            "Invalid ReadDTCInformation request length") != 0)
        failures++;

    /*
     * Clear all DTCs only after validating the initial database.
     */
    if (send_uds_request(
            sock,
            clear_all_request, sizeof(clear_all_request),
            clear_all_expected, sizeof(clear_all_expected),
            "Clear all DTCs (group FFFFFF)") != 0)
        failures++;

    if (send_uds_request(
            sock,
            read_all_request, sizeof(read_all_request),
            read_empty_expected, sizeof(read_empty_expected),
            "Verify DTC database is empty after clearing") != 0)
        failures++;

    if (send_uds_request(
            sock,
            unsupported_clear_group, sizeof(unsupported_clear_group),
            unsupported_clear_expected, sizeof(unsupported_clear_expected),
            "Unsupported ClearDiagnosticInformation group") != 0)
        failures++;

    if (send_uds_request(
            sock,
            invalid_clear_length, sizeof(invalid_clear_length),
            invalid_clear_expected, sizeof(invalid_clear_expected),
            "Invalid ClearDiagnosticInformation request length") != 0)
        failures++;

    close(sock);

    printf("\n====================================\n");

    if (failures == 0) {
        printf("ALL DTC ISO-TP TESTS PASSED\n");
        printf("====================================\n");
        return EXIT_SUCCESS;
    }

    printf("DTC ISO-TP TESTS FAILED: %d\n", failures);
    printf("====================================\n");
    return EXIT_FAILURE;
}
