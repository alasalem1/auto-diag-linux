#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <net/if.h>

#include "can_communication.h"
#include "uds_protocol.h"

int can_communication_open(const char *interface)
{
    int sock;
    struct sockaddr_can addr;
    struct ifreq ifr;

    struct can_filter filter = {
        .can_id = CAN_COMMUNICATION_RX_ID,
        .can_mask = CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG
    };

    if (interface == NULL) {
        errno = EINVAL;
        return -1;
    }

    sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);

    if (sock < 0) {
        perror("CAN RAW socket");
        return -1;
    }

    /*
     * Keep CAN reception non-blocking so the ECU main loop
     * can monitor both ISO-TP and CAN RAW sockets with poll().
     */
    int flags = fcntl(sock, F_GETFL, 0);

    if (flags < 0 ||
        fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("CAN RAW non-blocking configuration");
        close(sock);
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);

    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        perror("CAN interface index");
        close(sock);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("CAN RAW bind");
        close(sock);
        return -1;
    }

    if (setsockopt(sock, SOL_CAN_RAW, CAN_RAW_FILTER,
                   &filter, sizeof(filter)) < 0) {
        perror("CAN RAW filter");
        close(sock);
        return -1;
    }

    return sock;
}

int can_communication_receive(
    int socket_fd,
    struct can_frame *frame)
{
    if (socket_fd < 0 || frame == NULL) {
        errno = EINVAL;
        return -1;
    }

    ssize_t received = read(socket_fd, frame, sizeof(*frame));

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }

        return -1;
    }

    if (received != sizeof(*frame) ||
        frame->can_dlc > CAN_MAX_DLEN) {
        errno = EPROTO;
        return -1;
    }

    if (uds_communication_control_get() == 0x02 ||
        uds_communication_control_get() == 0x03) {
        return 0;
    }

    return 1;
}

int can_communication_send(
    int socket_fd,
    const uint8_t *data,
    uint8_t length)
{
    struct can_frame frame;

    if (socket_fd < 0 || length > CAN_MAX_DLEN ||
        (length > 0 && data == NULL)) {
        errno = EINVAL;
        return -1;
    }

    if (uds_communication_control_get() == 0x01 ||
        uds_communication_control_get() == 0x03) {
        return 0;
    }

    memset(&frame, 0, sizeof(frame));

    frame.can_id = CAN_COMMUNICATION_TX_ID;
    frame.can_dlc = length;

    if (length > 0) {
        memcpy(frame.data, data, length);
    }

    ssize_t sent = write(socket_fd, &frame, sizeof(frame));

    if (sent < 0) {
        return -1;
    }

    if (sent != sizeof(frame)) {
        errno = EIO;
        return -1;
    }

    return 1;
}

void can_communication_close(int socket_fd)
{
    if (socket_fd >= 0) {
        close(socket_fd);
    }
}
