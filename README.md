# BSDSF24A013-OS-A03 — Operating Systems PA-03

A basic command-line shell written in C, based on the supplied starter source files.

## Project structure

- `src/main.c` — shell main loop
- `src/shell.c` — command input and tokenization
- `src/execute.c` — process creation and command execution
- `include/shell.h` — shared definitions and function prototypes
- `Makefile` — build instructions

## Build and run

```bash
make
./bin/myshell
```

Test the base shell with `ls -l`, `pwd`, and `whoami`.

## Clean

```bash
make clean
```

This is the Feature 1 base-shell stage. Subsequent assignment features will be developed in separate feature branches and merged in the order specified by the assignment.
