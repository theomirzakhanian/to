# regex

```
use regex
```

## Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `regex.match(pattern, text)` | list or none | First match with capture groups |
| `regex.find_all(pattern, text)` | list | All matches |
| `regex.replace(pattern, replacement, text)` | string | Replace all matches |
| `regex.split(pattern, text)` | list | Split by pattern |
| `regex.test(pattern, text)` | bool | Check if pattern matches |

## Example

```
use regex

~ Test if a string matches
regex.test("\\d+", "abc123")           ~ true

~ Find first match with groups
regex.match("(\\d+)-(\\d+)", "555-1234")  ~ ["555-1234", "555", "1234"]

~ Find all matches
regex.find_all("[a-z]+@[a-z.]+", "email me at a@b.com or c@d.org")
~ ["a@b.com", "c@d.org"]

~ Replace
regex.replace("\\s+", " ", "too   many   spaces")  ~ "too many spaces"

~ Split
regex.split("[,;]", "a,b;c,d")  ~ ["a", "b", "c", "d"]
```
