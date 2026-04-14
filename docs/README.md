---
description: A programming language that reads like English.
---

# To

**To** is a programming language designed to be radically easy to learn while being powerful enough to build real projects. Built from scratch in C++ with zero dependencies.

```
to greet(name: string):
  print "Hey, {name}!"

greet("World")
```

## What makes To different?

- **Reads like English** — `to greet`, `through list as item`, `build Person`, `my.name`
- **Zero ceremony** — no semicolons, no braces, no type annotations required
- **Batteries included** — HTTP server, C FFI, regex, filesystem, async/await, all built in
- **Full toolchain** — interpreter, compiler, REPL, debugger, test runner, VS Code extension

## Quick install

```bash
git clone https://github.com/theomirzakhanian/to.git
cd to && mkdir build && cd build
cmake .. && cmake --build .
sudo cp build/to /usr/local/bin/to
```

```bash
to version   # To v0.2.0
```

## What can you build?

- **CLI tools and scripts** — file processing, automation, data manipulation
- **Web APIs** — built-in multi-threaded HTTP server with JSON support
- **System tools** — call any C library through FFI
- **Anything you'd use Python for** — but with cleaner syntax

## Start here

<table data-card-size="large" data-view="cards">
<thead><tr><th></th><th></th></tr></thead>
<tbody>
<tr><td><strong>Your First Program</strong></td><td>Write and run your first To program in 30 seconds</td></tr>
<tr><td><strong>Language Guide</strong></td><td>Variables, functions, classes, loops — the full walkthrough</td></tr>
<tr><td><strong>Standard Library</strong></td><td>math, time, fs, regex, web, json, ffi — what's built in</td></tr>
<tr><td><strong>Advanced Features</strong></td><td>Pipes, async/await, destructuring, shapes, enums</td></tr>
</tbody>
</table>
