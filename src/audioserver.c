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
 * TODO:
 *  - Handle propagation of SIGTERM to the children
 * Antoine Pinsard
 * Mar. 15, 2015
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <glob.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <wait.h>
#include "audioserver.h"
#include "sysprog-audio/audio.h"


volatile sig_atomic_t done = 0;


void term(int signum) {
    done = 1;
}


/**
 * Create a memory shared empty client list and return a pointer to it.
 */
struct client_list* create_client_list() {
    int i
      , shmid;
    struct client_list* list;

    shmid = shmget(IPC_PRIVATE, sizeof(struct client_list), 0600);
    list = (struct client_list*) shmat(shmid, 0, 0);

    list->shmid = shmid;
    list->nb_clients = 0;
    list->handler = -1;
    for (i = 0; i < MAX_NB_CLIENTS; i++) {
        list->clients[i] = NULL;
    }

    return list;
}


/**
 * Remove all clients from the list and free shared memory.
 * Send a message to clients that were still served if any.
 */
void destroy_client_list(struct client_list* list, int sock) {
    int i
      , shmid;
    struct sockaddr_in* addr;

    assert(list != NULL);

    shmid = list->shmid;

    for (i = 0; i < MAX_NB_CLIENTS; i++) {
        addr = remove_client(list, i);
        if (addr != NULL) {
            send_error_message(sock, addr, 0xDEADDEAD,
                               "Sorry, I gotta go. My mum's shouting at me.");
        }
    }
    list->nb_clients = 0;
    list->shmid = -1;

    shmdt((void *) list);
    wait(NULL);
    shmctl(shmid, IPC_RMID, 0);
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
 * Return NULL if there was no client with the given identifier.
 */
struct sockaddr_in* remove_client(struct client_list* list, int client_id) {
    struct sockaddr_in* addr;

    assert(list != NULL);
    assert(client_id >= 0);
    assert(client_id < MAX_NB_CLIENTS);

    if (list->clients[client_id] == NULL) {
        return NULL;
    }

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
 *
 */
void send_file_to_client(struct client_list* list, int client_id,
                         char* filename, int socket)
{
    
}


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
        perror("Message sending failed.");
    }

    return msg_len;
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


/**
 * Wrapper of send_message() to format and send an error message.
 * See : send_message(int, struct sockaddr_in*, unsigned char*)
 */
int send_error_message(int sock, struct sockaddr_in* dest, unsigned int code,
                       const char* message)
{
    unsigned char buffer[MSG_LENGTH];

    assert(dest != NULL);

    gen_error_message(buffer, code, message);

    return send_message(sock, dest, buffer);
}


/**
 * Retrieve the filename part from a client streaming request.
 *
 * Return a pointer to the filename or NULL if malloc failed.
 */
char* retrieve_filename(unsigned char* request) {
    char* filename;
    int len
      , i;

    assert(request != NULL);

    // Calculate the length of the filename
    for (len = 1; (len < MSG_LENGTH-1) && (request[len] != '\0'); len++);
    filename = malloc(len * sizeof(char));
    if (filename == NULL) {
        return NULL;
    }

    // Fill the filename.
    for (i = 0; i < len-1; i++) {
        filename[i] = request[i+1];
    }
    // Make sure the last character is an EOS marker.
    filename[len-1] = '\0';

    return filename;
}


/**
 * Return the list of wave files in the current directory, terminated by a NULL
 * marker.
 * Return NULL if an error occured or if the current directory does not contain
 * any wave file.
 */
char** get_available_files_list() {
    char** filelist;
    int i;
    glob_t pglob;
    int glob_res;

    // Search wave files
    glob_res = glob("*.wav", GLOB_ERR, NULL, &pglob);
    if (glob_res != 0) {
        return NULL;
    }

    // Copy found file names to the filelist.
    filelist = malloc((pglob.gl_pathc+1) * sizeof(char*));
    if (filelist == NULL) {
        return NULL;
    }
    for (i = 0; i < pglob.gl_pathc+1; i++) {
        filelist[i] = pglob.gl_pathv[i];
    }

    return filelist;
}


/**
 * Determine if a file is available in a list of filenames.
 *
 * Assume that the list available_files is terminated by a NULL marker.
 *
 * Return 1 if it exists, 0 otherwise.
 */
int file_is_available(char* filename, char** available_files) {
    char* name;

    assert(filename != NULL);

    for (name = available_files[0]; name != NULL; name++) {
        if (strlen(name) == strlen(filename)) {
            if (strncmp(name, filename, strlen(filename)) == 0) {
                return 1;
            }
        }
    }

    return 0;
}


int main(int argc, char** argv) {
    int sock
      , bind_err
      , msg_len
      , client_id;
    socklen_t flen;
    pid_t pid;
    struct sockaddr_in server_addr
                     , client_addr;
    struct client_list* cur_served_clients;
    unsigned char msg_buffer[MSG_LENGTH];
    char* filename;
    char** available_files;
    struct sigaction action;

    // Handle signals
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = term;
    sigaction(SIGTERM, &action, NULL);

    // List files in the local directory
    // If the list is empty, exit with error.
    available_files = get_available_files_list();
    if (available_files == NULL) {
        perror("No wave files found in the current directory.");
        exit(EXIT_FAILURE);
    }

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

    cur_served_clients = create_client_list();

    // Client requests handling loop
    while (!done) {
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
            send_error_message(sock, &client_addr, 0x0BADC0DE,
                               "I can has cheezburger?");
            continue;
        }

        // Determine client request
        switch (msg_buffer[0]) {
            case REQ_STREAMING:
                client_id = append_client(cur_served_clients, &client_addr);
                if (client_id < 0) {
                    send_error_message(sock, &client_addr, 0x00C0FFEE,
                                       "I'm really sorry, but I'm swamped "
                                       "right now!");
                }
                else {
                    filename = retrieve_filename(msg_buffer);
                    if (filename == NULL) {
                        perror("Dynamic allocation failed!");
                    }
                    if (!file_is_available(filename, available_files)) {
                        send_error_message(sock, &client_addr, 0xDEADF11E,
                                           "Sorry but the requested file is "
                                           "not available.");
                    }
                    pid = fork();
                    if (pid < 0) {
                        send_error_message(sock, &client_addr, 0x00C0FFEE,
                                           "I am currently facing some issues "
                                           "and I can't satisfy you request "
                                           "right now. So sorry for that.");
                        remove_client(cur_served_clients, client_id);
                        free(filename);
                    }
                    else if (pid == 0) {
                        send_file_to_client(cur_served_clients, client_id,
                                            filename, sock);
                    }
                }
                break;
            case REQ_HEARTBEAT:
                client_id = notify_heartbeat(cur_served_clients,
                                             &client_addr);
                if (client_id < 0) {
                    send_error_message(sock, &client_addr, 0xDEADBEA7,
                                       "Undead alert, undead alert class! "
                                       "You too believe in the flying "
                                       "spaghetti monster ? You bobblehead !");
                }
                break;
            default:
                send_error_message(sock, &client_addr, 0x0BADC0DE,
                                   "I have no idea what I'm doing.");
        }
    }

    free(available_files);
    destroy_client_list(cur_served_clients, sock);

    close(sock);

    return EXIT_SUCCESS;
}
