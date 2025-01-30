#include "shared.h"

void _fatal_error(
    const char* message,
    const char* file,
    size_t line
) {
    printf("[FATAL]");
    if (message != NULL) {
        printf(": %s", message);
    }
    printf(" --> %s\n", strerror(errno));
    printf("\t\\---> %s:%d\n", file, (int)line);
    exit(errno);
}

int strincmp(
    const char* a,
    const char* b,
    size_t n    
) {
    if (a == NULL || b == NULL) {
        return 1;
    }

    while (n && *a && *b) {
        if (tolower(*a) != tolower(*b)) {
            return *a - *b;
        }

        n--;
        a++;
        b++;
    }

    return n != 0;
}

char* vformat(
    const char* fmt,
    va_list vargs
) {
    char* buffer;
    int buffer_len;
    
    /*Holy shit, segfaults everywhere, MSVC va_list implementation
    works differently, or just the implementation of vprintf...
    - https://lists.freebsd.org/pipermail/freebsd-questions/2014-November/262315.html
          VVVVVV*/
    va_list vargs_copy;
    va_copy(vargs_copy, vargs);

    buffer_len = vsnprintf(NULL, 0, fmt, vargs);

    if (buffer_len < 0) {
        fatal_error("Invalid vformat");
    }

    buffer = calloc(buffer_len + 1, sizeof(char));
    vsnprintf(buffer, buffer_len + 1, fmt, vargs_copy);

    return buffer;
}

char* format(
    const char* fmt,
    ...
) {
    va_list vargs;
    va_start(vargs, fmt);
    char* fmtd = vformat(fmt, vargs);
    va_end(vargs);
    return fmtd;
}