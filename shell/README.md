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
