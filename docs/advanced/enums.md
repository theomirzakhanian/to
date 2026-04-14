# Enums

## Defining Enums

Use `build ... as options` to define a fixed set of values:

```
build Color as options:
  Red, Green, Blue

build Direction as options:
  North, South, East, West
```

## Accessing Values

Use dot notation:

```
my_color = Color.Red
print my_color         ~ "Red"
```

## Checking Membership

Use `.has()` to test if a string is a valid variant:

```
print Color.has("Red")     ~ true
print Color.has("Yellow")  ~ false
```

## Listing All Values

Use `.values()` to get every variant:

```
print Color.values()   ~ ["Red", "Green", "Blue"]
```

## Pattern Matching with Enums

Combine enums with `given` for clean branching:

```
to describe(color):
  given color:
    when Color.Red:
      return "warm"
    when Color.Blue:
      return "cool"
    when Color.Green:
      return "natural"

print describe(Color.Red)   ~ "warm"
```

## Practical Example

```
build Status as options:
  Pending, Active, Closed

to handle(ticket):
  given ticket.status:
    when Status.Pending:
      print "Waiting for review"
    when Status.Active:
      print "In progress"
    when Status.Closed:
      print "Done"

ticket = {id = 1, status = Status.Active}
handle(ticket)         ~ "In progress"
```
