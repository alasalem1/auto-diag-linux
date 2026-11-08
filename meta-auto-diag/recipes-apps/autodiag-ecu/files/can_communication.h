#ifndef CAN_COMMUNICATION_H
#define CAN_COMMUNICATION_H

#include <stdint.h>
#include <linux/can.h>

#define CAN_COMMUNICATION_RX_ID 0x300
#define CAN_COMMUNICATION_TX_ID 0x301

/*
 * Open and bind a SocketCAN RAW socket.
 * Returns the socket file descriptor, or -1 on error.
 */
int can_communication_open(const char *interface);

/*
 * Receive one normal application CAN frame.
 * Returns:
 *   1  frame accepted
 *   0  frame ignored because RX is disabled
 *  -1  error
 */
int can_communication_receive(
    int socket_fd,
    struct can_frame *frame
);

/*
 * Transmit one normal application CAN frame.
 * Returns:
 *   1  frame transmitted
 *   0  transmission blocked because TX is disabled
 *  -1  error
 */
int can_communication_send(
    int socket_fd,
    const uint8_t *data,
    uint8_t length
);

/* Close the RAW socket. */
void can_communication_close(int socket_fd);

#endif
