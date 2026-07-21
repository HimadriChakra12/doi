/* log.c — append-only timestamped log writer
 *
 * Opens, writes, and closes the log file on every call.  Inefficient
 * for high-frequency logging, but doid is low-traffic and this keeps
 * the implementation trivially simple with no global state.
 */
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include "log.h"
#include "../config.h"

void log_write(const char *fmt, ...) {
	const char *home = getenv("HOME");
	char   path[512];
	char   ts[32];
	time_t t;
	struct tm *tm;
	FILE   *f;
	va_list ap;

	snprintf(path, sizeof(path), "%s/%s",
	         home ? home : "/tmp", LOG_FILE);

	f = fopen(path, "a");
	if (!f) return;

	t  = time(NULL);
	tm = localtime(&t);
	strftime(ts, sizeof(ts), "%H:%M:%S", tm);

	fprintf(f, "[%s] ", ts);
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fputc('\n', f);
	fclose(f);
}
