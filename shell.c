/* =============================================================================
 * shell.c — REPL loop and shell lifecycle management
 *
 * Responsibilities
 *   • Display the prompt.
 *   • Read a line of input with getline().
 *   • Drive parse → execute for every line.
 *   • Handle EOF (Ctrl-D) gracefully.
 *
 * The REPL (Read-Eval-Print Loop) is the heartbeat of every shell:
 *
 *   ┌─────────────────────────────────────────────────┐
 *   │  1. Print prompt                                │
 *   │  2. Read line (getline)                         │
 *   │  3. Parse line  → Pipeline                      │
 *   │  4. Execute Pipeline (builtins or fork/exec)    │
 *   │  5. Free resources                              │
 *   │  6. Go to 1                                     │
 *   └─────────────────────────────────────────────────┘
 * ============================================================================= */

#include "shell.h"

/* ── shell_init ───────────────────────────────────────────────────────────── */

/*
 * shell_init()
 * ------------
 * Called once at startup.  Sets up anything that must exist before the first
 * prompt appears:
 *   • Ignores SIGINT on the parent so Ctrl-C only kills the foreground child
 *     (full job-control is beyond Mini-Bash's scope, but this one line already
 *     prevents the shell itself from dying on Ctrl-C).
 */
void shell_init(void)
{
    /* SIG_IGN makes the shell itself immune to Ctrl-C.
     * Child processes inherit the default handler after fork(). */
    signal(SIGINT, SIG_IGN);

    printf("Mini-Bash v1.0  |  type 'exit' to quit\n");
}

/* ── shell_loop ───────────────────────────────────────────────────────────── */

/*
 * shell_loop()
 * ------------
 * The infinite REPL.  Only exits when the user types "exit" (handled inside
 * run_builtin()) or when getline() signals EOF.
 *
 * getline() API recap
 * -------------------
 *   ssize_t getline(char **lineptr, size_t *n, FILE *stream);
 *
 *   • Dynamically allocates (or grows) the buffer pointed to by *lineptr.
 *   • Updates *n with the current buffer capacity.
 *   • Returns the number of characters read (including '\n'), or -1 on EOF
 *     or error.
 *   • The caller is responsible for free()ing *lineptr.
 *
 * Why getline() instead of fgets()?
 *   fgets() requires a fixed buffer size, silently truncating longer lines.
 *   getline() handles arbitrarily long input, allocating memory as needed.
 */
void shell_loop(void)
{
    char   *line = NULL;   /* getline manages this buffer                     */
    size_t  cap  = 0;      /* current buffer capacity (managed by getline)    */
    ssize_t len;

    while (1) {
        /* 1. Print prompt to stdout, flush so it appears before blocking. */
        printf("%s", PROMPT);
        fflush(stdout);

        /* 2. Read one line of input.
         *    getline blocks until '\n' or EOF.
         *    It reallocates 'line' as needed, updating 'cap'.             */
        len = getline(&line, &cap, stdin);

        if (len == -1) {
            /* EOF reached (user pressed Ctrl-D) — exit cleanly. */
            printf("\n");
            break;
        }

        /* Strip the trailing newline getline leaves in the buffer. */
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        /* Skip blank lines. */
        char *trimmed = trim_whitespace(line);
        if (trimmed[0] == '\0') {
            continue;
        }

        /* 3. Parse the raw line into a Pipeline struct. */
        Pipeline *pipeline = parse_input(trimmed);
        if (pipeline == NULL || pipeline->count == 0) {
            free_pipeline(pipeline);
            continue;
        }

        /* 4. Execute: builtins run in-process, external cmds via fork/exec. */
        execute_pipeline(pipeline);

        /* 5. Release the Pipeline (argv pointers are strdup'd in parser). */
        free_pipeline(pipeline);
    }

    /* Release getline's buffer before exiting. */
    free(line);
}
