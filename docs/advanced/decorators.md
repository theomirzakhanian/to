# Decorators

Decorators wrap functions without cluttering their body. Use them for logging, caching, timing, authentication — anything that's orthogonal to the function's logic.

## The idea

A decorator is just a function that takes a function and returns a new function:

```
to logged(fn):
  to wrapped(x):
    print "Calling with {x}"
    result = fn(x)
    print "Got {result}"
    return result
  return wrapped
```

Apply it with `@decorator_name` right before a function definition:

```
@logged
to double(x):
  return x * 2

double(5)
~ Calling with 5
~ Got 10
~ Returns: 10
```

## Stacking decorators

Multiple decorators stack — innermost first (closest to the function):

```
@logged
@cached
to expensive(x):
  ~ ... slow work ...
  return x * x
```

This is equivalent to: `expensive = logged(cached(original_expensive))`.

## Common patterns

### Caching

```
to cached(fn):
  store = {}
  to wrapped(x):
    key = str(x)
    if store.has(key):
      return store[key]
    result = fn(x)
    store[key] = result
    return result
  return wrapped

@cached
to fib(n):
  if n <= 1:
    return n
  return fib(n - 1) + fib(n - 2)
```

### Timing

```
use time

to timed(fn):
  to wrapped(x):
    start = time.ms()
    result = fn(x)
    print "Took {time.ms() - start}ms"
    return result
  return wrapped

@timed
to slow_work(n):
  ~ ...
```

### Authentication

```
to requires_auth(fn):
  to wrapped(request):
    if not request.authenticated:
      return {status = 401, body = "Unauthorized"}
    return fn(request)
  return wrapped

@requires_auth
to handle_admin(request):
  return {status = 200, body = "Admin panel"}
```

## Tips

- Decorators run once at function-definition time, not every call.
- The returned function can have any signature, but usually matches the original.
- Combining `@cached` with `@timed` shows cache hit speedup clearly.
