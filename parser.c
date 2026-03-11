/* =============================================================================
 * parser.c — Input tokenization and Pipeline construction
 *
 * Responsibilities
 *   • Split a raw input string into Pipeline → Command → argv tokens.
 *   • Detect '|' separators (pipe) and '&' suffix (background).
 *   • Return a heap-allocated Pipeline the executor can consume directly.
 *
 * Parsing strategy (two-level split)
 * ------------------------------------
 *   Raw line    →  split on '|'   →  ["ls -l", "grep .c", "wc -l"]
 *   Each chunk  →  split on ' '  →  argv arrays  + detect '&'
 *
 *   Example:
 *     Input : "ls -l | grep .c &"
 *
 *     After pipe split:
 *       segment[0] = "ls -l "
 *       segment[1] = " grep .c &"
 *
 *     After token split:
 *       cmd[0].argv = {"ls", "-l", NULL},  pipe_out=1
 *       cmd[1].argv = {"grep", ".c", NULL}, background=1
 * ============================================================================= */

#include "shell.h"

/* ── Internal helpers ─────────────────────────────────────────────────────── */

/*
 * tokenize_command()
 * ------------------
 * Fills one Command struct from a single segment string (no '|' characters).
 *
 * strtok() splits 'segment' on whitespace delimiters.
 * It returns NULL when no more tokens remain.
 *
 * The '&' token is consumed here: it sets cmd->background and is NOT added
 * to argv so that execvp() never sees it.
 */
static void tokenize_command(char *segment, Command *cmd)
{
    cmd->argc       = 0;
    cmd->background = 0;
    cmd->pipe_out   = 0;

    /* strtok() modifies the string in place; we work on a copy (see caller). */
    char *token = strtok(segment, " \t");

    while (token != NULL && cmd->argc < MAX_ARGS - 1) {
        if (strcmp(token, "&") == 0) {
            /* Background operator: record the flag, do not add to argv. */
            cmd->background = 1;
        } else {
            /* Regular argument: duplicate so we own the memory. */
            cmd->argv[cmd->argc++] = strdup(token);
        }
        token = strtok(NULL, " \t");
    }

    /* execvp() requires the last element to be NULL. */
    cmd->argv[cmd->argc] = NULL;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

/*
 * parse_input()
 * -------------
 * Converts a raw input line into a heap-allocated Pipeline.
 *
 * Parameters
 *   line — trimmed, NUL-terminated input (caller retains ownership).
 *
 * Returns
 *   Pointer to a new Pipeline, or NULL on allocation failure.
 *   The caller must call free_pipeline() when done.
 *
 * Step-by-step
 *   1. Duplicate the line so strtok() can mutate it.
 *   2. Split on '|' to get pipe segments.
 *   3. Tokenize each segment into a Command.
 *   4. Mark all but the last command as pipe_out=1.
 *   5. Propagate background flag: if the last command has '&', every command
 *      in the pipeline runs in the background.
 */
Pipeline *parse_input(char *line)
{
    Pipeline *p = calloc(1, sizeof(Pipeline));
    if (p == NULL) {
        warn("parse_input: calloc failed");
        return NULL;
    }

    /* Work on a mutable copy so the caller's string is untouched. */
    char *line_copy = strdup(line);
    if (line_copy == NULL) {
        warn("parse_input: strdup failed");
        free(p);
        return NULL;
    }

    /* ── Stage 1: split on '|' ─────────────────────────────────────────── */
    /*
     * strtok() with delimiter "|" walks through line_copy, replacing each '|'
     * with '\0' and returning a pointer to the start of each segment.
     *
     * Note: strtok is NOT reentrant (it uses a static internal pointer).
     * For nested tokenization we therefore finish the pipe split first,
     * save all segment pointers, then call strtok again inside tokenize_command.
     */
    char *segments[MAX_PIPE_CMDS];
    int   seg_count = 0;

    char *seg = strtok(line_copy, "|");
    while (seg != NULL && seg_count < MAX_PIPE_CMDS) {
        segments[seg_count++] = seg;
        seg = strtok(NULL, "|");
    }

    if (seg_count == 0) {
        free(line_copy);
        free(p);
        return NULL;
    }

    /* ── Stage 2: tokenize each segment into a Command ─────────────────── */
    for (int i = 0; i < seg_count; i++) {
        char *trimmed = trim_whitespace(segments[i]);
        tokenize_command(trimmed, &p->cmds[i]);
    }
    p->count = seg_count;

    /* ── Stage 3: mark pipe_out flags ──────────────────────────────────── */
    /*
     * Every command except the last feeds its stdout into the next command's
     * stdin via a pipe.  The executor uses this flag to know which ends to
     * connect.
     */
    for (int i = 0; i < p->count - 1; i++) {
        p->cmds[i].pipe_out = 1;
    }

    /* ── Stage 4: propagate background flag ────────────────────────────── */
    /*
     * "ls | grep .c &" — the '&' is written after the last command.
     * If the last command is background, the whole pipeline is background.
     */
    if (p->cmds[p->count - 1].background) {
        for (int i = 0; i < p->count - 1; i++) {
            p->cmds[i].background = 1;
        }
    }

    free(line_copy);
    return p;
}

/*
 * free_pipeline()
 * ---------------
 * Releases all heap memory owned by a Pipeline: the strdup'd argv strings
 * and the Pipeline struct itself.
 */
void free_pipeline(Pipeline *p)
{
    if (p == NULL) return;

    for (int i = 0; i < p->count; i++) {
        for (int j = 0; j < p->cmds[i].argc; j++) {
            free(p->cmds[i].argv[j]);
        }
    }
    free(p);
}
