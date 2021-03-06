/* Distributed under the terms of the GNU General Public License v2 */
/* L3info - SYR2 Project - SYR DeaDBeeF
 * ============================================================================
 * Player
 * ----------------------------------------------------------------------------
 * The player reads a WAV file passed as argument.
 * ----------------------------------------------------------------------------
 * Antoine Pinsard
 * Mar. 8, 2015
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "player.h"
#include "sysprog-audio/audio.h"


int main(int argc, char** argv) {

    char* filename;
    FILE* file;
    char* buffer;
    unsigned long file_length;
    int sample_rate;
    int sample_size;
    int channels;
    int audout_fd;

    if (argc != 2) {
        fprintf(stderr, "%s expects exactly one argument. %d found.\n",
                argv[0], argc-1);
        exit(1);
    }

    filename = argv[1];

    if (aud_readinit(filename, &sample_rate, &sample_size, &channels) < 0) {
        perror("Error while attempting to read the audio file");
        exit(EXIT_FAILURE);
    }

    audout_fd = aud_writeinit(sample_rate, sample_size, channels);
    if (audout_fd < 0) {
        perror("Error while attempting to play the audio file");
        exit(EXIT_FAILURE);
    }

    file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr,
                "An error happend while attempting to open %s for reading.\n",
                filename);
        perror("");
        exit(EXIT_FAILURE);
    }

    // Get file length
    fseek(file, 0, SEEK_END);
    file_length = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate buffer
    buffer = (char*) malloc(file_length+1);
    if (buffer == NULL) {
        perror("Memory error");
        exit(EXIT_FAILURE);
    }

    // Read file into buffer
    fread(buffer, file_length, 1, file);

    fclose(file);

    write(audout_fd, buffer, file_length * sizeof(char));

    free(buffer);

    return EXIT_SUCCESS;
}
