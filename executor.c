/* =============================================================================
 * executor.c — Process execution engine (fork / execvp / pipe / waitpid)
 *
 * Responsibilities
 *   • Execute a fully parsed Pipeline.
 *   • Handle single commands (fork + execvp) and pipelines (pipe + fork + dup2).
 *   • Support background execution (no waitpid for parent).
 *   • Reap zombie processes.
 *
 * Key system calls explained
 * --------------------------
 *
 *  fork()
 *  ------
 *  pid_t fork(void);
 *  Creates an exact copy of the calling process.  Both parent and child
 *  continue from the instruction after fork().  Return value distinguishes them:
 *    > 0  → caller is the PARENT; value is the child's PID
 *    = 0  → caller is the CHILD
 *    < 0  → error; no child was created
 *
 *  execvp()
 *  --------
 *  int execvp(const char *file, char *const argv[]);
 *  Replaces the calling process image with a new program.  The 'p' suffix
 *  means it searches $PATH automatically.  On success it NEVER RETURNS —
 *  the child's code is completely replaced.  On failure it returns -1 and
 *  sets errno.
 *
 *  pipe()
 *  ------
 *  int pipe(int pipefd[2]);
 *  Creates a unidirectional byte-stream kernel buffer.
 *    pipefd[PIPE_READ]  (0) — read end
 *    pipefd[PIPE_WRITE] (1) — write end
 *  Data written to [1] is read from [0].  Closing the write end sends EOF
 *  to any reader; closing the read end sends SIGPIPE to any writer.
 *
 *  dup2()
 *  ------
 *  int dup2(int oldfd, int newfd);
 *  Makes newfd a copy of oldfd (closing newfd first if open).  We use it to
 *  redirect file descriptors:
 *    dup2(pipe[PIPE_WRITE], STDOUT_FILENO)  — redirect stdout into pipe
 *    dup2(pipe[PIPE_READ],  STDIN_FILENO)   — redirect stdin from pipe
 *
 *  waitpid()
 *  ---------
 *  pid_t waitpid(pid_t pid, int *wstatus, int options);
 *  Waits for state changes in a child process.
 *    pid = -1      → wait for any child (like wait())
 *    pid = <pid>   → wait for that specific child
 *    options = 0   → block until child terminates (foreground)
 *    options = WNOHANG → return immediately if no child has changed state
 *  Prevents zombie processes by collecting the exit status.
 * ============================================================================= */

#include "shell.h"

/* ── Internal: single external command ────────────────────────────────────── */

/*
 * exec_single()
 * -------------
 * Forks and execs a single Command with no pipe involvement.
 *
 * Parent behaviour
 *   Foreground: waitpid() blocks until the child exits → no zombies.
 *   Background: parent returns immediately; child becomes an orphan that the
 *               init process will eventually reap.  We print the PID so the
 *               user knows how to track it.
 */
static void exec_single(Command *cmd)
{
    pid_t pid = fork();   /* ← System call: duplicate the process            */

    if (pid < 0) {
        /* fork() failed — kernel could not allocate a process table entry.  */
        warn("fork");
        return;
    }

    if (pid == 0) {
        /* ── CHILD ────────────────────────────────────────────────────── */

        /* Restore default SIGINT handling so Ctrl-C kills the child,
         * not the shell (the shell set SIG_IGN in shell_init). */
        signal(SIGINT, SIG_DFL);

        /*
         * execvp() — replace this child image with the requested binary.
         * It searches $PATH (the 'p' suffix), so "ls" finds /bin/ls, etc.
         * argv[0]        = command name (also used in error messages)
         * cmd->argv      = full argument vector, NULL-terminated
         * If execvp returns, it failed.
         */
        execvp(cmd->argv[0], cmd->argv);

        /* execvp only reaches here on error. */
        fprintf(stderr, "mini-bash: %s: %s\n", cmd->argv[0], strerror(errno));
        exit(EXIT_FAILURE);   /* exit the child, NOT the shell               */
    }

    /* ── PARENT ────────────────────────────────────────────────────────── */

    if (cmd->background) {
        /* Background job: do not block. Print PID and return to prompt.    */
        printf("[background] PID %d\n", pid);
    } else {
        /* Foreground job: block until this specific child terminates.
         *
         * waitpid(pid, &status, 0)
         *   pid  → wait for exactly this child (not any random child)
         *   0    → no flags; block until child changes state
         *
         * This prevents zombie processes: a process that has exited but
         * whose exit status has not been collected remains in the process
         * table as a "zombie" until the parent calls wait/waitpid.         */
        int status;
        waitpid(pid, &status, 0);

        /* Optionally surface non-zero exit codes for debugging. */
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            /* Non-zero exit: command reported failure (like grep found nothing). */
        }
    }
}

/* ── Internal: pipeline of N commands ─────────────────────────────────────── */

/*
 * exec_pipeline()
 * ---------------
 * Executes a sequence of commands connected by pipes.
 *
 *   cmd[0] stdout  ──pipe[0]──►  cmd[1] stdin
 *   cmd[1] stdout  ──pipe[1]──►  cmd[2] stdin
 *   ...
 *
 * Algorithm
 * ---------
 * For N commands we need N-1 pipes.  We allocate them all up front into a
 * two-dimensional array pipes[N-1][2], then for each command:
 *
 *   • If it is NOT the first command: dup2 the READ end of the previous
 *     pipe onto stdin.
 *   • If it is NOT the last command: dup2 the WRITE end of the current
 *     pipe onto stdout.
 *   • Close ALL pipe fds in the child — the ones we dup2'd are now on
 *     stdin/stdout; the rest would keep pipes open unnecessarily.
 *
 * Closing pipe ends is critical
 * ------------------------------
 * After dup2, the original fd must be closed in every process (parent AND
 * child) or the reader will never see EOF.  A pipe's READ end blocks until
 * ALL write ends are closed.  If the parent forgets to close its copy of
 * pipefd[WRITE], "grep" will block forever waiting for more input.
 */
