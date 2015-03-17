/* Distributed under the terms of the GNU General Public License v2 */
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
 * Mar. 17, 2015
 */
#ifndef _AUDIOSERVER_H_
#define _AUDIOSERVER_H_

#include <glob.h>
#include <math.h>
#include <signal.h>
#include <sys/sem.h>
#include "deadbeef.h"

#define MAX_NB_CLIENTS 5
#define HEARTBEAT_THRESHOLD (5 * HEARTBEAT_FREQUENCY)

struct client {
    int shmid; // Share memory identifier
    struct sockaddr_in* addr;
    pid_t handler;
    int heartbeat_counter;
    // Each message from the client causes the counter to be reset to
    // HEARTBEAT_THRESHOLD.
    // Each message sent to the client causes the counter to be decreased by 1.
    // Whenever the counter reaches 0, the communication is aborted and a last
    // error message is sent with code 0xDEADBEA7.
};

struct client_list {
    int shmid; // Share memory identifier
    int nb_clients;
    struct client* clients[MAX_NB_CLIENTS];
};

void term(int);

struct client_list* create_client_list();
void destroy_client_list(struct client_list*, int);
int append_client(struct client_list*, struct sockaddr_in*);
struct sockaddr_in* remove_client(struct client_list*, int, int);
int notify_heartbeat(struct client_list*, struct sockaddr_in*);
void send_file_to_client(struct client_list*, int, char*, int, int);

void gen_error_message(unsigned char*, unsigned int, const char*);
int send_error_message(int, struct sockaddr_in*, unsigned int, const char*);

char* retrieve_filename(unsigned char*);
char** get_available_files_list();
int file_is_available(char*, char**);

#endif
