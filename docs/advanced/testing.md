# Testing

## Running Tests

Use the `to test` command to run all tests in a file:

```
to test math_tests.to
```

## Writing Tests

Name test functions with a `test_` prefix and use `assert`:

```
~ math_tests.to

to add(x, y):
  return x + y

to test_addition():
  assert add(2, 3) == 5
  assert add(-1, 1) == 0
  assert add(0, 0) == 0

to test_string_concat():
  result = "hello" + " " + "world"
  assert result == "hello world"
```

## Assert

`assert` checks that a condition is true. If it fails, the test fails:

```
to test_basics():
  assert 1 + 1 == 2
  assert true
  assert len([1, 2, 3]) == 3
  assert "hello".length == 5
```

You can add a message:

```
to test_with_message():
  value = 42
  assert value > 0, "value should be positive"
```

## Test Output

```
$ to test math_tests.to
  check test_addition
  check test_string_concat

2 passed, 0 failed
```

Failed tests show what went wrong:

```
$ to test broken_tests.to
  check test_one
  cross test_two
    AssertionError: expected 4, got 5
    at broken_tests.to:8

1 passed, 1 failed
```

## Organizing Tests

Keep tests in a `tests/` directory or alongside source files:

```
project/
  math.to
  math_test.to
  utils.to
  utils_test.to
```

Run all tests in a directory:

```
to test tests/
```
