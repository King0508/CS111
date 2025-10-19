# Cash Shell - California Shell

## Compile

```bash
make clean
make
```

## Test

```bash
./cash
```

## Features Implemented

- Built-in commands: `exit`, `cd`, `pwd`, `help`, `wait`
- Process spawning with `fork()`, `execve()`, `waitpid()`
- PATH resolution
- I/O redirection (`<` and `>`)
- Background jobs (`&`)
- Signal handling for interactive mode

## Submit

```bash
zip vitamin2.zip *.c *.h
```

Upload to Gradescope.

## Key Fixes (for autograder)

- Improved memory management in `spawn_process()`
- Added NULL checks in `resolve_path()`
- Fixed memory initialization in `parse_redirections()`
- Proper cleanup of dynamically allocated memory
