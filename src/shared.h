#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <memory.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>

#include "sqlite3.h"

#pragma once

// my last four id digits were restricted :( 
#define SERVER_PORT 12344 
#define BROADCAST_PORT 12345

#define MAGIC_EOR '\x1'

#define MAGIC_TEXT "IAMHERE!"

#define LENGTHOF(_arr) (sizeof(_arr) / sizeof((_arr)[0]))

// why EWOULDBLOCK isnt standard is beyond me
#define FD_WOULDBLOCK (errno == EWOULDBLOCK || errno == EAGAIN) 

#define MAX(_a, _b) ((_a) > (_b) ? (_a) : (_b))
#define MIN(_a, _b) ((_a) < (_b) ? (_a) : (_b))

typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
typedef struct timeval timeval;

void _fatal_error(
    const char* message,
    const char* file,
    size_t line
);

#define fatal_error(_msg) _fatal_error((_msg), __FILE__, __LINE__)

#define fatal_assert(_expr, _msg) { if (!(_expr)) { fatal_error(_msg); } }

// MSVC had it right
int strincmp(
    const char* a,
    const char* b,
    size_t n    
);

char* vformat(
    const char* fmt,
    va_list vargs
);

char* format(
    const char* fmt,
    ...
);