/* =============================================================================
 * utils.c — Shared utility functions
 *
 * Responsibilities
 *   • Centralised error reporting (die, warn).
 *   • String manipulation helpers (trim_whitespace).
 *
 * Keeping utilities separate means every module can call them without
 * pulling in unrelated headers.
 * ============================================================================= */

#include "shell.h"

/* ── Error helpers ────────────────────────────────────────────────────────── */

/*
 * die()
 * -----
 * Print a descriptive error message and terminate the process.
 *
 * perror() prepends msg with the string representation of the current errno
 * value, e.g.  "fork: Cannot allocate memory".
 *
 * Use for unrecoverable errors (e.g. allocation failure at startup).
 */
void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

/*
 * warn()
 * ------
 * Print a descriptive error message and CONTINUE execution.
 *
 * Use for recoverable errors (e.g. a single fork failure in the REPL — the
 * shell should keep running).
 */
void warn(const char *msg)
{
    perror(msg);
}

/* ── String utilities ─────────────────────────────────────────────────────── */

/*
 * trim_whitespace()
 * -----------------
 * Returns a pointer into str with leading whitespace skipped, and trailing
 * whitespace replaced with NUL bytes.
 *
 * This operates IN PLACE — no allocation.  The returned pointer is somewhere
 * inside the original buffer, so the caller must NOT free() it separately.
 *
 * Example
 *   "  ls -l  \0"  →  "ls -l\0"
 */
char *trim_whitespace(char *str)
{
    if (str == NULL) return NULL;

    /* Advance past leading whitespace. */
    while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n') {
        str++;
    }

    /* If the string is now empty, return it as-is. */
    if (*str == '\0') return str;

    /* Walk to the end and trim trailing whitespace backwards. */
    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' ||
                          *end == '\r' || *end == '\n')) {
        *end-- = '\0';
    }

    return str;
}
