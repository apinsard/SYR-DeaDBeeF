/* Distributed under the terms of the GNU General Public License v2 */
/* L3info - SYR2 Project - SYR DeaDBeeF
 * ============================================================================
 * Audio Client
 * ----------------------------------------------------------------------------
 * The client sends the name of the file it whishes to play to the server. It
 * receives the data stream from the server and plays the sound while receiving
 * the data.
 * ----------------------------------------------------------------------------
 * Antoine Pinsard
 * Mar. 17, 2015
 */
#ifndef _AUDIOCLIENT_H_
#define _AUDIOCLIENT_H_

#include <arpa/inet.h>
#include "deadbeef.h"

void print_errmess(unsigned char*);

#endif
