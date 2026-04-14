# Lambdas

## Basic Syntax

Lambdas are inline anonymous functions using `(params): expression`:

```
double = (x): x * 2
print double(5)        ~ 10

add = (a, b): a + b
print add(3, 4)        ~ 7
```

## Passing to Higher-Order Functions

Lambdas work naturally with `map`, `filter`, and `reduce`:

```
numbers = [1, 2, 3, 4, 5]

doubled = numbers.map((x): x * 2)
print doubled          ~ [2, 4, 6, 8, 10]

evens = numbers.filter((x): x % 2 == 0)
print evens            ~ [2, 4]

sum = numbers.reduce((acc, x): acc + x, 0)
print sum              ~ 15
```

## Closures

Lambdas capture variables from their surrounding scope:

```
to make_multiplier(factor):
  return (x): x * factor

triple = make_multiplier(3)
print triple(10)       ~ 30

to make_counter():
  count = 0
  return ():
    count = count + 1
    return count

counter = make_counter()
print counter()        ~ 1
print counter()        ~ 2
```

## Closures Over Loop Variables

Each iteration captures its own copy of the variable:

```
funcs = []
for i in range(0, 3):
  funcs.add(((): i))

print funcs[0]()       ~ 0
print funcs[1]()       ~ 1
print funcs[2]()       ~ 2
```

No workarounds needed -- each closure gets the correct value.

## Multi-Line Lambdas

For more complex logic, use an indented block:

```
process = (x):
  result = x * 2
  result = result + 1
  return result

print process(5)       ~ 11
```
