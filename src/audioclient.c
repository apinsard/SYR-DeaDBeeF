/* L3info - SYR2 Project - SYR DeaDBeeF
 * ============================================================================
 * Audio Client
 * ----------------------------------------------------------------------------
 * The client sends the name of the file it whishes to play to the server. It
 * receives the data stream from the server and plays the sound while receiving
 * the data.
 * ----------------------------------------------------------------------------
 * Antoine Pinsard
 * Mar. 12, 2015
 */
#include "audioclient.h"

int main(int argc, char** argv) {
    int sock
      , msg_len
      , nb_packets
      , channels
      , sample_size
      , sample_rate
      , audout_fd
      , shmid
      , sel
      , i
      , packet_id
      , packets_received;
    socklen_t flen;
    pid_t pid;
    fd_set read_set;
    struct timeval timeout;
    struct sockaddr_in server_addr;
    unsigned char msg_buffer[MSG_LENGTH];
    unsigned char* data_buffer;

    // Check arguments
    if (argc < 3) {
        fprintf(stderr, "Usage: audioclient <server_host_name> <file_name>\n");
        exit(EXIT_FAILURE);
    }

    // Open connection to the server
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1664);
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Send the request
    msg_buffer[0] = REQ_STREAMING;
    for (i = 1; i < MSG_LENGTH-2; i++) {
        if (i-1 < strlen(argv[2])) {
            msg_buffer[i] = argv[2][i-1];
        }
        else {
            msg_buffer[i] = '\0';
        }
    }
    msg_buffer[MSG_LENGTH-2] = '\0';
    msg_buffer[MSG_LENGTH-1] = REQ_STREAMING;
    msg_len = send_message(sock, &server_addr, msg_buffer);
    if (msg_len < 0) {
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Wait for the answer
    flen = sizeof(struct sockaddr_in);
    msg_len = recvfrom(sock, msg_buffer, MSG_LENGTH, 0,
                       (struct sockaddr*) &server_addr, &flen);
    if (msg_len < 0) {
        perror("Message reception failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    if (msg_buffer[0] != msg_buffer[MSG_LENGTH-1]) {
        fprintf(stderr, "Bad formated message received");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Parse answer
    sample_rate = 0;
    sample_size = 0;
    channels = 0;
    nb_packets = 0;
    switch (msg_buffer[0]) {
        case RESP_ERROR:
            fprintf(stderr, "error\n");
            close(sock);
            exit(EXIT_FAILURE);
            break;
        case RESP_STREAMINFO:
            for (i = 0; i < 4; i++) {
                sample_rate += (msg_buffer[1+i] << (8*i));
                sample_size += (msg_buffer[5+i] << (8*i));
                channels += (msg_buffer[9+i] << (8*i));
                nb_packets += (msg_buffer[13+i] << (8*i));
            }
            printf("sample_rate=%d, sample_size=%d, channels=%d, "
                    "nb_packets=%d\n", sample_rate, sample_size, channels,
                    nb_packets);

            break;
        default:
            fprintf(stderr, "Unhandled response code: %x\n",
                    msg_buffer[0] & 0xff);
            close(sock);
            exit(EXIT_FAILURE);
    }

    // Init audio file descriptor
    audout_fd = aud_writeinit(sample_rate, sample_size, channels);
    if (audout_fd < 0) {
        perror("Error while attempting to play the audio file");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Create a buffer to store received data
    shmid = shmget(IPC_PRIVATE,
                   nb_packets * DATA_LENGTH * sizeof(unsigned char),
                   0600);
    if (shmid == -1) {
        perror("Unable to allocate shared memory");
        close(sock);
        exit(EXIT_FAILURE);
    }
    data_buffer = (unsigned char*) shmat(shmid, NULL, 0);
    if (data_buffer == NULL) {
        perror("Shared memory attachment failed");
        shmctl(shmid, IPC_RMID, NULL);
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Create a subprocess to read the buffer and write it to the audio fd.
    // The parent handles messages reception from the server.
    pid = fork();
    if (pid == -1) {
        perror("Fork failed");
        shmdt((void*) data_buffer);
        shmctl(shmid, IPC_RMID, NULL);
        close(sock);
        exit(EXIT_FAILURE);
    }
    else if (pid == 0) {
        packet_id = -1;
        for (packets_received = 0; packets_received < nb_packets;
             packets_received++)
        {
            FD_ZERO(&read_set);
            FD_SET(sock, &read_set);
            timeout.tv_sec = 5;
            timeout.tv_usec = 0;
            sel = select(sock+1, &read_set, NULL, NULL, &timeout);
            if (sel < 0) {
                perror("Timeout error");
                continue;
            }
            if (sel == 0) {
                fprintf(stderr, "Server connection timeout.\n"
                                "Received %d/%d packets\n", packets_received,
                        nb_packets);
                break;
            }
            msg_len = recvfrom(sock, msg_buffer, MSG_LENGTH, 0,
                               (struct sockaddr*) &server_addr, &flen);
            if (msg_len < 0) {
                perror("Message reception failed");
                continue;
            }
            packet_id = 0;
            for (i = 0; i < 4; i++) {
                packet_id += (msg_buffer[1+i] << (8*i));
            }
            for (i = 0; i < DATA_LENGTH; i++) {
                data_buffer[(packet_id*DATA_LENGTH)+i] = msg_buffer[5+i];
            }
            if (packets_received % HEARTBEAT_FREQUENCY == 0) {
                msg_buffer[0] = REQ_HEARTBEAT;
                msg_buffer[MSG_LENGTH-1] = REQ_HEARTBEAT;
                send_message(sock, &server_addr, msg_buffer);
            }
        }
    }
    else {
        for (i = 0; i < nb_packets; i++) {
            write(audout_fd, data_buffer+(i*DATA_LENGTH),
                  DATA_LENGTH * sizeof(unsigned char));
        }
    }

    shmdt((void*)data_buffer);
    close(sock);

    printf("%d: I'm done\n", pid);

    if (pid == 0) {
        wait(NULL);
        shmctl(shmid, IPC_RMID, NULL);
    }

    return EXIT_SUCCESS;
}
