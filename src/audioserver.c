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
 * Mar. 14, 2015
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include "audioserver.h"
#include "sysprog-audio/audio.h"


/**
 * Create an empty client list at the given address.
 */
void create_client_list(struct client_list* list) {
    int i;

    assert(list != NULL);

    list->nb_clients = 0;
    for (i = 0; i < MAX_NB_CLIENTS; i++) {
        list->clients[i] = NULL;
    }
}


/**
 * Add a client to the list of currently served clients.
 *
 * Return -1 if the maximum of simultaenous served client is reached, in which
 *        case the client request should not be satisfied.
 * Return -2 if dynamic allocation of the client failed.
 * Return a client identifier otherwise. This identifier will be useful to
 *        remove a client from the list.
 */
int append_client(struct client_list* list, struct sockaddr_in* addr) {
    int client_id;
    struct client* client;

    assert(list != NULL);
    assert(addr != NULL);
    assert(list->nb_clients <= MAX_NB_CLIENTS);

    if (list->nb_clients == MAX_NB_CLIENTS) {
        return -1;
    }

    // Get lowest available client ID.
    for (client_id = 0; client_id < MAX_NB_CLIENTS; client_id++) {
        if (list->clients[client_id] == NULL) {
            break;
        }
    }
    assert(client_id < MAX_NB_CLIENTS);

    // Create the new client
    client = malloc(sizeof(struct client*));
    if (client == NULL) {
        perror("Dynamic allocation failed!");
        return -2;
    }
    client->addr = addr;
    client->heartbeat_counter = HEARTBEAT_THRESHOLD;

    // Store then new client
    list->clients[client_id] = client;
    list->nb_clients++;

    return client_id;
}


/**
 * Remove the client with the given ID from the currently served clients list.
 *
 * Return the removed client address.
 */
struct sockaddr_in* remove_client(struct client_list* list, int client_id) {
    struct sockaddr_in* addr;

    assert(list != NULL);
    assert(client_id >= 0);
    assert(client_id < MAX_NB_CLIENTS);

    addr = list->clients[client_id]->addr;
    free(list->clients[client_id]);
    list->clients[client_id] = NULL;

    return addr;
}


/**
 * Reset the heartbeat counter of the client matching the given addr to
 * HEARTBEAT_THRESHOLD.
 *
 * Return -1 if no client matched.
 * Return the client_id otherwise.
 */
int notify_heartbeat(struct client_list* list, struct sockaddr_in* addr) {
    int client_id;
    struct sockaddr_in* client_addr;

    assert(list != NULL);
    assert(addr != NULL);

    // Search the client that has the given addr.
    for (client_id = 0; client_id < MAX_NB_CLIENTS; client_id++) {
        client_addr = list->clients[client_id]->addr;
        if (client_addr->sin_port == addr->sin_port &&
                client_addr->sin_addr.s_addr == addr->sin_addr.s_addr)
        {
            break;
        }
    }
    if (client_id == MAX_NB_CLIENTS) {
        return -1;
    }

    // Reset the heartbeat counter to HEARTBEAT_THRESHOLD
    list->clients[client_id]->heartbeat_counter = HEARTBEAT_THRESHOLD;

    return client_id;
}


/**
 * Generate an error message with respect to the protocol.
 * The generated message is written to output.
 *
 * Valid error codes are:
 *  - 0xDEADBEA7: Heartbeat waiting timeout
 *  - 0xDEADDEAD: Server received a SIGTERM
 *  - 0xDEADF11E: File not found
 *  - 0x0BADC0DE: Bad request not processed (nor processable).
 *  - 0x00C0FFEE: Maximum simultaneous served clients reached.
 *
 * The message parameter is just a short human-readable description of the
 * error. This message is truncated if too long.
 */
void gen_error_message(unsigned char* output, unsigned int code,
        const char* message)
{
    int i
      , msg_len;

    assert(output != NULL);

    if (message == NULL) {
        msg_len = 0;
    }
    else {
        msg_len = strlen(message);
    }

    // First and foremost, store the instruction "RESP_ERROR".
    output[0] = RESP_ERROR;

    // Copy the error code to the next 4 bytes
    for (i = 0; i < 4; i++) {
        output[1+i] = (code >> (8*i)) & 0xFF;
    }

    // Then copy the human-readable to the remaining bytes.
    for (i = 5; i < (MSG_LENGTH-2); i++) {
        if (i-5 < msg_len) {
            output[i] = message[i-5];
        }
        else {
            output[i] = '\0';
        }
    }
    output[MSG_LENGTH-2] = '\0';

    // Eventually, say that this was an error message again.
    output[MSG_LENGTH-1] = RESP_ERROR;
}


int main(int argc, char** argv) {
    int sock
      , bind_err
      , msg_len
      , client_id;
    socklen_t flen;
    struct sockaddr_in server_addr
                     , client_addr;
    struct client_list cur_served_clients;
    unsigned char msg_buffer[MSG_LENGTH];

    // List files in the local directory
    // If the list is empty, exit with error.

    // Server initialization
    sock = socket(AF_INET, SOCK_DGRAM, 0);
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

    create_client_list(&cur_served_clients);

    // Client requests handling loop
    while (1) {
        // Wait for a client request
        flen = sizeof(struct sockaddr_in);
        msg_len = recvfrom(sock, &msg_buffer, MSG_LENGTH, 0,
                (struct sockaddr *) &client_addr, &flen);
        if (msg_len < 0) {
            perror("Message reception failed.");
            continue;
        }

        // Check the form of the request
        if (msg_buffer[0] != msg_buffer[MSG_LENGTH-1]) {
            gen_error_message(msg_buffer, 0x0BADC0DE,
                    "I can has cheezburger?");
            msg_len = sendto(sock, msg_buffer, MSG_LENGTH, 0,
                    (struct sockaddr *) &client_addr,
                    sizeof(struct sockaddr_in));
            if (msg_len < 0) {
                perror("Message sending failed.");
            }
            continue;
        }

        // Determine client request
        switch (msg_buffer[0]) {
            case REQ_STREAMING:
                client_id = append_client(&cur_served_clients, &client_addr);
                if (client_id < 0) {
                    gen_error_message(msg_buffer, 0x00C0FFEE,
                            "I'm really sorry, but I'm swamped right now!");
                    msg_len = sendto(sock, msg_buffer, MSG_LENGTH, 0,
                            (struct sockaddr *) &client_addr,
                            sizeof(struct sockaddr_in));
                    if (msg_len < 0) {
                        perror("Message sending failed.");
                    }
                }
                break;
            case REQ_HEARTBEAT:
                client_id = notify_heartbeat(&cur_served_clients,
                        &client_addr);
                if (client_id < 0) {
                    gen_error_message(msg_buffer, 0xDEADBEA7,
                            "Undead alert, undead alert class! "
                            "You too believe in the flying "
                            "spaghetti monster ? You bobblehead !");
                }
                break;
            default:
                gen_error_message(msg_buffer, 0x0BADC0DE,
                        "I have no idea what I'm doing.");
                msg_len = sendto(sock, msg_buffer, MSG_LENGTH, 0,
                        (struct sockaddr *) &client_addr,
                        sizeof(struct sockaddr_in));
                if (msg_len < 0) {
                    perror("Message sending failed.");
                }
        }
    }

    close(sock);

    return EXIT_SUCCESS;
}
