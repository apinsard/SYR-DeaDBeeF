/* L3info - SYR2 Project - SYR DeaDBeeF
 * ============================================================================
 * Deadbeef Header
 * ----------------------------------------------------------------------------
 * Common header for server and client
 * ----------------------------------------------------------------------------
 * Antoine Pinsard
 * Mar. 14, 2015
 */
#ifndef _DEADBEEF_H_
#define _DEADBEEF_H_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include <netinet/in.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "sysprog-audio/audio.h"

#define MSG_LENGTH 1024
#define DATA_LENGTH (MSG_LENGTH - 1 - 4 - 1)

#define REQ_STREAMING 0xDE
#define REQ_HEARTBEAT 0xDB
#define RESP_STREAMINFO 0xEA
#define RESP_DATA 0xAD
#define RESP_ERROR 0xEF

#define HEARTBEAT_FREQUENCY 10

int send_message(int, struct sockaddr_in*, unsigned char*);

#endif
