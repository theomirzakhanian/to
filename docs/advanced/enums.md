# Enums & Sum Types

Enums define a fixed set of named values. They come in two flavors: **plain** (just labels) and **sum types** (labels that carry data).

## Plain enums

```
build Color as options:
  Red
  Green
  Blue

print Color.Red          ~ "Red"
print Color.values()     ~ ["Red", "Green", "Blue"]
print Color.has("Red")   ~ true
```

Use with `given`:

```
given current:
  if "Red":
    stop()
  if "Green":
    go()
```

## Sum types — enums that carry data

Each variant can hold its own fields:

```
build Result as options:
  Ok(value)
  Err(message)

to divide(a, b):
  if b == 0:
    return Result.Err("cannot divide by zero")
  return Result.Ok(a / b)
```

## Pattern matching

Match each variant and bind its fields:

```
r = divide(10, 2)

given r:
  if Ok(v):
    print "Got {v}"           ~ v is bound to the value
  if Err(msg):
    print "Failed: {msg}"     ~ msg is bound to the message
```

## Common use cases

### Option (Maybe)

```
build Option as options:
  Some(value)
  None

to find_user(id):
  if id == 1:
    return Option.Some("Theo")
  return Option.None

given find_user(1):
  if Some(name):
    print "Found {name}"
  else:
    print "Not found"
```

### Events — variants with different arities

```
build Event as options:
  Click(x, y)
  KeyPress(key)
  Scroll(delta)
  Close

to handle(event):
  given event:
    if Click(x, y):
      print "Click at ({x}, {y})"
    if KeyPress(key):
      print "Key: {key}"
    if Scroll(delta):
      print "Scroll: {delta}"
    else:
      print "Close"
```

### State machines

```
build TrafficLight as options:
  Red
  Yellow
  Green

to next(light):
  given light:
    if "Red":
      return TrafficLight.Green
    if "Green":
      return TrafficLight.Yellow
    if "Yellow":
      return TrafficLight.Red
```

## Why sum types matter

Sum types make "impossible states unrepresentable." A `Result` either has a value OR an error — never both, never neither. Pattern matching pushes you to handle every case, so bugs get caught at review instead of runtime.
