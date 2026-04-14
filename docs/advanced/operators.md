# Operator Overloading

Classes can define what `+`, `==`, `<`, and other operators mean for their instances. Your custom types get to feel native.

## How it works

When To sees `a + b` where `a` is an instance, it looks for a `plus` method on the class. If found, it calls `a.plus(b)`. Same pattern for other operators:

| Operator | Method |
|----------|--------|
| `+` | `plus` |
| `-` | `minus` |
| `*` | `times` |
| `/` | `divide` |
| `%` | `mod` |
| `==` | `equals` |
| `!=` | `equals` (result inverted) |
| `<` | `less_than` |
| `<=` | `less_equal` |
| `>` | `greater_than` |
| `>=` | `greater_equal` |

## Example — Vector math

```
build Vector:
  to init(x, y):
    my.x = x
    my.y = y

  to plus(other):
    return Vector(my.x + other.x, my.y + other.y)

  to minus(other):
    return Vector(my.x - other.x, my.y - other.y)

  to times(scalar):
    return Vector(my.x * scalar, my.y * scalar)

  to equals(other):
    return my.x == other.x and my.y == other.y

v1 = Vector(1, 2)
v2 = Vector(3, 4)

sum = v1 + v2       ~ Vector(4, 6)
diff = v2 - v1      ~ Vector(2, 2)
scaled = v1 * 10    ~ Vector(10, 20)
same = v1 == v1     ~ true
```

## Example — Comparable objects

```
build Money:
  to init(amount):
    my.amount = amount

  to less_than(other):
    return my.amount < other.amount

  to equals(other):
    return my.amount == other.amount

small = Money(10)
big = Money(100)

print small < big   ~ true
print small == big  ~ false
```

## Tips

- You don't need to define every operator — only the ones that make sense for your type.
- The method receives the right-hand side as its first argument.
- `!=` automatically works if you define `equals`.
