# Variables & Types

## Declaring Variables

Variables are created by assignment. No keyword needed:

```
name = "Theo"
age = 18
pi = 3.14159
active = true
```

## Constants

Use `const` for values that should never change:

```
const MAX_SIZE = 100
const APP_NAME = "MyApp"

MAX_SIZE = 200   ~ Error: Cannot reassign constant 'MAX_SIZE'
```

## Dynamic Typing

Variables can hold any type and change types freely:

```
x = 5
x = "now a string"
x = [1, 2, 3]
x = true
```

## Types

| Type | Example | Description |
|------|---------|-------------|
| `int` | `42` | Whole numbers |
| `float` | `3.14` | Decimal numbers |
| `string` | `"hello"` or `'hello'` | Text |
| `bool` | `true` / `false` | Boolean |
| `none` | `none` | Absence of value |
| `list` | `[1, 2, 3]` | Ordered collection |
| `dict` | `{name = "Theo"}` | Key-value pairs |

## Strings

Both single and double quotes work:

```
greeting = "Hello"
greeting = 'Hello'
```

String interpolation uses `{}`:

```
name = "Theo"
print "Hey, {name}!"
print "2 + 2 = {2 + 2}"
```

Escape sequences: `\n` `\t` `\\` `\"` `\'`

## Lists

```
numbers = [1, 2, 3, 4, 5]
print numbers[0]       ~ 1
print numbers[-1]      ~ 5 (last element)
print numbers[1..3]    ~ [2, 3] (slicing)

numbers.add(6)
numbers.remove(0)
numbers.pop()
print numbers.length
```

**Methods:** `add`, `remove`, `pop`, `contains`, `join`, `reverse`, `length`

## Dictionaries

```
person = {name = "Theo", age = 18, city = "LA"}
print person.name
person.age = 19
```

**Methods:** `keys`, `values`, `has`

## Type Checking

```
print type(42)       ~ "int"
print type("hello")  ~ "string"
print type([1,2])    ~ "list"
```

## Type Conversion

```
x = int("42")        ~ 42
y = float("3.14")    ~ 3.14
s = str(42)          ~ "42"
```

## Truthiness

| Value | Truthy? |
|-------|---------|
| `0` | No |
| `""` | No |
| `[]` | No |
| `none` | No |
| `false` | No |
| Everything else | Yes |
