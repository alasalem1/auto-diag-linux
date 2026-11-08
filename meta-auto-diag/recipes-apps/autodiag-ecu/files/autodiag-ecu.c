#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/can.h>
#include <linux/can/isotp.h>

#include <net/if.h>

#include "uds_protocol.h"
#include "uds_session.h"

#define CAN_INTERFACE "vcan0"

#define ECU_TX_ID    0x708
#define TESTER_RX_ID 0x700

#define UDS_MAX_MESSAGE_SIZE 4095

int main(void)
{
    int sock;
    struct sockaddr_can addr;
    struct ifreq ifr;

    uint8_t request[UDS_MAX_MESSAGE_SIZE];
    uint8_t response[UDS_MAX_MESSAGE_SIZE];

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

    printf("AutoDiag UDS ECU Server started\n");
    printf("Interface: %s\n", CAN_INTERFACE);
    printf("ECU TX ID: 0x%03X\n", ECU_TX_ID);
    printf("Tester RX ID: 0x%03X\n", TESTER_RX_ID);
    printf("Waiting for UDS requests...\n");
    fflush(stdout);

    struct pollfd socket_poll = {
        .fd = sock,
        .events = POLLIN
    };

    while (1) {

        int poll_result = poll(&socket_poll, 1, 100);

        if (poll_result < 0) {
            if (errno == EINTR)
                continue;

            perror("ISO-TP poll");
            continue;
        }

        if (poll_result == 0) {
            if (uds_session_check_timeout()) {
                printf("S3Server timeout: returned to Default Session; "
                       "SecurityAccess reset\n");
                fflush(stdout);
            }

            continue;
        }

        if (socket_poll.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr, "ISO-TP socket poll error: 0x%X\n",
                    socket_poll.revents);
            continue;
        }

        if (!(socket_poll.revents & POLLIN))
            continue;

        ssize_t received = read(sock, request, sizeof(request));

        if (received < 0) {
            if (errno == EINTR)
                continue;

            perror("ISO-TP read");
            continue;
        }

        /* Expire the old session before this request can refresh it. */
        if (uds_session_check_timeout()) {
            printf("S3Server timeout: returned to Default Session; "
                   "SecurityAccess reset\n");
        }

        /* Any received diagnostic request refreshes an active session. */
        uds_session_activity();

        printf("Received UDS request: ");

        for (ssize_t i = 0; i < received; i++)
            printf("%02X ", request[i]);

        printf("\n");

        size_t response_len = 0;

        if (uds_dispatch_request(
                request,
                (size_t)received,
                response,
                sizeof(response),
                &response_len) < 0) {

            fprintf(stderr, "UDS request processing failed\n");
            fflush(stdout);
            continue;
        }

        /* Suppress the positive response when requested (3E 80). */
        if (response_len == 0) {
            printf("Positive response suppressed\n");
            fflush(stdout);
            continue;
        }

        printf("Sending UDS response: ");

        for (size_t i = 0; i < response_len; i++)
            printf("%02X ", response[i]);

        printf("\n");

        if (write(sock, response, response_len) !=
            (ssize_t)response_len) {
            perror("ISO-TP write");
        }

        fflush(stdout);
    }

    close(sock);
    return EXIT_SUCCESS;
}
