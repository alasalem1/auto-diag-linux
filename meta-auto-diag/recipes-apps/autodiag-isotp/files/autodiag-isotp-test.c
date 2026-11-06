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

#define TESTER_TX_ID 0x700
#define ECU_RX_ID    0x708

#define VIN_LENGTH 17

int main(void)
{
    int sock;
    struct sockaddr_can addr;
    struct ifreq ifr;

    unsigned char request[] = {
        0x22, 0xF1, 0x90
    };

    unsigned char response[4095];

    struct timeval timeout = {
        .tv_sec = 5,
        .tv_usec = 0
    };

    printf("====================================\n");
    printf("       AutoDiag UDS Tester\n");
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

    printf("Interface       : %s\n", CAN_INTERFACE);
    printf("Tester TX ID    : 0x%03X\n", TESTER_TX_ID);
    printf("ECU RX ID       : 0x%03X\n", ECU_RX_ID);

    printf("\n[1] Sending UDS request...\n");
    printf("    Service     : ReadDataByIdentifier (0x22)\n");
    printf("    DID         : F190 (VIN)\n");

    ssize_t sent = write(sock, request, sizeof(request));

    if (sent != sizeof(request)) {
        if (sent < 0)
            perror("ISO-TP write");
        else
            fprintf(stderr, "Incomplete ISO-TP request\n");

        close(sock);
        return EXIT_FAILURE;
    }

    printf("    Request     : 22 F1 90\n");
    printf("    Status      : SENT\n");

    printf("\n[2] Waiting for ECU response...\n");

    ssize_t received = read(sock, response, sizeof(response));

    if (received < 0) {
        perror("ISO-TP read");
        close(sock);
        return EXIT_FAILURE;
    }

    printf("    Received    : %zd bytes\n", received);

    printf("\n[3] Validating UDS response...\n");

    if (received < 3) {
        fprintf(stderr, "FAIL: Response too short\n");
        close(sock);
        return EXIT_FAILURE;
    }

    if (response[0] == 0x7F) {
        printf("    Negative UDS response received\n");
        printf("    Request SID : 0x%02X\n", response[1]);
        printf("    NRC         : 0x%02X\n", response[2]);
        close(sock);
        return EXIT_FAILURE;
    }

    if (response[0] != 0x62) {
        fprintf(stderr, "FAIL: Unexpected response SID: 0x%02X\n",
                response[0]);
        close(sock);
        return EXIT_FAILURE;
    }

    if (response[1] != 0xF1 || response[2] != 0x90) {
        fprintf(stderr, "FAIL: Incorrect DID in ECU response\n");
        close(sock);
        return EXIT_FAILURE;
    }

    if (received != 3 + VIN_LENGTH) {
        fprintf(stderr, "FAIL: Expected %d response bytes, received %zd\n",
                3 + VIN_LENGTH, received);
        close(sock);
        return EXIT_FAILURE;
    }

    char vin[VIN_LENGTH + 1];

    for (int i = 0; i < VIN_LENGTH; i++) {
        unsigned char c = response[i + 3];

        if (c < 0x20 || c > 0x7E) {
            fprintf(stderr, "FAIL: Invalid VIN character at position %d\n",
                    i + 1);
            close(sock);
            return EXIT_FAILURE;
        }

        vin[i] = (char)c;
    }

    vin[VIN_LENGTH] = '\0';

    printf("    Response SID: 0x62 (positive response)\n");
    printf("    DID         : F190\n");
    printf("    Validation  : PASS\n");

    printf("\n[4] Decoded ECU data\n");
    printf("    Simulated VIN: %s\n", vin);

    printf("\n====================================\n");
    printf("             TEST PASS\n");
    printf("====================================\n");

    close(sock);

    return EXIT_SUCCESS;
}
