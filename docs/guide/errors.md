# Error Handling

## Try / Catch / Finally

```
try:
  result = 10 / 0
catch error:
  print "Something went wrong: {error}"
finally:
  print "This always runs"
```

`finally` is optional and always executes regardless of success or failure.

## Stack Traces

When an error occurs deep in your code, you get a full stack trace:

```
Stack trace (most recent call last):
  main.to:1 in <main>
  main.to:15 in process_data
  main.to:8 in validate
<runtime>:3: error: Index out of bounds: 99
```

## Assertions

Use `assert` to verify conditions:

```
assert x > 0
assert name != ""
assert items.length == 3
```

Failed assertions throw an error with the line number:

```
Error: Assertion failed at line 12
```
