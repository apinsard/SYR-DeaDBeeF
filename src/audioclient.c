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
      , i
      , j;
    socklen_t flen;
    struct sockaddr_in server_addr;
    unsigned char msg_buffer[MSG_LENGTH];
    char data_buffer[DATA_LENGTH];

    if (argc < 3) {
        fprintf(stderr, "Usage: audioclient <server_host_name> <file_name>\n");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1664);
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

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

    audout_fd = aud_writeinit(sample_rate, sample_size, channels);
    if (audout_fd < 0) {
        perror("Error while attempting to play the audio file");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < nb_packets; i++) {
        if (i % 1000 == 0)
            printf("Waiting packet %d/%d\n", i, nb_packets);
        msg_len = recvfrom(sock, msg_buffer, MSG_LENGTH, 0,
                           (struct sockaddr*) &server_addr, &flen);

        if (msg_len < 0) {
            perror("Message reception failed");
            close(sock);
            exit(EXIT_FAILURE);
        }
        for (j = 0; j < DATA_LENGTH; j++) {
            data_buffer[j] = msg_buffer[5+j];;
        }
        write(audout_fd, data_buffer, DATA_LENGTH * sizeof(char));
    }

    close(sock);

    return EXIT_SUCCESS;
}
