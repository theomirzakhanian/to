<p align="center">
  <h1 align="center">To</h1>
  <p align="center">A programming language that reads like English.</p>
  <p align="center">
    <a href="#install">Install</a> &middot;
    <a href="#quick-start">Quick Start</a> &middot;
    <a href="docs/README.md">Documentation</a> &middot;
    <a href="#examples">Examples</a>
  </p>
</p>

---

```
to greet(name: string):
  print "Hey, {name}!"

greet("World")
```

**To** is a programming language designed to be radically easy to learn while being powerful enough to build real projects. It compiles and interprets `.to` files, ships with a built-in HTTP server, C FFI, regex, filesystem access, async/await, a debugger, and a test runner.

Built from scratch in C++. No dependencies.

---

## Install

```bash
git clone https://github.com/theomirzakhanian/to.git
cd to
mkdir build && cd build
cmake .. && cmake --build .
```

Add to your PATH:

```bash
sudo cp build/to /usr/local/bin/to
```

## Quick Start

Create `hello.to`:

```
print "Hello, World!"

name = input("What's your name? ")
print "Nice to meet you, {name}!"
```

Run it:

```bash
to run hello.to
```

## The Basics

### Variables

```
age = 18          ~ mutable
const PI = 3.14   ~ immutable

x = 5
x = "hello"       ~ dynamic typing — totally fine
```

### Functions

```
to add(x: int, y: int) -> int:
  return x + y

to greet(name):
  print "Hey, {name}!"
```

### Control Flow

```
if age >= 21:
  print "Welcome"
or age >= 18:
  print "Almost"
else:
  print "Not yet"
```

### Pattern Matching

```
given day:
  if "Monday":
    print "Pain"
  if "Friday":
    print "Freedom"
  else:
    print "Meh"
```

### Loops

```
through 1..10 as i:
  print i

through names as name:
  print "Hello, {name}!"

while x < 100:
  x = x + 1
```

### Classes & Inheritance

```
build Animal:
  to init(name, sound):
    my.name = name
    my.sound = sound
  to speak():
    print "{my.name} says {my.sound}!"

build Dog from Animal:
  to init(name):
    my.name = name
    my.sound = "Woof"
    my.tricks = []
  to learn(trick):
    my.tricks.add(trick)

rex = Dog("Rex")
rex.speak()       ~ Rex says Woof!
rex.learn("sit")
```

## Advanced Features

### Pipe Operator

Chain function calls with `then` — data flows left to right:

```
result = [1, 2, 3, 4, 5]
  then filter((x): x > 2)
  then map((x): x * x)
  then sum

print result   ~ 50
```

### Destructuring

```
[first, second, ...rest] = [1, 2, 3, 4, 5]
{name, age} = {name = "Theo", age = 18}
```

### Lambdas

```
double = (x): x * 2
squared = map(numbers, (x): x * x)
evens = filter(numbers, (x): x % 2 == 0)
```

### Async / Await

```
task1 = async fetch_data("users")
task2 = async fetch_data("posts")

users = await task1
posts = await task2
```

### Enums

```
build Color as options:
  Red
  Green
  Blue

print Color.Red         ~ "Red"
print Color.values()    ~ ["Red", "Green", "Blue"]
```

### Shapes (Traits)

```
shape Measurable:
  to area() -> number
  to perimeter() -> number

build Circle fits Measurable:
  to init(r):
    my.r = r
  to area() -> number:
    return 3.14 * my.r * my.r
  to perimeter() -> number:
    return 2 * 3.14 * my.r
```

### Optional Type Hints

```
to add(x: int, y: int) -> int:
  return x + y

add("hello", "world")   ~ Error: parameter 'x' expects int, got string
```

### Error Handling with Stack Traces

```
try:
  risky_operation()
catch error:
  print "Failed: {error}"
finally:
  cleanup()
```

Errors show full stack traces:

```
Stack trace (most recent call last):
  main.to:1 in <main>
  main.to:10 in process
  main.to:5 in validate
<runtime>:3: error: Division by zero
```

## Examples

### Web Server

```
use web

to handle(request):
  if request.path == "/":
    return {status = 200, body = "<h1>Hello!</h1>", type = "text/html"}

  if request.path == "/api/users":
    return {status = 200, body = [{name = "Alice"}, {name = "Bob"}]}

  return {status = 404, body = "Not found"}

web.serve(8080, handle)
```

```bash
$ to run server.to

  To Web Server running on http://localhost:8080
```

### C FFI

```
use ffi

libm = ffi.open("libm")
print libm.call("sqrt", [16.0], "double")   ~ 4.0
print libm.call("pow", [2.0, 10.0], "double") ~ 1024.0
```

### Testing

```
to test_math():
  assert 1 + 1 == 2
  assert add(3, 4) == 7

to test_strings():
  assert "hello".upper() == "HELLO"
  assert len("test") == 4
```

```bash
$ to test math_test.to

  ✓ test_math
  ✓ test_strings

  2 passed, 0 failed
```

## Standard Library

| Module | What it does |
|--------|-------------|
| `math` | `sqrt`, `sin`, `cos`, `pow`, `floor`, `ceil`, `pi`, `e` |
| `time` | `now()`, `sleep()`, `date()`, `clock()`, `year()`, `month()` |
| `fs` | `read()`, `write()`, `exists()`, `list()`, `walk()`, `mkdir()`, `copy()`, `remove()` |
| `regex` | `match()`, `find_all()`, `replace()`, `split()`, `test()` |
| `web` | `serve()`, `json()`, `parse_json()` |
| `json` | `stringify()`, `parse()` |
| `ffi` | `open()`, `sizeof()` — call any C library |
| `io` | `read()`, `write()` |

## CLI

```
to                              Interactive REPL
to run <file.to>                Run a file
to build <file.to>              Compile to native binary
to build <file.to> -o app       Compile with custom name
to check <file.to>              Check for errors
to test <file.to>               Run test functions
to debug <file.to>              Run with debugger
to version                      Print version
```

## Editor Support

**VS Code** — Full extension with:
- Syntax highlighting
- Real-time error checking
- Autocomplete (keywords, builtins, your own functions)
- Hover documentation
- Go to definition (Cmd+Click)
- Document outline

Install: copy `to-vscode/` to `~/.vscode/extensions/to-language-0.2.0/`

## Language at a Glance

| Feature | Syntax |
|---------|--------|
| Variables | `x = 5` |
| Constants | `const PI = 3.14` |
| Functions | `to add(x, y): return x + y` |
| Classes | `build Dog: ...` |
| Inheritance | `build Dog from Animal: ...` |
| Instance ref | `my.name` |
| If/else | `if ... or ... else ...` |
| Pattern match | `given x: if "a": ...` |
| For loop | `through list as item: ...` |
| While loop | `while x < 10: ...` |
| Lambdas | `(x): x * 2` |
| Pipes | `data then transform then output` |
| Destructure | `[a, b] = list` / `{x, y} = dict` |
| Async | `task = async work()` / `await task` |
| Enums | `build Color as options: Red, Green, Blue` |
| Traits | `shape X: ... ` / `build Y fits X: ...` |
| Type hints | `to f(x: int) -> string:` |
| Assertions | `assert x == 5` |
| Strings | `"Hello, {name}!"` or `'single quotes'` |
| Comments | `~ line` / `~' block '~` |
| Ranges | `0..10` |
| Slicing | `list[1..4]` / `str[0..5]` |
| Error handling | `try: ... catch e: ... finally: ...` |
| Imports | `use math` / `use greet from "file.to"` |

---

<p align="center">
  Built by Theo.
</p>
