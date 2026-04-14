# CLI Reference

## Commands

| Command | Description |
|---------|-------------|
| `to` | Start interactive REPL |
| `to run <file>` | Interpret and run a `.to` file |
| `to build <file>` | Compile to a native binary |
| `to build <file> -o <name>` | Compile with a custom output name |
| `to check <file>` | Check for syntax errors without running |
| `to test <file>` | Run all `test_` functions in the file |
| `to debug <file>` | Run with the interactive debugger |
| `to version` | Print version number |
| `to help` | Show help |

## Examples

```bash
# Run a script
to run server.to

# Compile to binary
to build app.to -o myapp
./myapp

# Check for errors
to check main.to

# Run tests
to test tests.to

# Debug step by step
to debug buggy.to
```

## REPL

Launch with no arguments:

```bash
$ to
To v0.2.0 — Interactive REPL
Type 'exit' or press Ctrl+C to quit.

to>
```

The REPL remembers variables and functions across lines. Expressions are automatically printed. Multi-line blocks (ending with `:`) wait for you to finish with a blank line.
