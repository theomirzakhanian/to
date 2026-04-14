# Control Flow

## If / Or / Else

To uses `or` instead of `elif` or `else if`:

```
age = 18

if age >= 21:
  print "Full access"
or age >= 18:
  print "Limited access"
or age >= 13:
  print "Teen access"
else:
  print "No access"
```

## Given (Pattern Matching)

Match a single value against multiple options:

```
day = "Friday"

given day:
  if "Monday":
    print "Start of the week"
  if "Friday":
    print "Almost weekend!"
  if "Saturday":
    print "Freedom"
  else:
    print "Just another day"
```

## When to Use What

- **if/or/else** — checking different conditions (unrelated tests)
- **given** — matching one value against many possible values

```
~ Use if/or/else for different conditions
if x > 0 and y > 0:
  print "Both positive"
or x == 0:
  print "x is zero"

~ Use given for matching one value
given status:
  if "active":
    handle_active()
  if "paused":
    handle_paused()
```

## Boolean Operators

```
if x > 0 and x < 100:
  print "In range"

if name == "admin" or name == "root":
  print "Superuser"

if not logged_in:
  print "Please log in"
```
