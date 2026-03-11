/* =============================================================================
 * main.c — Entry point for Mini-Bash
 *
 * Responsibilities
 *   • Parse any future command-line flags (currently none).
 *   • Initialise the shell environment.
 *   • Hand control to the REPL loop.
 *
 * Design note
 *   Keeping main() thin makes it easy to add startup flags (e.g. -c "cmd" for
 *   non-interactive use) without touching the core REPL logic.
 * ============================================================================= */

#include "shell.h"

int main(void)
{
    /* Initialise signal handlers, print the welcome banner, etc. */
    shell_init();

    /* Enter the Read-Eval-Print Loop — only returns on "exit". */
    shell_loop();

    return EXIT_SUCCESS;
}
