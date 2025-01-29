#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <memory.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#include "sqlite3.h"

#pragma once

// my last four id digits were restricted :( 
#define SERVER_PORT 12344 
#define BROADCAST_PORT 12345

#define MAGIC_TEXT "IAMHERE!"

#define LENGTHOF(_arr) (sizeof(_arr) / sizeof((_arr)[0]))

#define MAX(_a, _b) ((_a) > (_b) ? (_a) : (_b))
#define MIN(_a, _b) ((_a) < (_b) ? (_a) : (_b))

typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
typedef struct timeval timeval;

void fatal_error(
    const char* message
);

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