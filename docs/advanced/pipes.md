# Pipes

## The `then` Operator

Chain function calls left-to-right with `then`:

```
to double(x):
  return x * 2

to add_ten(x):
  return x + 10

result = 5 then double then add_ten
print result           ~ 20
```

The value on the left becomes the first argument to the function on the right.

## Piping with Arguments

Pass additional arguments using `function(args)`. The piped value is inserted as the first argument:

```
to multiply(x, y):
  return x * y

to add(x, y):
  return x + y

result = 10 then multiply(3) then add(5)
print result           ~ 35
```

`10 then multiply(3)` calls `multiply(10, 3)`.

## Piping with Lambdas

Use inline lambdas in a pipe chain:

```
result = 7 then (x): x * 2 then (x): x + 1
print result           ~ 15
```

## Chaining List Operations

Pipes make list transformations readable:

```
numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

result = numbers
  then filter((x): x % 2 == 0)
  then map((x): x * 3)
  then reduce((acc, x): acc + x, 0)

print result           ~ 90
```

## Practical Example

```
to fetch_users():
  return [
    {name = "Alice", age = 30},
    {name = "Bob", age = 17},
    {name = "Carol", age = 25}
  ]

adults = fetch_users()
  then filter((u): u.age >= 18)
  then map((u): u.name)

print adults           ~ ["Alice", "Carol"]
```
