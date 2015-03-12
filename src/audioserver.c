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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "player.h"
#include "sysprog-audio/audio.h"

int main(int argc, char** argv) {

}