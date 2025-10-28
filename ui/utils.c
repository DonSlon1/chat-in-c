#include <time.h>
#include <stdio.h>

void get_current_time(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(buffer, size, "%02d:%02d", t->tm_hour, t->tm_min);
}
