#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include "can_communication.h"
#include "uds_protocol.h"
#include "uds_session.h"

static int open_test_socket(
    const char *interface,
    int filter_enabled,
    canid_t filter_id)
{
    int fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd < 0)
        return -1;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);

    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        close(fd);
        return -1;
    }

    struct sockaddr_can addr = {
        .can_family = AF_CAN,
        .can_ifindex = ifr.ifr_ifindex
    };

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    if (filter_enabled) {
        struct can_filter filter = {
            .can_id = filter_id,
            .can_mask = CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG
        };

        if (setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER,
                       &filter, sizeof(filter)) < 0) {
            close(fd);
            return -1;
        }
    }

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static int activate_mode(uint8_t mode)
{
    const uint8_t request[] = {
        UDS_SID_COMMUNICATION_CONTROL,
        mode,
        0x01
    };

    uint8_t response[8] = {0};
    size_t response_len = 0;

    uds_session_reset();

    if (uds_session_set(UDS_SESSION_EXTENDED) != 0)
        return -1;

    if (uds_service_28(
            request, sizeof(request),
            response, sizeof(response),
            &response_len) != 0)
        return -1;

    if (response_len != 3 ||
        response[0] != 0x68 ||
        response[1] != mode ||
        response[2] != 0x01)
        return -1;

    return 0;
}

static int wait_for_frame(int fd, int timeout_ms)
{
    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN
    };

    return poll(&pfd, 1, timeout_ms);
}

static int test_mode(
    int ecu_fd,
    int injector_fd,
    int monitor_fd,
    uint8_t mode,
    int expected_rx,
    int expected_tx)
{
    int passed = 1;

    if (activate_mode(mode) != 0) {
        printf("[FAIL] Mode 0x%02X activation\n", mode);
        return 1;
    }

    /* Inject a tester-to-ECU application frame (0x300). */
    struct can_frame incoming = {
        .can_id = CAN_COMMUNICATION_RX_ID,
        .can_dlc = 2,
        .data = {0xA0, mode}
    };

    if (write(injector_fd, &incoming, sizeof(incoming))
        != sizeof(incoming)) {
        printf("[FAIL] Mode 0x%02X RX injection\n", mode);
        return 1;
    }

    int ready = wait_for_frame(ecu_fd, 500);
    struct can_frame received;
    int rx_result = ready > 0
        ? can_communication_receive(ecu_fd, &received)
        : -1;

    if (rx_result != expected_rx) {
        printf("[FAIL] Mode 0x%02X RX gate: expected %d, got %d\n",
               mode, expected_rx, rx_result);
        passed = 0;
    } else {
        printf("[PASS] Mode 0x%02X application RX %s\n",
               mode, expected_rx ? "enabled" : "blocked");
    }

    /* Attempt ECU-to-tester application transmission (0x301). */
    const uint8_t payload[] = {0xB0, mode};

    int tx_result = can_communication_send(
        ecu_fd, payload, sizeof(payload));

    if (tx_result != expected_tx) {
        printf("[FAIL] Mode 0x%02X TX gate: expected %d, got %d\n",
               mode, expected_tx, tx_result);
        passed = 0;
    } else {
        printf("[PASS] Mode 0x%02X application TX %s\n",
               mode, expected_tx ? "enabled" : "blocked");
    }

    int tx_ready = wait_for_frame(monitor_fd, 200);

    if (expected_tx) {
        struct can_frame transmitted;
        ssize_t count = tx_ready > 0
            ? read(monitor_fd, &transmitted, sizeof(transmitted))
            : -1;

        if (count != sizeof(transmitted) ||
            transmitted.can_id != CAN_COMMUNICATION_TX_ID ||
            transmitted.can_dlc != sizeof(payload) ||
            memcmp(transmitted.data, payload, sizeof(payload)) != 0) {
            printf("[FAIL] Mode 0x%02X transmitted frame verification\n",
                   mode);
            passed = 0;
        } else {
            printf("[PASS] Mode 0x%02X CAN frame 0x301 verified\n", mode);
        }
    } else if (tx_ready != 0) {
        printf("[FAIL] Mode 0x%02X unexpected CAN TX frame\n", mode);
        passed = 0;
    } else {
        printf("[PASS] Mode 0x%02X no CAN TX frame observed\n", mode);
    }

    return passed ? 0 : 1;
}

int main(void)
{
    int failures = 0;

    printf("====================================\n");
    printf(" AutoDiag CAN Communication Tests\n");
    printf("====================================\n");

    int ecu_fd = can_communication_open("vcan-ptest");
    int injector_fd = open_test_socket("vcan-ptest", 0, 0);
    int monitor_fd = open_test_socket(
        "vcan-ptest", 1, CAN_COMMUNICATION_TX_ID);

    if (ecu_fd < 0 || injector_fd < 0 || monitor_fd < 0) {
        perror("Unable to open CAN test sockets");
        failures++;
        goto cleanup;
    }

    failures += test_mode(ecu_fd, injector_fd, monitor_fd,
                          0x00, 1, 1);
    failures += test_mode(ecu_fd, injector_fd, monitor_fd,
                          0x01, 1, 0);
    failures += test_mode(ecu_fd, injector_fd, monitor_fd,
                          0x02, 0, 1);
    failures += test_mode(ecu_fd, injector_fd, monitor_fd,
                          0x03, 0, 0);

cleanup:
    can_communication_close(ecu_fd);

    if (injector_fd >= 0)
        close(injector_fd);

    if (monitor_fd >= 0)
        close(monitor_fd);

    uds_session_reset();

    printf("------------------------------------\n");
    printf("Failures: %d\n", failures);
    printf("%s\n", failures == 0
           ? "ALL CAN TESTS PASSED"
           : "CAN TESTS FAILED");

    return failures == 0 ? 0 : 1;
}
