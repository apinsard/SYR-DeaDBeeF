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
#ifndef _AUDIOSERVER_H_
#define _AUDIOSERVER_H_

#define MAX_NB_CLIENTS 5
#define MAX_MSG_LENGTH 128

struct cli_list {
    int nb_clients;
    struct sockaddr_in* list[MAX_NB_CLIENTS];
};

int append_client(struct sockaddr_in*);
int remove_client(struct sockaddr_in*);

#endif
