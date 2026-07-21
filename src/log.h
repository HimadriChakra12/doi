#ifndef DOI_LOG_H
#define DOI_LOG_H

/* log_write: append a printf-style line to $HOME/.doi/doi.log.
 * Safe to call from signal handlers? No — uses stdio.  Call only from
 * the main daemon process, never from render children.               */
void log_write(const char *fmt, ...);

#endif /* DOI_LOG_H */
