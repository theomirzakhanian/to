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

## Two learning paths

These docs are organized so you can go from zero to expert:

### 🌱 Beginner — _Learn the basics_

Start with the **Learn To** section. Go top to bottom. Each page builds on the last. By the end, you'll be able to write real programs — CLI tools, classes, error handling, multi-file projects.

### 🚀 Advanced — _Master the language_

Once you're comfortable, the **Master To** section shows you the powerful features that separate good code from great code: lambdas, pipes, async, pattern matching, generators, and more.

## What makes To different?

- **Reads like English** — `to greet`, `through list as item`, `build Person`, `my.name`
- **Zero ceremony** — no semicolons, no braces, no required type annotations
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

- **CLI tools** — file processing, automation, data manipulation
- **Web APIs** — built-in multi-threaded HTTP server with JSON support
- **System tools** — call any C library through FFI
- **Anything you'd use Python for** — but with cleaner syntax

## Ready?

👉 Start with [Installation](getting-started/installation.md) and work your way through the Learn To section in order.
