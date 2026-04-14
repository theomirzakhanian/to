# json

```
use json
```

## Functions

| Function | Description |
|----------|-------------|
| `json.stringify(value)` | Convert any To value to a JSON string |
| `json.parse(string)` | Parse a JSON string into To values |

## Example

```
use json

~ Serialize
data = {name = "Theo", scores = [95, 87, 92]}
text = json.stringify(data)
print text   ~ {"name": "Theo", "scores": [95, 87, 92]}

~ Parse
parsed = json.parse('{"active": true, "count": 42}')
print parsed.active   ~ true
print parsed.count    ~ 42
```

## Type Mapping

| JSON | To |
|------|-----|
| `"string"` | string |
| `42` | int |
| `3.14` | float |
| `true`/`false` | bool |
| `null` | none |
| `[...]` | list |
| `{...}` | dict |
