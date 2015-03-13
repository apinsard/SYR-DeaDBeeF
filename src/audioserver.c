/* L3info - SYR2 Project - SYR DeaDBeeF
 * ============================================================================
 * Audio Server
 * ----------------------------------------------------------------------------
 * The server waits expects at least one WAV file in its local directory. It
 * waits for a client request to read one of its files. When it receives a such
 * request, it opens the underlying file and start its transfert to the client.
 * When the file is entirely read, the server closes the file and waits for the
 * next request.
 * ----------------------------------------------------------------------------
 * Antoine Pinsard
 * Mar. 12, 2015
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "audioserver.h"
#include "sysprog-audio/audio.h"

int main(int argc, char** argv) {
    int sock
      , bind_err
      , recv_len;
    struct sockaddr_in server_addr;
                     , client_addr;
    struct sockaddr_in cur_served_clients[MAX_NB_CLIENTS];
    char recv_buffer[MAX_MSG_LENGTH];

    // List files in the local directory
    // If the list is empty, exit with error.

    // Server initialization
    sock = socket(AF_INET, SOCK_DGRAM, 0)
    if (sock < 0) {
        perror("Socket creation failed.");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1664);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind_err = bind(sock, (struct sockaddr *) &server_addr,
                    sizeof(struct sockaddr_in));

    if (bind_err < 0) {
        perror("Failed to bind socket.");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Client requests handling loop
    while (true) {
        // Wait for a client request
        recv_len = recvfrom(sock, &recv_buffer, MAX_MSG_LENGTH, 0,
                            (struct sockaddr *) &client_addr,
                            sizeof(struct sockaddr_in));
        if (recv_len < 0) {
            perror("Message reception failed.");
        }

        
    }

    close(sock);

    return EXIT_SUCCESS;
}
