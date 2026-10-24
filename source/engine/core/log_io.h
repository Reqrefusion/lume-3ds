#ifndef LUME_LOG_IO_H
#define LUME_LOG_IO_H
#include <stdarg.h>
/* Formatting is bounded; file writes belong to the application's I/O owner. */
void lume_log_io_init(const char *path);
void lume_log_io_attach(void (*wake)(void));
void lume_log_io_writev(const char *format,va_list args);
void lume_log_io_drain(void);
#endif
