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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MSG_LENGTH 128

#define REQ_STREAMING 0xDE
#define REQ_HEARTBEAT 0xDB
#define RESP_STREAMINFO 0xEA
#define RESP_DATA 0xAD
#define RESP_ERROR 0xEF

#define HEARTBEAT_FREQUENCY 10

#endif
