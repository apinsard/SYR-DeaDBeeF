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
#ifndef _AUDIOSERVER_H_
#define _AUDIOSERVER_H_

#include "deadbeef.h"

#define MAX_NB_CLIENTS 5
#define HEARTBEAT_THRESHOLD (5 * HEARTBEAT_FREQUENCY)

struct client {
    struct sockaddr_in* addr;
    int heartbeat_counter;
    // Each message from the client causes the counter to be reset to
    // HEARTBEAT_THRESHOLD.
    // Each message sent to the client causes the counter to be decreased by 1.
    // Whenever the counter reaches 0, the communication is aborted and a last
    // error message is sent with code 0xDEADBEA7.
};

struct client_list {
    int nb_clients;
    struct client* clients[MAX_NB_CLIENTS];
};

void create_client_list(struct client_list*);
int append_client(struct client_list*, struct sockaddr_in*);
struct sockaddr_in* remove_client(struct client_list*, int);
int notify_heartbeat(struct client_list*, struct sockaddr_in*);

void gen_error_message(unsigned char*, unsigned int, const char*);

#endif
