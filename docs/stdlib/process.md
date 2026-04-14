# process

The `process` module provides functions for running shell commands, reading
environment variables, and controlling the current process.

```to
use process
```

## process.run(cmd, args)

Runs a command with arguments and returns a record with `stdout`, `stderr`, and
`exit_code`.

```to
let result = process.run("git", ["status", "--short"])

print(result.stdout)
print(result.exit_code)  // 0 on success

if result.exit_code != 0
    print("Error: {result.stderr}")
end
```

## process.exec(cmd)

Runs a command string and returns its stdout directly. Raises an error if the
command exits with a non-zero code.

```to
let branch = process.exec("git rev-parse --abbrev-ref HEAD")
print("Current branch: {branch}")
```

## process.env(name)

Returns the value of an environment variable, or `nil` if it is not set.

```to
let home = process.env("HOME")
let port = process.env("PORT") ?? "8080"

print("Home directory: {home}")
print("Listening on port: {port}")
```

## process.exit(code)

Terminates the program immediately with the given exit code.

```to
if missing_config
    print("Error: config file not found")
    process.exit(1)
end
```

## process.pid()

Returns the process ID of the current program.

```to
print("Running as PID {process.pid()}")
```

## Example: Build Script

```to
use process

fn sh(cmd)
    let result = process.run("sh", ["-c", cmd])
    if result.exit_code != 0
        print("FAILED: {cmd}")
        print(result.stderr)
        process.exit(1)
    end
    return result.stdout
end

sh("mkdir -p build")
sh("cp -r src/* build/")
print("Build complete.")
```
