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
 * Mar. 15, 2015
 */
#include "audioserver.h"


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
        addr = remove_client(list, i, 1);
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
    client = malloc(sizeof(struct client));
    if (client == NULL) {
        perror("Dynamic allocation failed");
        return -2;
    }
    client->addr = addr;
    client->handler = -1;
    client->heartbeat_counter = HEARTBEAT_THRESHOLD;

    // Store then new client
    list->clients[client_id] = client;
    list->nb_clients++;

    return client_id;
}


/**
 * Remove the client with the given ID from the currently served clients list.
 *
 * Kill the process handler if kill_handler is set;
 *
 * Return the removed client address.
 * Return NULL if there was no client with the given identifier.
 */
struct sockaddr_in* remove_client(struct client_list* list, int client_id,
                                  int kill_handler)
{
    struct sockaddr_in* addr;
    pid_t handler;

    assert(list != NULL);
    assert(client_id >= 0);
    assert(client_id < MAX_NB_CLIENTS);

    if (list->clients[client_id] == NULL) {
        return NULL;
    }

    handler = list->clients[client_id]->handler;

    addr = list->clients[client_id]->addr;
    free(list->clients[client_id]);
    list->clients[client_id] = NULL;

    if (kill_handler && handler > 0) {
        kill(handler, SIGKILL);
    }

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
                         char* filename, int sock)
{
    FILE* file;
    unsigned long file_length;
    int sample_rate
      , sample_size
      , channels
      , nb_packets
      , i
      , j;
    struct client* my_client;
    unsigned char msg_buffer[MSG_LENGTH];
    char* file_buffer;

    assert(list != NULL);
    assert(list->clients[client_id] != NULL);
    assert(filename != NULL);

    my_client = list->clients[client_id];

    if (aud_readinit(filename, &sample_rate, &sample_size, &channels) < 0) {
        send_error_message(sock, my_client->addr, 0xDEADF11E,
                           "An error occured while attempting to read the "
                           "requested filed.");
        perror("Error while attempting to read the audio file");
        remove_client(list, client_id, 0);
        shmdt((void*) list);
        free(filename);
        exit(EXIT_FAILURE);
    }

    file = fopen(filename, "rb");
    if (file == NULL) {
        send_error_message(sock, my_client->addr, 0xDEADF11E,
                           "An error occured while attempting to read the "
                           "requested filed.");
        fprintf(stderr,
                "An error happened while attempting to open %s for reading.\n",
                filename);
        perror("");
        remove_client(list, client_id, 0);
        shmdt((void*) list);
        free(filename);
        exit(EXIT_FAILURE);
    }
    free(filename);

    // Get file length
    fseek(file, 0, SEEK_END);
    file_length = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate buffer
    file_buffer = (char*) malloc(file_length+1);
    if (file_buffer == NULL) {
        perror("Memory error");
        exit(EXIT_FAILURE);
    }

    // Read file into buffer
    fread(file_buffer, file_length, 1, file);

    fclose(file);

    nb_packets = file_length / DATA_LENGTH;
    if (file_length % DATA_LENGTH != 0)
        nb_packets++;

    // Build the stream info packet
    msg_buffer[0] = RESP_STREAMINFO;
    for (i = 0; i < 4; i++) {
        msg_buffer[1+i] = (sample_rate >> (8*i)) & 0xFF;
        msg_buffer[5+i] = (sample_size >> (8*i)) & 0xFF;
        msg_buffer[9+i] = (channels >> (8*i)) & 0xFF;
        msg_buffer[13+i] = (nb_packets >> (8*i)) & 0xFF;
    }
    // Pad the remaining part of the message with null characters to avoid data
    // leaks.
    for (i = 17; i < MSG_LENGTH-2; i++) {
        msg_buffer[i] = '\0';
    }
    msg_buffer[MSG_LENGTH-1] = RESP_STREAMINFO;

    send_message(sock, my_client->addr, msg_buffer);

    for (i = 0; i < nb_packets; i++) {
        if (i % 1000 == 0)
            printf("Sending packet %d/%d\n", i, nb_packets);
        msg_buffer[0] = RESP_DATA;
        for (j = 0; j < 4; j++) {
            msg_buffer[1+j] = (i >> (8*i)) & 0xFF;
        }
        for (j = 0; j < DATA_LENGTH; j++) {
            msg_buffer[5+j] = file_buffer[(i*DATA_LENGTH)+j];
        }
        msg_buffer[MSG_LENGTH-1] = RESP_DATA;
        send_message(sock, my_client->addr, msg_buffer);
        usleep(10000);
    }

    free(file_buffer);

    remove_client(list, client_id, 0);
    shmdt((void*) list);

    exit(EXIT_SUCCESS);
}


