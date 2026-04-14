# Destructuring

## List Destructuring

Unpack list elements into variables:

```
[a, b, c] = [1, 2, 3]
print a                ~ 1
print b                ~ 2
print c                ~ 3
```

## Rest Syntax

Capture remaining elements with `...`:

```
[first, ...rest] = [1, 2, 3, 4, 5]
print first            ~ 1
print rest             ~ [2, 3, 4, 5]

[head, second, ...tail] = ["a", "b", "c", "d"]
print head             ~ "a"
print second           ~ "b"
print tail             ~ ["c", "d"]
```

## Dict Destructuring

Extract values by key name:

```
person = {name = "Theo", age = 18, city = "LA"}

{name, age} = person
print name             ~ "Theo"
print age              ~ 18
```

Keys not present in the dict become `none`:

```
{name, email} = {name = "Theo"}
print email            ~ none
```

## Destructuring Return Values

Unpack functions that return lists or dicts:

```
to get_point():
  return [10, 20]

[x, y] = get_point()
print x                ~ 10
print y                ~ 20

to get_user():
  return {name = "Alice", role = "admin"}

{name, role} = get_user()
print role             ~ "admin"
```

## In Loops

Destructure while iterating:

```
pairs = [[1, "one"], [2, "two"], [3, "three"]]
for [num, word] in pairs:
  print "{num} is {word}"
```

## Swap Variables

```
a = 1
b = 2
[a, b] = [b, a]
print a                ~ 2
print b                ~ 1
```
