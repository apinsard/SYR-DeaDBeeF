/* L3info - SYR2 - Project - Audio Streaming Player
 * =============================================================================
 * Player
 * -----------------------------------------------------------------------------
 * The player reads a WAV file passed as argument.
 * -----------------------------------------------------------------------------
 * Antoine Pinsard
 * Feb. 26, 2015
 */

#include <stdlib.h>
#include <stdio.h>

#include "player.h"
#include "sysprog-audio/audio.h"

int main(int argc, char** argv) {

  char* filename;
  FILE* file;
  int   sample_rate;
  int   sample_size;
  int   channels;
  int   audout_fd;
  char  buffer[BUF_SIZE];

  if (argc != 2) {
    fprintf(stderr, "%s expects exactly one argument. %d found.\n", argv[0], argc-1);
    exit(1);
  }

  filename = argv[1];

  if (aud_readinit(filename, &sample_rate, &sample_size, &channels) < 0) {
    perror("Error while attempting to read the audio file.");
    exit(1);
  }

  audout_fd = aud_writeinit(sample_rate, sample_size, channels);
  if (audout_fd < 0) {
    perror("Error while attempting to play the audio file.");
    exit(1);
  }

  file = fopen(filename, "r");
  if (file == NULL) {
    fprintf(stderr, "An error happend while attempting to open %s for reading.\n", filename);
    perror("");
    exit(1);
  }

  while (fread(buffer, BUF_SIZE, 1, ) {
    
  }

  return 0;
}

