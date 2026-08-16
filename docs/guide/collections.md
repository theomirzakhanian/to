# Collections

To ships eight collection types. Pick the one that matches what you are
actually doing, and the code says what it means.

| Type | Literal | Good at | Ordered | Duplicates |
|------|---------|---------|---------|------------|
| `list` | `[1, 2, 3]` | Everything, by default | Yes | Yes |
| `tuple` | `(1, 2, 3)` | Fixed groups, dict keys | Yes | Yes |
| `set` | `{1, 2, 3}` | Membership, deduplication | Insertion | No |
| `dict` | `{name = "Theo"}` | Lookup by key | Insertion | Keys are unique |
| `deque` | `deque([1, 2])` | Adding and removing at both ends | Yes | Yes |
| `queue` | `queue([1, 2])` | First in, first out | Yes | Yes |
| `stack` | `stack([1, 2])` | Last in, first out | Yes | Yes |
| `heap` | `heap([3, 1])` | Always getting the smallest next | By priority | Yes |

## Lists

```
xs = [5, 3, 1]
xs.add(9)          ~ [5, 3, 1, 9]
xs.sort()          ~ [1, 3, 5, 9]
print xs.length    ~ 4
print xs[0]        ~ 1
print xs[-1]       ~ 9
```

Lists carry the full set of transformations:

```
nums = [1, 2, 3, 4, 5]

print nums.map((x): x * 2)          ~ [2, 4, 6, 8, 10]
print nums.filter((x): x % 2 == 0)  ~ [2, 4]
print nums.reduce((a, b): a + b, 0) ~ 15
print nums.sum()                    ~ 15
print nums.min()                    ~ 1
print nums.max()                    ~ 5
print nums.take(2)                  ~ [1, 2]
print nums.drop(3)                  ~ [4, 5]
print nums.chunk(2)                 ~ [[1, 2], [3, 4], [5]]
print nums.zip(["a", "b"])          ~ [(1, "a"), (2, "b")]
print nums.reversed()               ~ [5, 4, 3, 2, 1]
print [[1, 2], [3]].flatten()       ~ [1, 2, 3]
print [1, 1, 2].unique()            ~ [1, 2]
```

Sorting takes an optional key function:

```
words = ["pear", "fig", "banana"]
words.sort((w): w.length)
print words              ~ ["fig", "pear", "banana"]
print words.sorted_desc((w): w.length)
```

`sort()` rearranges the list in place; `sorted()` leaves it alone and hands
back a new one. The same pairing holds for `reverse()` and `reversed()`.

## Tuples

A tuple is a list that cannot change. That makes it safe to use as a dict
key or a set member.

```
point = (3, 4)
print point[0]      ~ 3
print point.length  ~ 2

single = (7,)       ~ the trailing comma makes it a tuple
empty = ()
```

```
locations = {}
locations[(0, 0)] = "origin"
print locations[(0, 0)]   ~ origin
```

Trying to change one is an error rather than a silent copy:

```
point.add(5)   ~ Error: A tuple cannot be changed — use to_list() first
```

## Sets

A set holds each value once, in the order the values first arrived.

```
tags = {"blue", "red", "blue"}
print tags          ~ {"blue", "red"}
print tags.length   ~ 2
print tags.has("red")

tags.add("green")
tags.remove("blue")
```

The usual algebra is there, as methods or as operators:

```
a = {1, 2, 3}
b = {3, 4, 5}

print a.union(b)                 ~ {1, 2, 3, 4, 5}
print a.intersect(b)             ~ {3}
print a.difference(b)            ~ {1, 2}
print a.symmetric_difference(b)  ~ {1, 2, 4, 5}
print a + b                      ~ union
print a - b                      ~ difference

print a.is_subset({1, 2, 3, 4})  ~ true
print a.is_superset({1, 2})      ~ true
print a.is_disjoint({9})         ~ true
```

An empty set is `set()` — a bare `{}` is an empty dict.

## Dicts

Keys can be any value that does not change: strings, numbers, booleans,
`none`, and tuples.

```
person = {name = "Theo", age = 30}
print person["name"]
print person.name        ~ same thing, for string keys

scores = {}
scores[1] = "one"
scores[(1, 2)] = "a pair"
scores[true] = "yes"
```

```
print person.keys()      ~ ["name", "age"]
print person.values()    ~ ["Theo", 30]
print person.entries()   ~ [("name", "Theo"), ("age", 30)]

print person.get("name", "unknown")     ~ Theo
print person.get("email", "unknown")    ~ unknown
print person.has("age")                 ~ true
person.remove("age")

defaults = {theme = "dark"}
print defaults.merge({theme = "light", size = 12})  ~ a new dict
defaults.update({size = 12})                        ~ changes defaults

print person.map_values((v): v)
print person.filter((k, v): k == "name")
print person.invert()
```

Iterating a dict gives you its keys, in insertion order:

```
through person as key:
  print "{key} = {person[key]}"
```

## Deques, queues and stacks

All three hold a sequence; they differ in which end you are allowed to touch.

```
d = deque([2, 3])
d.push_front(1)   ~ deque[1, 2, 3]
d.push(4)         ~ deque[1, 2, 3, 4]
print d.pop_front()
print d.pop()
d.rotate(1)
```

```
q = queue()
q.push("a")
q.push("b")
print q.pop()     ~ a — whatever went in first comes out first
print q.peek()    ~ b
```

```
s = stack()
s.push(1)
s.push(2)
print s.pop()     ~ 2 — the most recent goes first
print s.peek()    ~ 1
```

## Heaps

A heap always hands back its smallest element, without keeping everything
sorted. Use `max_heap()` when you want the largest instead.

```
h = heap([5, 1, 3])
h.push(0)
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

Every ordered collection and every string can be sliced. A slice returns a
new value of the same kind.

```
nums = [0, 1, 2, 3, 4, 5]

print nums[1:3]    ~ [1, 2]
print nums[:2]     ~ [0, 1]
print nums[3:]     ~ [3, 4, 5]
print nums[-2:]    ~ [4, 5]
print nums[::2]    ~ [0, 2, 4]
print nums[::-1]   ~ [5, 4, 3, 2, 1]

s = "Hello, World!"
print s[0:5]       ~ Hello
print s[::-1]      ~ !dlroW ,olleH
```

The older `nums[1..4]` range form still works and means the same as
`nums[1:4]`.

## Converting between them

Every collection converts to every other:

```
xs = [3, 1, 2, 1]

print xs.to_set()      ~ {3, 1, 2}
print xs.to_tuple()    ~ (3, 1, 2, 1)
print xs.to_deque()
print xs.to_queue()
print xs.to_stack()
print xs.to_heap()
print {1, 2}.to_list() ~ [1, 2]
```

The constructors accept any collection, so `set(my_list)` and
`list(my_set)` work too.

## Shared methods

These read the same on every collection, and on strings where they make
sense:

```
length      is_empty     contains / has    index_of    count
first       last         join              map         filter
reduce      each         find              any         all
sum         min          max               sorted      reversed
unique      flatten      take              drop        chunk
zip         slice        to_list           copy
```

## Equality

Collections compare by their contents, not by identity:

```
print [1, 2] == [1, 2]           ~ true
print (1, 2) == (1, 2)           ~ true
print {1, 2} == {2, 1}           ~ true — order does not matter for sets
print {a = 1} == {a = 1}         ~ true
print [1, [2, 3]] == [1, [2, 3]] ~ true
```

Sorting a mixed list never fails: values of different types fall into a
stable order by type.
