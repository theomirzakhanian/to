# Type Hints

## Parameter Types

Add optional type annotations to function parameters:

```
to greet(name: string):
  print "Hey, {name}!"

to add(x: int, y: int):
  return x + y
```

## Return Types

Specify what a function returns with `->`:

```
to double(x: int) -> int:
  return x * 2

to full_name(first: string, last: string) -> string:
  return "{first} {last}"
```

## Available Types

| Type | Matches |
|------|---------|
| `int` | Whole numbers (`42`) |
| `float` | Decimal numbers (`3.14`) |
| `number` | Either `int` or `float` |
| `string` | Text (`"hello"`) |
| `bool` | `true` / `false` |
| `list` | Lists (`[1, 2, 3]`) |
| `dict` | Dicts (`{a = 1}`) |
| `any` | Any type (no checking) |
| `none` | The `none` value |

## Runtime Checking

Type mismatches are caught when the function is called:

```
to add(x: int, y: int) -> int:
  return x + y

add(1, 2)              ~ 3
add("a", "b")          ~ Error: parameter 'x' expects int, got string
```

Return type violations are also caught:

```
to get_name() -> string:
  return 42
~ Error: Function 'get_name' expected to return string, got int
```

## Backward Compatibility

Type hints are fully optional. Untyped functions work exactly as before:

```
to add(x, y):
  return x + y

add(1, 2)              ~ 3
add("a", "b")          ~ "ab"
```

You can annotate some parameters and leave others untyped:

```
to search(query: string, limit):
  ~ query is checked, limit accepts anything
  ...
```
