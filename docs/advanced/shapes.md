# Shapes

## Defining a Shape

A shape declares methods a class must implement:

```
shape Printable:
  to display() -> string
```

## Implementing a Shape

Use `fits` when building a class:

```
build User fits Printable:
  has name
  has age

  to display() -> string:
    return "{name}, age {age}"

u = User("Theo", 18)
print u.display()      ~ "Theo, age 18"
```

## Compile-Time Enforcement

If a class claims to fit a shape but is missing a method, you get an error:

```
shape Serializable:
  to serialize() -> string
  to size() -> int

build Thing fits Serializable:
  has value

  to serialize() -> string:
    return str(value)

  ~ Missing size() -- error!
~ Error: Class 'Thing' does not implement 'size' required by shape 'Serializable'
```

## Multiple Shapes

A class can fit more than one shape:

```
shape Printable:
  to display() -> string

shape Saveable:
  to save()

build Document fits Printable, Saveable:
  has title
  has content

  to display() -> string:
    return title

  to save():
    print "Saving {title}..."
```

## When to Use Shapes

- **Define contracts** between parts of your codebase
- **Enforce consistency** across classes that serve the same role
- **Document intent** -- a shape tells readers what behavior is expected

```
shape Handler:
  to handle(request) -> dict

~ Any class fitting Handler is guaranteed to have handle()
~ This lets you write code that works with any Handler
to process(h: Handler, req):
  return h.handle(req)
```
