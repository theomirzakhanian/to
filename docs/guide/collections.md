# Collections

To has eight ways to hold more than one value. The trick to learning them
is that you only have to learn them once: **the same words work on all of
them.**

```
box.add(value)     ~ put one in
box.pop()          ~ take the next one out
box.peek()         ~ look at the next one, leave it there
box.has(value)     ~ is it in there
box.remove(value)  ~ take that one out
box.length         ~ how many
```

What changes is *which* value `pop()` gives you back. That is the entire
difference between a stack and a queue, and it is the only thing you have
to decide.

```
to fill(box):
  box.add(1)
  box.add(2)
  box.add(3)
  return box.pop()

print fill([])        ~ 3   a list gives you the newest
print fill(stack())   ~ 3   so does a stack
print fill(queue())   ~ 1   a queue gives you the one that waited longest
print fill(heap())    ~ 1   a heap gives you the smallest
```

Forgotten a name? Ask:

```
help(queue())     ~ prints everything a queue can do
help()            ~ prints this page, short
```

And if you type a word from another language, `to` tells you its own:

```
s = stack()
s.push(1)
~ A stack has no 'push' — in to it is called 'add'. Try stack.add(value)
```

## The eight

| | Written | Holds | `pop()` gives you |
|---|---|---|---|
| **list** | `[1, 2, 3]` | anything, in a row | the last one |
| **tuple** | `(1, 2, 3)` | a fixed row — cannot change | — |
| **set** | `{1, 2, 3}` | each value once | the one added first |
| **dict** | `{name = "Theo"}` | values under keys | — |
| **deque** | `deque()` | a row, open at both ends | the last one |
| **queue** | `queue()` | a waiting line | the one that waited longest |
| **stack** | `stack()` | a pile | the one on top |
| **heap** | `heap()` | anything | the smallest |

Start with `list` and `dict`. Reach for the others when the name of the
thing matches what you are doing — a `queue` of jobs, a `stack` of undo
steps, a `set` of tags you do not want repeated.

## Lists

The one you will use most.

```
xs = [5, 3, 1]
xs.add(9)
print xs.length     ~ 4
print xs[0]         ~ 5
print xs[-1]        ~ 9    counting back from the end
xs.sort()
print xs            ~ [1, 3, 5, 9]
```

Lists also know how to reshape themselves. Every one of these hands back
a *new* list and leaves the original alone:

```
nums = [1, 2, 3, 4, 5]

print nums.map((x): x * 2)          ~ [2, 4, 6, 8, 10]
print nums.filter((x): x % 2 == 0)  ~ [2, 4]
print nums.reduce((a, b): a + b, 0) ~ 15
print nums.sum()                    ~ 15
print nums.first(2)                 ~ [1, 2]
print nums.last(2)                  ~ [4, 5]
print nums.skip(3)                  ~ [4, 5]
print nums.chunk(2)                 ~ [[1, 2], [3, 4], [5]]
print nums.zip(["a", "b"])          ~ [(1, "a"), (2, "b")]
print [1, 1, 2].unique()            ~ [1, 2]
print [[1, 2], [3]].flatten()       ~ [1, 2, 3]
```

When two methods do the same job, the shorter name changes the thing and
the longer name hands back a new one:

```
xs.sort()        ~ changes xs
xs.sorted()      ~ leaves xs alone, gives you a sorted copy
xs.reverse()     ~ changes xs
xs.reversed()    ~ leaves xs alone
```

Sorting takes a function when you want to sort by something other than
the value itself:

```
words = ["pear", "fig", "banana"]
words.sort((w): w.length)
print words                  ~ ["fig", "pear", "banana"]
```

Positions are for lists only, and they say so in their names:

```
xs.insert(0, "first")   ~ put it at a position
xs.remove_at(0)         ~ take out whatever is at a position
xs.remove("first")      ~ take out that value, wherever it is
```

## Tuples

A tuple is a row that cannot change. That is what makes it safe to use as
a dict key or a set member.

```
point = (3, 4)
print point[0]      ~ 3
print point.length  ~ 2

single = (7,)       ~ the trailing comma is what makes it a tuple
empty = ()
```

One thing to watch: `print (1, 2)` is read as `print(1, 2)` — the function
form of `print`, given two values. Put the tuple in a variable first, or
write `print((1, 2))`, when you want to see the tuple itself.

```
distances = {}
distances[(0, 0)] = "origin"
print distances[(0, 0)]   ~ origin
```

Trying to change one tells you what to do instead:

```
point.add(5)
~ A tuple cannot be changed, so it has no 'add'.
~ Call to_list() first if you need to change it.
```

## Sets

A set keeps each value once, in the order the values first showed up.

