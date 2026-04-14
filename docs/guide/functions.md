# Functions

## Defining Functions

Use the `to` keyword:

```
to greet(name):
  print "Hey, {name}!"

to add(x, y):
  return x + y
```

## Calling Functions

```
greet("Theo")           ~ Hey, Theo!
result = add(3, 4)      ~ 7
```

## Return Values

Functions without an explicit `return` return `none`:

```
to say_hi():
  print "Hi!"

result = say_hi()    ~ result is none
```

## Type Hints (Optional)

Add type annotations for safety:

```
to add(x: int, y: int) -> int:
  return x + y

to greet(name: string) -> string:
  return "Hello, {name}!"

to process(data: list) -> dict:
  return {count = data.length}
```

Available types: `int`, `float`, `number` (int or float), `string`, `bool`, `list`, `dict`, `any`, `none`

Type errors are caught at runtime:

```
add("hello", "world")
~ Error: Function 'add' parameter 'x' expects int, got string
```

## Built-in Functions

| Function | Description |
|----------|-------------|
| `print value` | Print to stdout |
| `input(prompt)` | Read line from stdin |
| `len(x)` | Length of string/list/dict |
| `type(x)` | Type name as string |
| `int(x)` | Convert to integer |
| `float(x)` | Convert to float |
| `str(x)` | Convert to string |
| `range(start, end)` | Create range list |
| `abs(x)` | Absolute value |
| `min(a, b)` | Minimum |
| `max(a, b)` | Maximum |
| `round(x)` | Round to nearest int |