/**
 * Generate an error message with respect to the protocol.
 * The generated message is written to output.
 *
 * Valid error codes are:
 *  - 0xDEADBEA7: Heartbeat waiting timeout
 *  - 0xDEADDEAD: Server received a SIGTERM or SIGINT
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
    int i
      , glob_res;
    glob_t pglob;

    // Search wave files
    glob_res = glob("*.wav", GLOB_ERR, NULL, &pglob);
    if (glob_res != 0) {
        return NULL;
    }

    // Copy found file names to the filelist.
    filelist = malloc((pglob.gl_pathc+1) * sizeof(char*));
    if (filelist == NULL) {
        globfree(&pglob);
        return NULL;
    }
    for (i = 0; i <= pglob.gl_pathc; i++) {
        if (pglob.gl_pathv[i] == NULL) {
            filelist[i] = NULL;
        }
        else {
            filelist[i] = strdup(pglob.gl_pathv[i]);
            if (filelist[i] == NULL) {
                globfree(&pglob);
                return NULL;
            }
        }
    }

    globfree(&pglob);
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
      , client_id
      , i;
    socklen_t flen;
    pid_t pid;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    struct client_list* cur_served_clients;
    unsigned char msg_buffer[MSG_LENGTH];
    char* filename;
    char** available_files;
    struct sigaction action;

    // Handle signals
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = term;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    // List files in the local directory
    // If the list is empty, exit with error.
    available_files = get_available_files_list();
    if (available_files == NULL) {
        perror("No wave files found in the current directory");
        exit(EXIT_FAILURE);
    }

    // Server initialization
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1664);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind_err = bind(sock, (struct sockaddr *) &server_addr,
                    sizeof(struct sockaddr_in));

    if (bind_err < 0) {
        perror("Failed to bind socket");
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
            perror("Message reception failed");
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
                        perror("Dynamic allocation failed");
                        break;
                    }
                    if (!file_is_available(filename, available_files)) {
                        send_error_message(sock, &client_addr, 0xDEADF11E,
                                           "Sorry but the requested file is "
                                           "not available.");
                    }
                    else {
                        pid = fork();
                        if (pid < 0) {
                            send_error_message(sock, &client_addr, 0x00C0FFEE,
                                               "I am currently facing some "
                                               "issues and I can't satisfy "
                                               "you request right now. So "
                                               "sorry for that.");
                            remove_client(cur_served_clients, client_id, 0);
                        }
                        else if (pid == 0) {
                            send_file_to_client(cur_served_clients, client_id,
                                                filename, sock);
                        }
                        else {
                            cur_served_clients->clients[client_id]
                                ->handler = pid;
                        }
                    }
                    free(filename);
                }
                break;
            case REQ_HEARTBEAT:
                client_id = notify_heartbeat(cur_served_clients, &client_addr);
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

    for (i=0; available_files[i] != NULL; i++) {
        free(available_files[i]);
    }
    free(available_files);
    destroy_client_list(cur_served_clients, sock);

    close(sock);

    return EXIT_SUCCESS;
}
