#ifndef XPS_H
#define XPS_H

#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <asm-generic/socket.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include "lib/vec/vec.h"

#define DEFAULT_BACKLOG 64
#define MAX_EPOLL_EVENTS 32
#define DEFAULT_BUFFER_SIZE 100000
#define BUFF_SIZE 10000
#define DEFAULT_NULLS_THRESH 32

// Error constants
#define OK 0            // Success
#define E_FAIL -1       // Un-recoverable error
#define E_AGAIN -2      // Try again
#define E_NEXT -3       // Do next
#define E_NOTFOUND -4   // File not found
#define E_PERMISSION -5 // File permission denied
#define E_EOF -6        // End of file reached

typedef unsigned char u_char;
typedef unsigned int u_int;
typedef unsigned long u_long;

struct xps_core_s;
struct xps_loop_s;
struct xps_listener_s;
struct xps_connection_s;
struct xps_buffer_s;
struct xps_buffer_list_s;

typedef struct xps_core_s xps_core_t;
typedef struct xps_loop_s xps_loop_t;
typedef struct xps_listener_s xps_listener_t;
typedef struct xps_connection_s xps_connection_t;
typedef struct xps_buffer_s xps_buffer_t;
typedef struct xps_buffer_list_s xps_buffer_list_t;

typedef void (*xps_handler_t)(void *ptr);

#include "core/xps_core.h"
#include "core/xps_loop.h"
#include "network/xps_connection.h"
#include "network/xps_listener.h"
#include "utils/xps_logger.h"
#include "utils/xps_utils.h"
#include "utils/xps_buffer.h"

#endif