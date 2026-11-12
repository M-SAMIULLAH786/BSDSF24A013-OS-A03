# BSDSF24A013-OS-A03 — Operating Systems PA-03

A basic command-line shell written in C for the Operating Systems Programming Assignment 03.

## Project structure

- `src/main.c` — shell main loop
- `src/shell.c` — command input, Readline completion, and tokenization
- `src/execute.c` — built-ins, process creation, redirection, pipes, and jobs
- `include/shell.h` — shared definitions and function prototypes
- `Makefile` — build instructions

## Build and run

```bash
make
./bin/myshell
```

GNU Readline development files are required (Debian/Ubuntu: `sudo apt install libreadline-dev`).

## Feature 1 — Base shell

The base shell reads a command, tokenizes it, and runs external programs using `fork()`, `execvp()`, and `waitpid()`.

## Feature 2 — Built-in commands

- `exit` — exit the shell.
- `cd <directory>` — change the shell's current working directory; uses `HOME` when no directory is given.
- `help` — display help.
- `jobs` — list active background jobs (implemented in Feature 6).

## Feature 3 — Command history

- Remembers the most recent 20 commands.
- `history` displays saved commands with their numbers.
- `!n` runs a command again by its displayed number.
- The oldest command is removed when the history is full.

## Feature 4 — GNU Readline

- GNU Readline provides interactive input.
- Up and Down arrow keys navigate command history.
- Tab completes command names from built-ins and executable programs in `PATH`.
- Tab completes file and directory names after a space.
- The build links with `-lreadline`.

## Feature 5 — Input/output redirection and pipes

The tokenizer recognizes `<`, `>`, and `|` even without spaces around them.

```bash
wc -l < input.txt
ls -l > listing.txt
ls -l | wc -l
```

The shell uses `open()`, `dup2()`, `close()`, and `pipe()`. One pipe connecting two commands is supported; redirection can be used on either side.

## Feature 6 — Command chaining and background processes

- `; ` runs commands sequentially. The next command starts after the previous foreground command finishes.
- `&` starts a command in the background so the prompt returns immediately.
- Background process IDs are stored in a job list.
- `jobs` displays the PIDs that are still active.
- The shell reaps completed children without blocking by repeatedly calling `waitpid(-1, &status, WNOHANG)` before the next prompt and when listing jobs.
- A background pipeline tracks both child PIDs.

Examples:

```bash
pwd ; ls -l ; whoami
sleep 30 &
jobs
ls -l | wc -l &
jobs
```

## Clean

```bash
make clean
```

Each assignment feature is developed on its own branch and merged into `main` in the instructor's order. Full build and runtime testing will be completed after all features have been implemented.

## Feature 7 — Conditional statements

The shell supports multiline `if`, `then`, optional `else`, and `fi` blocks. It runs the condition as a child process, waits for it, and checks the result with `WIFEXITED()` and `WEXITSTATUS()`. A zero exit status runs the `then` block; a non-zero or unsuccessful condition runs the `else` block.

Example:

```bash
if test -f README.md
then
  echo "README exists"
else
  echo "README not found"
fi
```
