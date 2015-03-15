
/* L3info - SYR2 Project - SYR DeaDBeeF
 * ============================================================================
 * Deadbeef
 * ----------------------------------------------------------------------------
 * Common functions for server and client
 * ----------------------------------------------------------------------------
 * Antoine Pinsard
 * Mar. 15, 2015
 */
#include "deadbeef.h"

/**
 * Wrapper of sendto().
 * See: sendto(int, const void*, size_t, int, const struct sockaddr*,
 *             socklen_t)
 */
int send_message(int sock, struct sockaddr_in* dest, unsigned char* buffer) {
    int msg_len;

    assert(dest != NULL);
    assert(buffer != NULL);

    msg_len = sendto(sock, buffer, MSG_LENGTH, 0, (struct sockaddr *) dest,
                     sizeof(struct sockaddr_in));
    if (msg_len < 0) {
        perror("Message sending failed");
    }

    return msg_len;
}
