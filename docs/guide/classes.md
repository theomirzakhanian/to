# Classes

## Defining a Class

Use `build` to define a class. Use `my` to refer to the instance (like `self` or `this`):

```
build Person:
  to init(name, age):
    my.name = name
    my.age = age

  to greet():
    print "I'm {my.name}, age {my.age}"
```

## Creating Instances

```
theo = Person("Theo", 18)
theo.greet()          ~ I'm Theo, age 18
print theo.name       ~ Theo
```

## Inheritance

Use `from` to inherit from a parent class:

```
build Animal:
  to init(name, sound):
    my.name = name
    my.sound = sound
  to speak():
    print "{my.name} says {my.sound}!"

build Dog from Animal:
  to init(name):
    my.name = name
    my.sound = "Woof"
    my.tricks = []
  to learn(trick):
    my.tricks.add(trick)
```

Child classes inherit parent methods they don't override:

```
rex = Dog("Rex")
rex.speak()     ~ Rex says Woof!  (inherited from Animal)
rex.learn("sit")
```

## Shapes (Traits)

Define an interface with `shape`, implement it with `fits`:

```
shape Printable:
  to display() -> string

build User fits Printable:
  to init(name):
    my.name = name
  to display() -> string:
    return "User: {my.name}"
```

If a class `fits` a shape but is missing a required method, you get an error at definition time.

## Enums

Define a fixed set of values with `options`:

```
build Status as options:
  Active
  Paused
  Stopped

print Status.Active   ~ "Active"
Status.has("Active")  ~ true
Status.values()       ~ ["Active", "Paused", "Stopped"]
```
