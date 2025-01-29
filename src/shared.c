#include "shared.h"

void fatal_error(
    const char* message    
) {
    printf("[FATAL ERROR]: ");

    if (message != NULL) {
        printf("%s", message);
    }

    printf(" --> %s\n", strerror(errno));

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

    buffer_len = vsnprintf(NULL, 0, fmt, vargs);
    buffer = calloc(buffer_len + 1, sizeof(char));
    vsnprintf(buffer, buffer_len + 1, fmt, vargs);

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