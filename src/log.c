#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include "log.h"

void w_log(const char* fmt, ...) {
        const char* home = getenv("HOME");
        char path[512];
        FILE* f;
        time_t t;
        struct tm* tm;
        char ts[32];
        va_list ap;
        snprintf(path, sizeof(path), "%s/.doi/doi.log", home ? home : "/tmp");
        f = fopen(path, "a");
        if (!f) return;
        t  = time(NULL);
        tm = localtime(&t);
        strftime(ts, sizeof(ts), "%H:%M:%S", tm);
        fprintf(f, "[%s] ", ts);
        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        va_end(ap);
        fprintf(f, "\n");
        fclose(f);
}
