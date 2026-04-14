# Async

## Spawning Tasks

Use `async` to run a function concurrently:

```
to fetch_data(url):
  ~ simulate slow work
  wait(2)
  return "data from {url}"

task = async fetch_data("https://example.com")
```

The function starts running immediately. `task` is a handle you use to get the result later.

## Awaiting Results

Use `await` to get the value from a task:

```
task = async fetch_data("https://example.com")
~ do other work here while task runs...
result = await task
print result           ~ "data from https://example.com"
```

`await` pauses until the task completes and returns its value.

## Running Multiple Tasks Concurrently

Spawn several tasks, then await them all:

```
to slow_double(x):
  wait(1)
  return x * 2

task_a = async slow_double(5)
task_b = async slow_double(10)
task_c = async slow_double(15)

a = await task_a
b = await task_b
c = await task_c

print [a, b, c]        ~ [10, 20, 30]
```

All three tasks run at the same time -- total time is ~1 second, not 3.

## Async in Loops

Spawn tasks in a loop, collect results after:

```
tasks = []
for i in range(1, 6):
  tasks.add(async slow_double(i))

results = []
for t in tasks:
  results.add(await t)

print results          ~ [2, 4, 6, 8, 10]
```

## Error Handling

Errors in async tasks surface when you `await`:

```
to risky():
  fail "something broke"

task = async risky()
try:
  result = await task
handle err:
  print err.message    ~ "something broke"
```
