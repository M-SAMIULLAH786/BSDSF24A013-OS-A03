# BSDSF24A013-OS-A03 — Operating Systems PA-03

A basic command-line shell written in C for the Operating Systems Programming Assignment 03.

## Project structure

- `src/main.c` — shell main loop
- `src/shell.c` — command input and tokenization
- `src/execute.c` — built-ins, process creation, and external command execution
- `include/shell.h` — shared definitions and function prototypes
- `Makefile` — build instructions

## Build and run

```bash
make
./bin/myshell
```

## Feature 1 — Base shell

The base shell reads a command, tokenizes it, and runs external programs using `fork()`, `execvp()`, and `waitpid()`.

## Feature 2 — Built-in commands

The following built-ins run directly inside the shell process (before `fork()`):

- `exit` — exit the shell.
- `cd <directory>` — change the shell's current working directory. With no directory, it attempts to use `HOME`.
- `help` — display the built-in command list.
- `jobs` — currently displays a placeholder message; job tracking is added in a later assignment feature.

Try `pwd`, `cd ..`, `pwd`, `help`, `jobs`, and `exit`.

## Clean

```bash
make clean
```

Later assignment features are developed in separate feature branches and merged in the order specified by the instructor.

## Feature 3 — Command history

- The shell remembers the most recent 20 commands.
- Type `history` to display saved commands with their numbers.
- Type `!n` (for example, `!2`) to run a command again by its displayed number.
- When the history is full, the oldest command is removed.

The `!n` shortcut is expanded before tokenization, and the recalled command is saved as the command that was executed.
