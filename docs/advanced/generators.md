# Generators

Generators are functions that **pause and resume**. They use the `yield` keyword to produce values lazily — one at a time, only when asked.

## Why use them?

When you only need one value at a time, generators avoid building huge lists in memory. They're perfect for streams, infinite sequences, and paginated data.

## Defining a generator

Any function that uses `yield` becomes a generator:

```
to counter(start, stop):
  i = start
  while i < stop:
    yield i
    i = i + 1
```

Calling `counter(0, 5)` doesn't run the body — it returns a generator object.

## Iterating with `through`

The easiest way to consume a generator is a `through` loop:

```
through counter(0, 5) as n:
  print n
~ prints 0, 1, 2, 3, 4
```

## Manual iteration

Use `.next()` to get one value at a time:

```
g = counter(10, 20)
print g.next()   ~ 10
print g.next()   ~ 11
print g.next()   ~ 12
```

## Collecting to a list

```
g = counter(0, 5)
print g.to_list()   ~ [0, 1, 2, 3, 4]
```

## Fibonacci example

```
to fib(n):
  a = 0
  b = 1
  count = 0
  while count < n:
    yield a
    temp = a + b
    a = b
    b = temp
    count = count + 1

print fib(10).to_list()   ~ [0, 1, 1, 2, 3, 5, 8, 13, 21, 34]
```

## Breaking out early

`break` in a `through` loop cleanly terminates the generator:

```
through counter(0, 1000000) as n:
  if n > 100:
    break
  print n
```

The generator only produces the 101 values you asked for — not a million.
