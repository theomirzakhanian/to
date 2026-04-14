<div align="center">

# To

### A programming language that reads like English.

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-0.2.0-brightgreen.svg)](#)
[![Built with C++](https://img.shields.io/badge/built%20with-C%2B%2B17-orange.svg)](#)
[![Tests](https://img.shields.io/badge/tests-19%20passing-success.svg)](#)

[**Documentation**](https://theo-10.gitbook.io/theo-docs) &nbsp;&middot;&nbsp; [**Install**](#-install) &nbsp;&middot;&nbsp; [**Examples**](#-examples) &nbsp;&middot;&nbsp; [**VS Code Extension**](#-editor-support)

---

```
to greet(name: string):
  print "Hey, {name}!"

greet("World")
```

</div>

&nbsp;

**To** is a programming language designed to be radically easy to learn while being powerful enough to build real projects. It compiles and interprets `.to` files, and ships with a built-in HTTP server, C FFI, regex, filesystem, async/await, a debugger, and a test runner.

Built from scratch in **8,400 lines of C++**. No dependencies.

&nbsp;

## Why To?

```
~ Python                          ~ To
def greet(name):                  to greet(name):
    print(f"Hey, {name}!")            print "Hey, {name}!"
                                
class Dog:                        build Dog:
    def __init__(self, name):       to init(name):
        self.name = name                my.name = name
                                
for item in items:                through items as item:
    print(item)                       print item
                                
if x > 0:                        if x > 0:
    pass                             print "yes"
elif x == 0:                     or x == 0:
    pass                             print "zero"
else:                            else:
    pass                             print "no"
```

Same power. Less noise. Reads like English.

&nbsp;

---

## 📦 Install

```bash
git clone https://github.com/theomirzakhanian/to.git
cd to
mkdir build && cd build
cmake .. && cmake --build .
sudo cp build/to /usr/local/bin/to
```

Verify:

```bash
$ to version
To v0.2.0
```

&nbsp;

---

## 🚀 Quick Start

```
print "Hello, World!"

name = input("What's your name? ")
print "Nice to meet you, {name}!"
```

```bash
$ to run hello.to
Hello, World!
What's your name? Theo
Nice to meet you, Theo!
```

&nbsp;

---

## 📖 The Language

<details>
<summary><b>Variables & Types</b></summary>

&nbsp;

```
age = 18              ~ mutable
const PI = 3.14       ~ immutable

x = 5
x = "hello"           ~ dynamic typing — totally fine
```

Types: `int`, `float`, `string`, `bool`, `none`, `list`, `dict`

</details>

<details>
<summary><b>Functions & Type Hints</b></summary>

&nbsp;

```
to add(x: int, y: int) -> int:
  return x + y

to greet(name):
  print "Hey, {name}!"

~ Type hints are optional — these work the same:
to double(x):       ~ no hints
  return x * 2
to double(x: number) -> number:   ~ with hints
  return x * 2
```

</details>

<details>
<summary><b>Control Flow</b></summary>

&nbsp;

```
if age >= 21:
  print "Welcome"
or age >= 18:
  print "Almost"
else:
  print "Not yet"
```

Pattern matching with `given`:

```
given day:
  if "Monday":
    print "Pain"
  if "Friday":
    print "Freedom"
  else:
    print "Meh"
```

</details>

<details>
<summary><b>Loops</b></summary>

&nbsp;

```
through 1..10 as i:
  print i

through names as name:
  print "Hello, {name}!"

while x < 100:
  x = x + 1
```

</details>

<details>
<summary><b>Classes & Inheritance</b></summary>

&nbsp;

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
rex.speak()         ~ Rex says Woof!
rex.learn("sit")
```

</details>

<details>
<summary><b>Lambdas & Pipes</b></summary>

&nbsp;

```
double = (x): x * 2
add = (a, b): a + b

~ Pipe operator — chain with 'then'
result = [1, 2, 3, 4, 5]
  then filter((x): x > 2)
  then map((x): x * x)
  then sum

print result   ~ 50
```

</details>

<details>
<summary><b>Destructuring</b></summary>

&nbsp;

```
[first, second, ...rest] = [1, 2, 3, 4, 5]
print rest   ~ [3, 4, 5]

{name, age} = {name = "Theo", age = 18}
print name   ~ Theo
```

</details>

<details>
<summary><b>Async / Await</b></summary>

&nbsp;

```
task1 = async slow_compute(1000)
task2 = async slow_compute(2000)

result1 = await task1
result2 = await task2
~ Both ran in parallel
```

</details>

<details>
<summary><b>Enums & Shapes</b></summary>

&nbsp;

```
build Color as options:
  Red
  Green
  Blue

print Color.Red           ~ "Red"
print Color.values()      ~ ["Red", "Green", "Blue"]

shape Measurable:
  to area() -> number

build Circle fits Measurable:
  to init(r):
    my.r = r
  to area() -> number:
    return 3.14 * my.r * my.r
```

</details>

<details>
<summary><b>Error Handling</b></summary>

&nbsp;

```
try:
  risky_operation()
catch error:
  print "Failed: {error}"
finally:
  cleanup()
```

Full stack traces on errors:

```
Stack trace (most recent call last):
  main.to:1 in <main>
  main.to:10 in process
  main.to:5 in validate
<runtime>:3: error: Division by zero
```

</details>

&nbsp;

---

## 🔥 Examples

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

```
$ to run server.to

  To Web Server running on http://localhost:8080
```

### C FFI

```
use ffi

libm = ffi.open("libm")
print libm.call("sqrt", [16.0], "double")      ~ 4.0
print libm.call("pow", [2.0, 10.0], "double")   ~ 1024.0
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

```
$ to test math_test.to

  ✓ test_math
  ✓ test_strings

  2 passed, 0 failed
```

&nbsp;

---

## 📚 Standard Library

| Module | What it does |
|--------|-------------|
| **math** | `sqrt`, `sin`, `cos`, `pow`, `floor`, `ceil`, `pi`, `e` |
| **time** | `now()`, `sleep()`, `date()`, `clock()`, `year()`, `month()` |
| **fs** | `read()`, `write()`, `exists()`, `list()`, `walk()`, `mkdir()`, `copy()`, `remove()` |
| **regex** | `match()`, `find_all()`, `replace()`, `split()`, `test()` |
| **web** | `serve()`, `json()`, `parse_json()` — multi-threaded HTTP server |
| **json** | `stringify()`, `parse()` |
| **ffi** | `open()`, `sizeof()` — call any C library |

&nbsp;

---

## 🛠 CLI

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

&nbsp;

---

## ✏️ Editor Support

**VS Code** — Full extension included in `editor/vscode/`:

- Syntax highlighting with string interpolation
- Real-time error checking (red squiggles)
- Autocomplete for keywords, builtins, modules, and your own code
- Hover documentation
- Go to definition (Cmd+Click)
- Document outline

Install:

```bash
cp -r editor/vscode ~/.vscode/extensions/to-language-0.2.0
# Restart VS Code
```

&nbsp;

---

## 📋 Language at a Glance

| Feature | Syntax |
|---------|--------|
| Variables | `x = 5` |
| Constants | `const PI = 3.14` |
| Functions | `to add(x, y): return x + y` |
| Classes | `build Dog: ...` |
| Inheritance | `build Dog from Animal: ...` |
| Instance ref | `my.name` |
| If / else | `if ... or ... else ...` |
| Pattern match | `given x: if "a": ...` |
| For loop | `through list as item: ...` |
| While loop | `while x < 10: ...` |
| Lambdas | `(x): x * 2` |
| Pipes | `data then transform then output` |
| Destructure | `[a, b] = list` / `{x, y} = dict` |
| Async | `task = async work()` / `await task` |
| Enums | `build Color as options: Red, Green, Blue` |
| Shapes | `shape X: ...` / `build Y fits X: ...` |
| Type hints | `to f(x: int) -> string:` |
| Assert | `assert x == 5` |
| Strings | `"Hello, {name}!"` or `'single quotes'` |
| Comments | `~ line` / `~' block '~` |
| Ranges | `0..10` |
| Slicing | `list[1..4]` / `str[0..5]` |
| Errors | `try: ... catch e: ... finally: ...` |
| Imports | `use math` / `use greet from "file.to"` |

&nbsp;

---

<div align="center">

**[Read the full documentation](https://theo-10.gitbook.io/theo-docs)**

Built by Theo.

</div>
