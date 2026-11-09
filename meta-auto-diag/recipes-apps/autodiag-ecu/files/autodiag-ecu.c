#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>

#include <linux/can.h>
#include <linux/can/isotp.h>

#include <net/if.h>

#include "uds_protocol.h"
#include "uds_session.h"
#include "did_manager.h"
#include "can_communication.h"

#define CAN_INTERFACE "vcan0"

#define ECU_TX_ID    0x708
#define TESTER_RX_ID 0x700

#define UDS_MAX_MESSAGE_SIZE 4095


static void execute_pending_reset(void)
{
    uint8_t reset_type = uds_ecu_reset_get_pending();

    if (reset_type == 0)
        return;

    switch (reset_type) {
    case 0x01:
        printf("Executing ECU Hard Reset\n");
        break;
    case 0x02:
        printf("Executing ECU Key Off On Reset\n");
        break;
    case 0x03:
        printf("Executing ECU Soft Reset\n");
        break;
    default:
        fprintf(stderr, "Unknown pending ECU reset type: 0x%02X\n",
                reset_type);
        uds_ecu_reset_clear_pending();
        return;
    }

    fflush(stdout);

    /* Clear the pending state before attempting the reboot. */
    uds_ecu_reset_clear_pending();

    sync();

    if (reboot(RB_AUTOBOOT) < 0)
        perror("ECU reboot");
}

int main(void)
{
    if (did_manager_init("/var/lib/autodiag-ecu/calibration-f1a1.bin") != 0) {
        perror("DID storage initialization");
        return EXIT_FAILURE;
    }

    int sock;
    int can_sock;
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

    /* Open the normal application CAN RAW socket. */
    can_sock = can_communication_open(CAN_INTERFACE);

    if (can_sock < 0) {
        fprintf(stderr, "Failed to open application CAN socket\n");
        close(sock);
        return EXIT_FAILURE;
    }

    printf("AutoDiag UDS ECU Server started\n");
    printf("Interface: %s\n", CAN_INTERFACE);
    printf("ECU TX ID: 0x%03X\n", ECU_TX_ID);
    printf("Tester RX ID: 0x%03X\n", TESTER_RX_ID);
    printf("Waiting for UDS requests...\n");
    fflush(stdout);

    struct pollfd socket_poll[2] = {
        { .fd = sock, .events = POLLIN },
        { .fd = can_sock, .events = POLLIN }
    };

    while (1) {

        int poll_result = poll(socket_poll, 2, 100);

        if (poll_result < 0) {
            if (errno == EINTR)
                continue;

            perror("ECU poll");
            continue;
        }

        /*
         * Check S3Server on every iteration, including when
         * application CAN traffic keeps the poll loop active.
         */
        if (uds_session_check_timeout()) {
            printf("S3Server timeout: returned to Default Session; "
                   "SecurityAccess reset\n");
            fflush(stdout);
        }

        if (poll_result == 0)
            continue;

        /* Process normal application CAN traffic independently. */
        if (socket_poll[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr, "CAN RAW socket poll error: 0x%X\n",
                    socket_poll[1].revents);
        }

        if (socket_poll[1].revents & POLLIN) {
            struct can_frame app_frame;

            int can_result =
                can_communication_receive(can_sock, &app_frame);

            if (can_result < 0) {
                perror("Application CAN receive");
            } else if (can_result > 0) {
                printf("Received application CAN frame 0x%03X: ",
                       app_frame.can_id & CAN_SFF_MASK);

                for (uint8_t i = 0; i < app_frame.can_dlc; i++)
                    printf("%02X ", app_frame.data[i]);

                printf("\n");

                int send_result = can_communication_send(
                    can_sock,
                    app_frame.data,
                    app_frame.can_dlc
                );

                if (send_result < 0) {
                    perror("Application CAN transmit");
                } else if (send_result > 0) {
                    printf("Sent application CAN response 0x%03X\n",
                           CAN_COMMUNICATION_TX_ID);
                } else {
                    printf("Application CAN transmission disabled "
                           "by UDS 0x28\n");
                }

                fflush(stdout);
            }
        }

        /* Process UDS diagnostics independently of application CAN. */
        if (socket_poll[0].revents & (POLLHUP | POLLNVAL)) {
            fprintf(stderr, "ISO-TP socket unavailable: 0x%X\n",
                    socket_poll[0].revents);
            continue;
        }

        /*
         * POLLERR may occur together with POLLIN.
         * Inspect the pending error without discarding readable data.
         */
        if (socket_poll[0].revents & POLLERR) {
            int socket_error = 0;
            socklen_t error_length = sizeof(socket_error);

            if (getsockopt(sock, SOL_SOCKET, SO_ERROR,
                           &socket_error, &error_length) < 0) {
                perror("ISO-TP SO_ERROR");
            } else if (socket_error != 0) {
                fprintf(stderr, "ISO-TP pending error: %s (%d)\n",
                        strerror(socket_error), socket_error);
            } else {
                fprintf(stderr, "ISO-TP reported POLLERR\n");
            }
        }

        if (!(socket_poll[0].revents & POLLIN))
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

        /* Handle requests that suppress their positive response. */
        if (response_len == 0) {
            printf("Positive response suppressed\n");
            fflush(stdout);

            if (uds_ecu_reset_get_pending() != 0)
                execute_pending_reset();

            continue;
        }

        printf("Sending UDS response: ");

        for (size_t i = 0; i < response_len; i++)
            printf("%02X ", response[i]);

        printf("\n");

        ssize_t sent = write(sock, response, response_len);

        if (sent != (ssize_t)response_len) {
            perror("ISO-TP write");

            if (uds_ecu_reset_get_pending() != 0) {
                uds_ecu_reset_clear_pending();
                fprintf(stderr,
                        "ECU reset cancelled: response transmission failed\n");
            }
        } else if (uds_ecu_reset_get_pending() != 0) {
            execute_pending_reset();
        }

        fflush(stdout);
    }

    can_communication_close(can_sock);
    close(sock);
    return EXIT_SUCCESS;
}
