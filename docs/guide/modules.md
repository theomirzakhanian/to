# Modules & Imports

## Importing Standard Library Modules

```
use math
use time
use fs
use regex
use web
use json
use ffi
```

Then access with dot notation:

```
use math
print math.sqrt(16)    ~ 4
print math.pi          ~ 3.14159
```

## Importing Specific Names

```
use sqrt, pi from math

print sqrt(16)
print pi
```

## Importing from Files

```
use greet, add from "helpers.to"

greet("Theo")
print add(5, 3)
```

## Creating a Module

Any `.to` file is a module. Just define functions and they're importable:

**utils.to:**
```
to double(x):
  return x * 2

to clamp(x, low, high):
  if x < low:
    return low
  if x > high:
    return high
  return x
```

**main.to:**
```
use double, clamp from "utils.to"

print double(21)         ~ 42
print clamp(150, 0, 100) ~ 100
```
