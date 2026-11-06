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

#define CAN_INTERFACE "vcan0"

#define ECU_TX_ID    0x708
#define TESTER_RX_ID 0x700

int main(void)
{
    int sock;
    struct sockaddr_can addr;
    struct ifreq ifr;

    unsigned char request[4095];
    unsigned char response[] = {
        0x62, 0xF1, 0x90,
        'A', 'U', 'T', 'O', 'D', 'I', 'A', 'G',
        '0', '0', '0', '0', '0', '0', '0', '0', '1'
    };

    sock = socket(PF_CAN, SOCK_DGRAM, CAN_ISOTP);

    if (sock < 0) {
        perror("ISO-TP socket");
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
    addr.can_addr.tp.tx_id = ECU_TX_ID;
    addr.can_addr.tp.rx_id = TESTER_RX_ID;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("ISO-TP bind");
        close(sock);
        return EXIT_FAILURE;
    }

    printf("AutoDiag simulated ECU started\n");
    printf("Interface: %s\n", CAN_INTERFACE);
    printf("ECU TX ID: 0x%03X\n", ECU_TX_ID);
    printf("Tester RX ID: 0x%03X\n", TESTER_RX_ID);
    printf("Waiting for UDS requests...\n");
    fflush(stdout);

    while (1) {
        ssize_t received = read(sock, request, sizeof(request));

        if (received < 0) {
            perror("ISO-TP read");
            continue;
        }

        printf("Received UDS request: ");

        for (ssize_t i = 0; i < received; i++) {
            printf("%02X ", request[i]);
        }

        printf("\n");

        if (received == 3 &&
            request[0] == 0x22 &&
            request[1] == 0xF1 &&
            request[2] == 0x90) {

            printf("ReadDataByIdentifier (0x22) - DID F190\n");
            printf("Sending positive response...\n");

            if (write(sock, response, sizeof(response)) < 0) {
                perror("ISO-TP write");
            } else {
                printf("Response sent: 62 F1 90 + simulated VIN\n");
            }
        } else {
            printf("Unsupported UDS request\n");
        }

        fflush(stdout);
    }

    close(sock);
    return EXIT_SUCCESS;
}
