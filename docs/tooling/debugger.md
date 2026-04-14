# Debugger

To has a built-in interactive debugger.

## Starting the Debugger

```bash
to debug myfile.to
```

The program pauses before each line. You control execution with commands.

## Commands

| Command | Short | Description |
|---------|-------|-------------|
| `next` | `n` | Execute the next line |
| `step` | `s` | Step into (same as next) |
| `continue` | `c` | Run until next breakpoint |
| `break <line>` | `b <line>` | Set a breakpoint |
| `print <var>` | `p <var>` | Inspect a variable's value and type |
| `stack` | `bt` | Show the call stack |
| `help` | `h` | Show available commands |
| `quit` | `q` | Exit the debugger |

## Example Session

```
$ to debug app.to
To Debugger — app.to
Type 'help' for commands. Press enter to step.

[debug] Paused at line 1
debug> n
[debug] Paused at line 2
debug> p name
name = "Theo" (string)
debug> b 10
Breakpoint set at line 10
debug> c
[debug] Paused at line 10
debug> stack
Stack trace (most recent call last):
  app.to:1 in <main>
  app.to:10 in process
debug> p result
result = 42 (int)
debug> c
```

## Tips

- Use `n` to step through line by line
- Use `b` to set breakpoints, then `c` to run until you hit one
- Use `p` to check variable values at any point
- Use `stack` / `bt` to see where you are in nested function calls
