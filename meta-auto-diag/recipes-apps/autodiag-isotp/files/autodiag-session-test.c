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
    const unsigned char protected_did_request[] = {0x22, 0xF1, 0xA0};
    const unsigned char protected_did_default_denied[] = {
        0x7F, 0x22, 0x7E
    };
    const unsigned char protected_did_locked_denied[] = {
        0x7F, 0x22, 0x33
    };
    const unsigned char protected_did_expected[] = {
        0x62, 0xF1, 0xA0,
        'A', 'U', 'T', 'O', 'D', 'I', 'A', 'G',
        '-', 'C', 'A', 'L', '-', '0', '0', '1'
    };

    const unsigned char security_seed_request[] = {0x27, 0x01};
    const unsigned char security_seed_expected[] = {
        0x67, 0x01, 0x12, 0x34, 0x56, 0x78
    };
    const unsigned char security_seed_denied[] = {0x7F, 0x27, 0x7E};
    const unsigned char security_wrong_key[] = {
        0x27, 0x02, 0x00, 0x00, 0x00, 0x00
    };
    const unsigned char security_wrong_key_expected[] = {
        0x7F, 0x27, 0x35
    };
    const unsigned char security_correct_key[] = {
        0x27, 0x02, 0xB7, 0xF7, 0xC8, 0x09
    };
    const unsigned char security_unlock_expected[] = {0x67, 0x02};

    const unsigned char writable_did_write_request[] = {
        0x2E, 0xF1, 0xA1,
        'N', 'E', 'W', '-', 'C', 'A', 'L', '-',
        'D', 'A', 'T', 'A', '-', '0', '0', '2'
    };
    const unsigned char writable_did_write_expected[] = {
        0x6E, 0xF1, 0xA1
    };
    const unsigned char writable_did_read_request[] = {
        0x22, 0xF1, 0xA1
    };
    const unsigned char writable_did_read_expected[] = {
        0x62, 0xF1, 0xA1,
        'N', 'E', 'W', '-', 'C', 'A', 'L', '-',
        'D', 'A', 'T', 'A', '-', '0', '0', '2'
    };

    int sock;
    struct sockaddr_can addr;
    struct ifreq ifr;

    struct timeval timeout = {
        .tv_sec = 5,
        .tv_usec = 0
    };

    const unsigned char default_request[] = {0x10, 0x01};
    const unsigned char default_expected[] = {
        0x50, 0x01, 0x00, 0x32, 0x01, 0xF4
    };

    const unsigned char extended_request[] = {0x10, 0x03};
    const unsigned char extended_expected[] = {
        0x50, 0x03, 0x00, 0x32, 0x01, 0xF4
    };

    const unsigned char unsupported_request[] = {0x10, 0x02};
    const unsigned char unsupported_expected[] = {
        0x7F, 0x10, 0x12
    };

    const unsigned char vin_request[] = {
        0x22, 0xF1, 0x90
    };

    const unsigned char vin_denied_expected[] = {
        0x7F, 0x22, 0x7E
    };

    const unsigned char vin_expected[] = {
        0x62, 0xF1, 0x90,
        'A', 'U', 'T', 'O', 'D', 'I', 'A', 'G',
        '0', '0', '0', '0', '0', '0', '0', '0', '1'
    };

    printf("====================================\n");
    printf(" AutoDiag UDS Session and Access Control Tester\n");
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

    int failures = 0;

    if (send_uds_request(
            sock,
            default_request, sizeof(default_request),
            default_expected, sizeof(default_expected),
            "Default Diagnostic Session (0x01)") != 0)
        failures++;

    if (send_uds_request(
            sock,
            extended_request, sizeof(extended_request),
            extended_expected, sizeof(extended_expected),
            "Extended Diagnostic Session (0x03)") != 0)
        failures++;

    if (send_uds_request(
            sock,
            unsupported_request, sizeof(unsupported_request),
            unsupported_expected, sizeof(unsupported_expected),
            "Unsupported Session (0x02)") != 0)
        failures++;

    if (send_uds_request(
            sock,
            vin_request, sizeof(vin_request),
            vin_denied_expected, sizeof(vin_denied_expected),
            "VIN access denied in Extended Session") != 0)
        failures++;

    if (send_uds_request(
            sock,
            default_request, sizeof(default_request),
            default_expected, sizeof(default_expected),
            "Return to Default Diagnostic Session") != 0)
        failures++;

    if (send_uds_request(
            sock,
            vin_request, sizeof(vin_request),
            vin_expected, sizeof(vin_expected),
            "VIN access restored in Default Session") != 0)
        failures++;


    /* SecurityAccess must be rejected in the default session. */
    if (send_uds_request(
            sock, security_seed_request, sizeof(security_seed_request),
            security_seed_denied, sizeof(security_seed_denied),
            "SecurityAccess denied in Default Session") != 0)
        failures++;

    if (send_uds_request(
            sock, extended_request, sizeof(extended_request),
            extended_expected, sizeof(extended_expected),
            "Enter Extended Session for SecurityAccess") != 0)
        failures++;

    if (send_uds_request(
            sock, security_seed_request, sizeof(security_seed_request),
            security_seed_expected, sizeof(security_seed_expected),
            "Request SecurityAccess Seed") != 0)
        failures++;

    if (send_uds_request(
            sock, security_wrong_key, sizeof(security_wrong_key),
            security_wrong_key_expected, sizeof(security_wrong_key_expected),
            "Reject Incorrect Security Key") != 0)
        failures++;

    if (send_uds_request(
            sock, security_seed_request, sizeof(security_seed_request),
            security_seed_expected, sizeof(security_seed_expected),
            "Request Fresh Seed After Incorrect Key") != 0)
        failures++;

    if (send_uds_request(
            sock, security_correct_key, sizeof(security_correct_key),
            security_unlock_expected, sizeof(security_unlock_expected),
            "Unlock ECU with Correct Security Key") != 0)
        failures++;

    if (send_uds_request(
            sock, default_request, sizeof(default_request),
            default_expected, sizeof(default_expected),
            "Return to Default Session and Reset Security") != 0)
        failures++;

    /* Protected DID access-control tests over ISO-TP. */

    if (send_uds_request(
            sock, protected_did_request, sizeof(protected_did_request),
            protected_did_default_denied, sizeof(protected_did_default_denied),
            "Protected DID denied in Default Session") != 0)
        failures++;

    if (send_uds_request(
            sock, extended_request, sizeof(extended_request),
            extended_expected, sizeof(extended_expected),
            "Enter Extended Session for Protected DID") != 0)
        failures++;

    if (send_uds_request(
            sock, protected_did_request, sizeof(protected_did_request),
            protected_did_locked_denied, sizeof(protected_did_locked_denied),
            "Protected DID denied before SecurityAccess") != 0)
        failures++;

    if (send_uds_request(
            sock, security_seed_request, sizeof(security_seed_request),
            security_seed_expected, sizeof(security_seed_expected),
            "Request Seed for Protected DID Access") != 0)
        failures++;

    if (send_uds_request(
            sock, security_correct_key, sizeof(security_correct_key),
            security_unlock_expected, sizeof(security_unlock_expected),
            "Unlock ECU for Protected DID Access") != 0)
        failures++;

    if (send_uds_request(
            sock, protected_did_request, sizeof(protected_did_request),
            protected_did_expected, sizeof(protected_did_expected),
            "Read Protected Calibration DID F1A0") != 0)
        failures++;

    if (send_uds_request(
            sock, writable_did_write_request, sizeof(writable_did_write_request),
            writable_did_write_expected, sizeof(writable_did_write_expected),
            "Write Calibration DID F1A1") != 0)
        failures++;

    if (send_uds_request(
            sock, writable_did_read_request, sizeof(writable_did_read_request),
            writable_did_read_expected, sizeof(writable_did_read_expected),
            "Read Back and Verify Calibration DID F1A1") != 0)
        failures++;

    if (send_uds_request(
            sock, default_request, sizeof(default_request),
            default_expected, sizeof(default_expected),
            "Return to Default Session After Protected Access") != 0)
        failures++;

    if (send_uds_request(
            sock, protected_did_request, sizeof(protected_did_request),
            protected_did_default_denied, sizeof(protected_did_default_denied),
            "Protected DID Denied After Security Reset") != 0)
        failures++;

    close(sock);

    printf("\n====================================\n");

    if (failures == 0) {
        printf(" ALL SESSION, SECURITY ACCESS AND PROTECTED DID WRITE/READBACK TESTS PASSED\n");
        printf("====================================\n");
        return EXIT_SUCCESS;
    }

    printf(" SESSION TESTS FAILED: %d\n", failures);
    printf("====================================\n");

    return EXIT_FAILURE;
}
