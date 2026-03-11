/* =============================================================================
 * builtins.c — Shell built-in command implementations
 *
 * Why are some commands "built-in"?
 * ----------------------------------
 * Commands that must modify the shell's own process state CANNOT be run in a
 * child process, because a child's changes to its own environment (current
 * working directory, environment variables, etc.) do not propagate back to the
 * parent.
 *
 *   cd /tmp       → calls chdir() — must change the SHELL's cwd, not a child's
 *   exit          → must terminate the SHELL process itself
 *
 * Built-ins run inside the shell process (no fork).
 *
 * Supported built-ins
 *   cd [dir]   — change directory (defaults to $HOME when dir omitted)
 *   exit       — terminate the shell
 *   help       — list available built-ins
 * ============================================================================= */

#include "shell.h"

/* ── is_builtin ───────────────────────────────────────────────────────────── */

/*
 * is_builtin()
 * ------------
 * Returns 1 if 'cmd' is a known built-in name, 0 otherwise.
 * The executor calls this before attempting fork/exec.
 */
int is_builtin(const char *cmd)
{
    if (cmd == NULL) return 0;

    return (strcmp(cmd, "cd")   == 0 ||
            strcmp(cmd, "exit") == 0 ||
            strcmp(cmd, "help") == 0);
}

/* ── Individual built-in handlers ─────────────────────────────────────────── */

/*
 * builtin_cd()
 * ------------
 * Changes the shell's current working directory.
 *
 * chdir() system call
 * -------------------
 *   int chdir(const char *path);
 *   • Returns 0 on success, -1 on failure (errno is set).
 *   • Changes the CWD of the calling process.
 *   • Must run in the shell (parent) process — a child's chdir() would not
 *     affect the shell's own CWD.
 *
 * Behaviour
 *   cd          → navigate to $HOME (mirrors bash behaviour)
 *   cd <path>   → navigate to <path>
 *   cd -        → navigate to $OLDPWD (previous directory)
 */
static int builtin_cd(Command *cmd)
{
    const char *target;

    if (cmd->argc < 2) {
        /* No argument supplied — default to $HOME. */
        target = getenv("HOME");
        if (target == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
    } else if (strcmp(cmd->argv[1], "-") == 0) {
        /* "cd -" swaps to the previous directory stored in $OLDPWD. */
        target = getenv("OLDPWD");
        if (target == NULL) {
            fprintf(stderr, "cd: OLDPWD not set\n");
            return 1;
        }
        printf("%s\n", target);   /* bash prints the target when using cd - */
    } else {
        target = cmd->argv[1];
    }

    /* Save current directory as OLDPWD before changing. */
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        setenv("OLDPWD", cwd, 1);  /* 1 = overwrite existing value */
    }

    /*
     * chdir() — the actual directory change.
     * On failure errno tells us why (ENOENT, EACCES, ENOTDIR, …).
     */
    if (chdir(target) != 0) {
        fprintf(stderr, "cd: %s: %s\n", target, strerror(errno));
        return 1;
    }

    /* Update $PWD so sub-processes and the prompt see the new directory. */
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        setenv("PWD", cwd, 1);
    }

    return 0;
}

/*
 * builtin_exit()
 * --------------
 * Terminates the shell.
 *
 * An optional numeric argument sets the exit status:
 *   exit      → exit with status 0
 *   exit 1    → exit with status 1
 *   exit 42   → exit with status 42
 */
static int builtin_exit(Command *cmd)
{
    int status = EXIT_SUCCESS;

    if (cmd->argc >= 2) {
        status = atoi(cmd->argv[1]);
    }

    printf("Goodbye!\n");
    exit(status);
    /* Unreachable, but satisfies the compiler. */
    return 0;
}

/*
 * builtin_help()
 * --------------
 * Prints a summary of all available built-in commands.
 */
static int builtin_help(Command *cmd)
{
    (void)cmd;   /* unused parameter */

    printf("\n");
    printf("  Mini-Bash Built-in Commands\n");
    printf("  ───────────────────────────────────────────────\n");
    printf("  cd [dir]          Change directory (default: $HOME)\n");
    printf("  cd -              Switch to previous directory\n");
    printf("  exit [status]     Exit the shell (default status: 0)\n");
    printf("  help              Show this help message\n");
    printf("\n");
    printf("  External commands are executed via fork() + execvp().\n");
    printf("  Append '&' to run a command in the background.\n");
    printf("  Use '|' to pipe commands together.\n");
    printf("\n");
    return 0;
}

/* ── run_builtin ──────────────────────────────────────────────────────────── */

/*
 * run_builtin()
 * -------------
 * Dispatches to the correct built-in handler.
 *
 * Returns
 *   0 on success, non-zero on failure (mirrors Unix exit-code convention).
 *
 * Precondition: is_builtin(cmd->argv[0]) returned non-zero.
 */
int run_builtin(Command *cmd)
{
    if (cmd->argc == 0 || cmd->argv[0] == NULL) return 1;

    const char *name = cmd->argv[0];

    if (strcmp(name, "cd")   == 0) return builtin_cd(cmd);
    if (strcmp(name, "exit") == 0) return builtin_exit(cmd);
    if (strcmp(name, "help") == 0) return builtin_help(cmd);

    /* Should never be reached if is_builtin() is consistent. */
    fprintf(stderr, "builtins: unknown command '%s'\n", name);
    return 1;
}
