# Pattern Matching

Beyond matching exact values, `given` can destructure dicts and lists — matching their **shape** and extracting values in one step.

## Dict patterns

Match fields by name, optionally binding them to variables:

```
given response:
  if {status: 200, body}:
    print "Success: {body}"       ~ `body` is bound from the dict
  if {status: 404}:
    print "Not found"
  if {status, error}:
    print "Failed ({status}): {error}"
  else:
    print "Unknown"
```

**How it works:**
- `status: 200` means "field `status` must equal 200"
- `body` means "bind field `body` to a variable"
- `status, error` means "both fields must exist, bind both"

## List patterns

Match by structure:

```
given items:
  if [x]:
    print "one element: {x}"
  if [x, y]:
    print "two elements: {x}, {y}"
  if [first, ...rest]:
    print "first={first}, rest has {rest.length} items"
```

## Why it's powerful

```
~ Without pattern matching
if response.status == 200:
  body = response.body
  process(body)
or response.status == 404:
  handle_not_found()
or response.error != none:
  log(response.status, response.error)

~ With pattern matching
given response:
  if {status: 200, body}:
    process(body)
  if {status: 404}:
    handle_not_found()
  if {status, error}:
    log(status, error)
```

The pattern version is shorter, safer, and reads like the data structure it's matching.

## Combining with regular matches

A `given` block can mix patterns and value matches:

```
given event:
  if "start":          ~ value match
    begin()
  if "stop":
    halt()
  if {type: "error", message}:  ~ pattern match
    log(message)
```