```
tags = {"blue", "red", "blue"}
print tags          ~ {"blue", "red"}
print tags.length   ~ 2
print tags.has("red")

tags.add("green")
tags.remove("blue")
```

Comparing two sets works either with words or with operators — pick
whichever reads better where you are:

```
a = {1, 2, 3}
b = {3, 4, 5}

print a.union(b)                 ~ {1, 2, 3, 4, 5}   same as a + b
print a.difference(b)            ~ {1, 2}            same as a - b
print a.intersect(b)             ~ {3}
print a.symmetric_difference(b)  ~ {1, 2, 4, 5}

print a.is_subset({1, 2, 3, 4})  ~ true
print a.is_superset({1, 2})      ~ true
print a.is_disjoint({9})         ~ true
```

An empty set is `set()`. A bare `{}` is an empty dict.

## Dicts

Dicts are the one container that works by key rather than by position, so
they have their own small vocabulary on top of the shared one.

```
person = {name = "Theo", age = 30}
print person["name"]
print person.name        ~ the same thing, for keys that look like names
```

Keys can be anything that does not change — text, numbers, `true`,
`none`, and tuples:

```
scores = {}
scores["ana"] = 10
scores[7] = "lucky"
scores[(1, 2)] = "a pair"
```

```
print person.keys()      ~ ["name", "age"]
print person.values()    ~ ["Theo", 30]
print person.entries()   ~ [("name", "Theo"), ("age", 30)]

print person.get("name", "unknown")   ~ Theo
print person.get("email", "unknown")  ~ unknown   no error when missing
print person.has("age")               ~ true
person.set("city", "Lisbon")          ~ same as person["city"] = "Lisbon"
person.remove("age")

print person.merge({city = "Porto"})  ~ a new dict; person is untouched
print person.map_values((v): v)
print person.filter((k, v): k == "name")
print person.invert()
```

Walking a dict gives you its keys, in the order they were added:

```
through person as key:
  print "{key} = {person[key]}"
```

## Queues, stacks and deques

These three hold a row of values. They differ only in which end you are
allowed to touch — which is the point of choosing one.

```
jobs = queue()
jobs.add("email")
jobs.add("backup")
print jobs.pop()      ~ email    the one that has waited longest
```

```
undo = stack()
undo.add("typed hello")
undo.add("deleted a line")
print undo.pop()      ~ deleted a line    the most recent
```

A deque is open at both ends. It uses the shared verbs for the back, and
`_first` versions for the front:

```
d = deque([2, 3])
d.add_first(1)     ~ deque[1, 2, 3]
d.add(4)           ~ deque[1, 2, 3, 4]
print d.pop_first()   ~ 1
print d.pop()         ~ 4
print d.peek_first()  ~ 2
d.rotate(1)
```

## Heaps

A heap always hands you the smallest value, without going to the trouble
of keeping everything in order. Use `max_heap()` when you want the largest.

```
h = heap([5, 1, 3])
h.add(0)
print h.peek()    ~ 0
print h.pop()     ~ 0
print h.pop()     ~ 1

biggest = max_heap([5, 1, 3])
print biggest.pop()   ~ 5
```

```
to heap_sort(items):
  h = heap(items)
  out = []
  while h.length > 0:
    out.add(h.pop())
  return out
```

## Slicing

Every container with an order, and every string, can be sliced. You get
back the same kind of thing you started with.

```
nums = [0, 1, 2, 3, 4, 5]

print nums[1:3]    ~ [1, 2]
print nums[:2]     ~ [0, 1]
print nums[3:]     ~ [3, 4, 5]
print nums[-2:]    ~ [4, 5]
print nums[::2]    ~ [0, 2, 4]     every second one
print nums[::-1]   ~ [5, 4, 3, 2, 1]

s = "Hello, World!"
print s[0:5]       ~ Hello
print s[::-1]      ~ !dlroW ,olleH
```

## Changing one into another

Every container converts to every other, with the same word every time:

```
xs = [3, 1, 2, 1]

print xs.to_set()      ~ {3, 1, 2}
print xs.to_tuple()    ~ (3, 1, 2, 1)
print xs.to_queue()
print xs.to_stack()
print xs.to_deque()
print xs.to_heap()
print {1, 2}.to_list() ~ [1, 2]
```

The constructors take a collection too, so `set(my_list)` does the same
as `my_list.to_set()`.

## Comparing

Collections compare by what is inside them, not by which one they are:

```
pair = (1, 2)
print [1, 2] == [1, 2]           ~ true
print pair == (1, 2)             ~ true
print {1, 2} == {2, 1}           ~ true    order does not matter to a set
print {a = 1} == {a = 1}         ~ true
print [1, [2, 3]] == [1, [2, 3]] ~ true
```

Sorting a list of mixed types never fails — values of different types
fall into a stable order by type.
