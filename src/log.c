#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "../config.h"

void w_log(const char* fmt, ...) {
        va_list ap;
        time_t t = time(NULL);
        struct tm* lt = localtime(&t);
        char ts[20];
        char path[512];
        const char* home;
        FILE* f;

        home = getenv("HOME");
        if (!home) {
                fprintf(stderr, "ERROR: HOME not set.\n");
                return;
        }
        snprintf(path, sizeof(path), "%s/%s", home, DOI_LOG_FILE);

        f = fopen(path, "a");
        if (!f) {
                fprintf(stderr, "ERROR: unable to open log file.\n");
                return;
        }
        strftime(ts, sizeof(ts), "%Y/%m/%d %H:%M:%S", lt);
        fprintf(f, "%s: ", ts);
        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        va_end(ap);
        fprintf(f, "\n");
        fclose(f);
}
