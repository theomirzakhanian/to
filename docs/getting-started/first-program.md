# Your First Program

## Hello World

Create a file called `hello.to`:

```
print "Hello, World!"
```

Run it:

```bash
to run hello.to
```

That's it. No boilerplate, no imports, no main function.

## Variables and Input

```
name = input("What's your name? ")
age = int(input("How old are you? "))

print "Hey {name}, you're {age} years old!"

if age >= 18:
  print "You're an adult!"
else:
  print "You're still young!"
```

## Your First Function

```
to factorial(n: int) -> int:
  if n <= 1:
    return 1
  return n * factorial(n - 1)

through 1..11 as i:
  print "{i}! = {factorial(i)}"
```

## Your First Class

```
build Counter:
  to init(start):
    my.count = start

  to increment():
    my.count = my.count + 1

  to value():
    return my.count

c = Counter(0)
c.increment()
c.increment()
c.increment()
print c.value()   ~ 3
```

## Running, Checking, and Testing

```bash
to run app.to         # run your program
to check app.to       # check for errors without running
to test app.to        # run test functions
to build app.to       # compile to native binary
```

## REPL

Just type `to` with no arguments for an interactive prompt:

```
$ to
To v0.2.0 — Interactive REPL
Type 'exit' or press Ctrl+C to quit.

to> 2 + 2
4
to> name = "Theo"
to> print "Hello, {name}!"
Hello, Theo!
to> exit
```