static void exec_pipeline(Pipeline *p)
{
    int   n     = p->count;
    int   pipes[MAX_PIPE_CMDS - 1][2];  /* one pipe between each pair        */
    pid_t pids[MAX_PIPE_CMDS];          /* track all child PIDs               */

    /* ── Create all N-1 pipes up front ─────────────────────────────────── */
    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            warn("pipe");
            return;
        }
    }

    /* ── Fork one child per command ─────────────────────────────────────── */
    for (int i = 0; i < n; i++) {
        pids[i] = fork();

        if (pids[i] < 0) {
            warn("fork (pipeline)");
            return;
        }

        if (pids[i] == 0) {
            /* ── CHILD i ─────────────────────────────────────────────── */

            signal(SIGINT, SIG_DFL);

            /* Connect stdin to the previous pipe's read end (not first cmd). */
            if (i > 0) {
                /*
                 * dup2(pipes[i-1][PIPE_READ], STDIN_FILENO)
                 * Makes fd 0 (stdin) point to the read end of the previous
                 * pipe.  Anything the previous command writes to its stdout
                 * will appear on our stdin.
                 */
                if (dup2(pipes[i-1][PIPE_READ], STDIN_FILENO) < 0) {
                    warn("dup2 stdin");
                    exit(EXIT_FAILURE);
                }
            }

            /* Connect stdout to the current pipe's write end (not last cmd). */
            if (i < n - 1) {
                /*
                 * dup2(pipes[i][PIPE_WRITE], STDOUT_FILENO)
                 * Makes fd 1 (stdout) point to the write end of the current
                 * pipe.  Anything we printf/write goes into the pipe buffer.
                 */
                if (dup2(pipes[i][PIPE_WRITE], STDOUT_FILENO) < 0) {
                    warn("dup2 stdout");
                    exit(EXIT_FAILURE);
                }
            }

            /* Close ALL raw pipe fds in the child.
             * After dup2, stdin/stdout ARE the pipe ends we need.
             * Leaving these open would prevent EOF propagation.             */
            for (int j = 0; j < n - 1; j++) {
                close(pipes[j][PIPE_READ]);
                close(pipes[j][PIPE_WRITE]);
            }

            /* Check if this is a built-in (edge case: built-ins in pipelines). */
            if (is_builtin(p->cmds[i].argv[0])) {
                int ret = run_builtin(&p->cmds[i]);
                exit(ret);
            }

            /* Execute the external command. */
            execvp(p->cmds[i].argv[0], p->cmds[i].argv);
            fprintf(stderr, "mini-bash: %s: %s\n",
                    p->cmds[i].argv[0], strerror(errno));
            exit(EXIT_FAILURE);
        }
        /* ── PARENT: close pipe ends as children are spawned ─────────── */
        /*
         * Each time we fork a child, we close the pipe ends that the
         * PARENT no longer needs.  This is essential: if the parent keeps
         * the write end of a pipe open, the reader (next child) will never
         * see EOF and will block forever.
         */
        if (i > 0) {
            close(pipes[i-1][PIPE_READ]);
        }
        if (i < n - 1) {
            close(pipes[i][PIPE_WRITE]);
        }
    }

    /* Close any remaining pipe ends in the parent. */
    for (int i = 0; i < n - 1; i++) {
        /* Defensively close both ends; already-closed fds produce EBADF
         * which we intentionally ignore here. */
        close(pipes[i][PIPE_READ]);
        close(pipes[i][PIPE_WRITE]);
    }

    /* ── Wait for all children ──────────────────────────────────────────── */
    int background = p->cmds[n-1].background;

    for (int i = 0; i < n; i++) {
        if (!background) {
            /*
             * waitpid() with pid=pids[i] waits for that specific child.
             * options=0 means "block until child exits or is killed."
             * Collecting the status prevents zombie processes.
             */
            int status;
            waitpid(pids[i], &status, 0);
        } else {
            printf("[background] PID %d\n", pids[i]);
        }
    }
}

/* ── Public API ───────────────────────────────────────────────────────────── */

/*
 * execute_pipeline()
 * ------------------
 * Main dispatch called by shell_loop() for every parsed Pipeline.
 *
 * Decision tree
 *   Single command AND it is a built-in  → run_builtin() (no fork)
 *   Single command, external             → exec_single()
 *   Multiple commands                    → exec_pipeline()
 */
void execute_pipeline(Pipeline *p)
{
    if (p == NULL || p->count == 0) return;

    /* Guard: first command must have at least one token. */
    if (p->cmds[0].argc == 0 || p->cmds[0].argv[0] == NULL) return;

    if (p->count == 1) {
        /* Single command path. */
        Command *cmd = &p->cmds[0];

        if (is_builtin(cmd->argv[0])) {
            /* Built-ins run directly in the shell process — no fork needed. */
            run_builtin(cmd);
        } else {
            exec_single(cmd);
        }
    } else {
        /* Multi-command pipeline. */
        exec_pipeline(p);
    }
}
