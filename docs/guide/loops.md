# Loops

## Through Loops

Iterate over lists, ranges, strings, or dicts:

```
~ Over a list
through ["Alice", "Bob", "Charlie"] as name:
  print "Hello, {name}!"

~ Over a range
through 0..10 as i:
  print i

~ Over a string
through "hello" as ch:
  print ch

~ Over dict keys
through {a = 1, b = 2} as key:
  print key
```

## While Loops

```
x = 0
while x < 10:
  print x
  x = x + 1
```

## Break and Continue

```
through 0..100 as i:
  if i == 5:
    break        ~ stop the loop
  print i

through 0..10 as i:
  if i % 2 == 0:
    continue     ~ skip to next iteration
  print i        ~ only prints odd numbers
```

## Ranges

The `..` operator creates a range (exclusive end):

```
0..5       ~ [0, 1, 2, 3, 4]
1..4       ~ [1, 2, 3]
```
