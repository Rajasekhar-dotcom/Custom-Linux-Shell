/* =============================================================================
 * shell.h — Central header for Mini-Bash
 *
 * Every module includes this header to share types, constants, and function
 * prototypes. Keeping declarations here prevents circular dependencies and
 * makes the interface between modules explicit.
 * ============================================================================= */

#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>      /* fork(), execvp(), chdir(), pipe(), dup2()       */
#include <sys/wait.h>    /* waitpid(), WIFEXITED, WEXITSTATUS               */
#include <sys/types.h>   /* pid_t                                            */
#include <errno.h>       /* errno, strerror()                                */
#include <limits.h>      /* PATH_MAX                                         */

/* ── Tuneable constants ───────────────────────────────────────────────────── */

#define PROMPT          "mini-bash$ "
#define MAX_ARGS        128       /* Maximum tokens in a single command        */
#define MINIBASH_MAX_INPUT  4096  /* Maximum bytes of raw input per line       */

/* Pipe fd index aliases — improves readability over bare 0/1 */
#define PIPE_READ       0
#define PIPE_WRITE      1

/* ── Command representation ───────────────────────────────────────────────── */

/*
 * Command
 * -------
 * Holds a fully parsed, ready-to-execute command.  The parser fills one of
 * these from the raw input string; the executor consumes it.
 *
 * Fields
 *   argv       — NULL-terminated argument vector, compatible with execvp().
 *   argc       — Number of arguments (argv[argc] == NULL).
 *   background — Non-zero when the user appended '&'; the parent will not
 *                wait for the child to finish.
 *   pipe_out   — Non-zero when this command feeds into the next via '|'.
 */
typedef struct {
    char  *argv[MAX_ARGS];   /* argument vector (execvp-ready)                */
    int    argc;             /* argument count                                 */
    int    background;       /* 1 = run in background, 0 = foreground          */
    int    pipe_out;         /* 1 = stdout should go into a pipe               */
} Command;

/*
 * Pipeline
 * --------
 * Represents a sequence of commands joined by '|'.
 *
 *   ls -l | grep .c | wc -l
 *
 * would produce a Pipeline with count=3 and three populated Command entries.
 */
#define MAX_PIPE_CMDS   16

typedef struct {
    Command cmds[MAX_PIPE_CMDS];  /* ordered list of commands in the pipeline  */
    int     count;                /* number of commands                        */
} Pipeline;

/* ── shell.c ──────────────────────────────────────────────────────────────── */
void shell_init(void);
void shell_loop(void);

/* ── parser.c ─────────────────────────────────────────────────────────────── */
Pipeline *parse_input(char *line);
void      free_pipeline(Pipeline *p);

/* ── executor.c ───────────────────────────────────────────────────────────── */
void execute_pipeline(Pipeline *p);

/* ── builtins.c ───────────────────────────────────────────────────────────── */
int  is_builtin(const char *cmd);
int  run_builtin(Command *cmd);

/* ── utils.c ──────────────────────────────────────────────────────────────── */
void  die(const char *msg);              /* print error + exit                 */
void  warn(const char *msg);             /* print error, continue              */
char *trim_whitespace(char *str);        /* strip leading/trailing spaces      */

#endif /* SHELL_H */
