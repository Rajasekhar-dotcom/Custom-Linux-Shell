# Custom Linux Shell (Mini-Bash)

## Overview
This project is a custom command-line interface designed to emulate the core functionality of Unix shells like `bash` or `zsh`. Written entirely in C, it bypasses high-level standard library abstractions (like `system()`) to interface directly with the Linux kernel via POSIX system calls.

This shell demonstrates a fundamental understanding of Operating System mechanics, specifically focusing on memory space duplication, process state management, and file descriptor manipulation. 

## Core System Calls & Architecture



* **Process Management (`fork`, `execvp`, `waitpid`):** The shell parses user input and uses `fork()` to clone the current process. The child process uses `execvp()` to load the requested binary into memory, while the parent uses `waitpid()` to prevent the creation of zombie processes.
* **Inter-Process Communication (`pipe`, `dup2`):** Supports command piping (e.g., `ls -l | grep txt`). The shell creates a unidirectional data channel using `pipe()`, and uses `dup2()` to redirect the standard output (`stdout`) of the first process to the standard input (`stdin`) of the second.
* **Background Execution (`&`):** Supports detached child processes running concurrently. If a command ends with `&`, the parent process bypasses the `waitpid()` call, returning control to the user immediately.
* **Built-in Commands:** Commands that modify the shell's own environment (like `cd` or `exit`) are handled natively within the parent process using system calls like `chdir()`.

## Installation & Usage
*Note: This project must be compiled and run in a Linux/Unix environment.*

# Clone the repository
git clone [https://github.com/yourusername/custom-linux-shell.git](https://github.com/yourusername/custom-linux-shell.git)
cd custom-linux-shell

# Compile the shell
gcc -Wall -Wextra src/shell.c src/parser.c -o myshell

# Launch the shell
./myshell

