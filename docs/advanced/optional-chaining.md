# Optional Chaining

`?.` safely accesses properties that might be `none`. If anything in the chain is `none`, the whole expression returns `none` instead of crashing.

## The problem it solves

```
~ Without optional chaining — crashes if user is none
city = user.address.city

~ With ?. — returns none safely
city = user?.address?.city
```

## Basic usage

```
user = {name = "Theo", address = {city = "LA"}}
print user?.name              ~ Theo
print user?.address?.city     ~ LA

missing = none
print missing?.name           ~ none  (doesn't crash)
```

## Chaining through function calls

```
to find_user(id):
  if id == 1:
    return {name = "Theo"}
  return none

print find_user(1)?.name   ~ Theo
print find_user(99)?.name  ~ none
```

## Chaining through methods

```
name = "Hello"
print name?.upper()   ~ HELLO

empty = none
print empty?.upper()  ~ none  (doesn't crash)
```

## When to use it

Use `?.` whenever an intermediate value might legitimately be `none` — API responses, optional fields, lookups that might fail. It replaces noisy `if x != none` checks with cleaner code.
