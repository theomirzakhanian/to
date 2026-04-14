# Formatter

To includes a built-in code formatter that enforces a consistent style across
your codebase.

## Usage

```sh
to fmt file.to
```

The formatter overwrites the file in place. To format an entire project:

```sh
to fmt src/
```

## What It Does

The formatter applies the following rules:

- **Indentation** -- normalizes to 2 spaces per level.
- **Operator spacing** -- ensures spaces around `=`, `+`, `-`, `==`, `!=`, etc.
- **Blank lines** -- inserts a single blank line between top-level functions.
- **Trailing whitespace** -- removes trailing spaces from all lines.
- **Consistent commas** -- ensures a space after commas in argument lists.

### Before

```to
fn add(a,b)
        return a+b
end
fn multiply( a,b )
    return a *b
end
```

### After

```to
fn add(a, b)
  return a + b
end

fn multiply(a, b)
  return a * b
end
```

## VS Code Integration

To run the formatter on save, add this to your VS Code `settings.json`:

```json
{
  "editor.formatOnSave": true,
  "[to]": {
    "editor.defaultFormatter": "to-lang.to"
  }
}
```

This requires the To extension for VS Code.

## CI Usage

To check formatting without overwriting files, use the `--check` flag:

```sh
to fmt --check src/
```

This exits with a non-zero code if any file would be changed, making it suitable
for CI pipelines.
