/**
 * mb64_log.h — Structured logging for mb64 instrumentation stages.
 *
 * Format: [PREFIX] key=value key2=value2 ...
 * Output: simultaneously to stdout and a log file (if mb64_log_init is called).
 *
 * Standard stage tags (one per processing stage):
 *   MB64_PARSE      — binary read + header decode
 *   MB64_GEOM_GEN   — tile array decoded into typed structs
 *   MB64_OBJ_SPAWN  — object array decoded
 *   MB64_COLLISION  — collision surface data built
 *   MB64_RENDER     — display list built / geo node injected
 *   MB64_INTEGRATION — sm64coopdx glue load/free lifecycle
 *
 * Usage:
 *   mb64_log_init("logs/mb64_instrument.log");
 *   MB64_LOG(MB64_LOG_PARSE, "tile_count=%u object_count=%u", tc, oc);
 *   mb64_log_close();
 */

#ifndef MB64_LOG_H
#define MB64_LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/* Stage tag string constants — use these with MB64_LOG() */
#define MB64_LOG_PARSE       "MB64_PARSE"
#define MB64_LOG_GEOM_GEN    "MB64_GEOM_GEN"
#define MB64_LOG_OBJ_SPAWN   "MB64_OBJ_SPAWN"
#define MB64_LOG_COLLISION   "MB64_COLLISION"
#define MB64_LOG_RENDER      "MB64_RENDER"
#define MB64_LOG_INTEGRATION "MB64_INTEGRATION"

static FILE *_mb64_log_file = NULL;

/* Open the log file for dual-sink output. Call once at startup.
 * Opens in write mode ("w") so each run produces a fresh log that
 * exactly matches stdout — required by the dual-sink equality test. */
static inline int mb64_log_init(const char *path) {
    _mb64_log_file = fopen(path, "w");
    return (_mb64_log_file != NULL) ? 0 : -1;
}

/* Close the log file. */
static inline void mb64_log_close(void) {
    if (_mb64_log_file) { fclose(_mb64_log_file); _mb64_log_file = NULL; }
}

/* Internal: emit one formatted log line to stdout + file. */
static inline void _mb64_log_emit(const char *prefix, const char *fmt, va_list ap) {
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    printf("[%s] %s\n", prefix, buf);
    fflush(stdout);
    if (_mb64_log_file) {
        fprintf(_mb64_log_file, "[%s] %s\n", prefix, buf);
        fflush(_mb64_log_file);
    }
}

/* Log a structured message: MB64_LOG("MB64_PARSE", "tile_count=%u", n); */
static inline void mb64_log(const char *prefix, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    _mb64_log_emit(prefix, fmt, ap);
    va_end(ap);
}

/* Convenience macro so callers can write MB64_LOG("PREFIX", "k=%d", v) */
#define MB64_LOG(prefix, ...) mb64_log((prefix), __VA_ARGS__)

#endif /* MB64_LOG_H */
