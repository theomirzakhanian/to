# Multi-line Strings

To supports multi-line strings using triple-quote syntax. These strings preserve
line breaks, support interpolation, and auto-dedent based on common leading
whitespace.

## Basic Syntax

```to
let message = """
    Hello,
    welcome to To.
    """
```

The result is `"Hello,\nwelcome to To."` -- leading whitespace shared by all
lines is stripped automatically.

## Interpolation

Multi-line strings support `{var}` interpolation, just like regular strings.

```to
let name = "world"
let lang = "To"

let greeting = """
    Hello, {name}!
    You are using {lang}.
    """
```

## HTML Templates

```to
fn render_page(title, body)
    return """
        <html>
        <head><title>{title}</title></head>
        <body>
            {body}
        </body>
        </html>
        """
end
```

## SQL Queries

```to
let table = "users"
let min_age = 18

let query = """
    SELECT id, name, email
    FROM {table}
    WHERE age >= {min_age}
    ORDER BY name ASC
    """
```

## Multi-line Messages

```to
fn usage()
    print("""
        Usage: myapp <command> [options]

        Commands:
            run     Run the application
            test    Run the test suite
            build   Build for production
        """)
end
```

## Notes

- The closing `"""` determines the base indentation level. All common leading
  whitespace is removed relative to it.
- Interpolation uses the same `{expr}` syntax as single-line strings.
- Triple-quoted strings can contain regular `"` characters without escaping.
