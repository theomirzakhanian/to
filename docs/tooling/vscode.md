# VS Code Extension

Full editor support for To in Visual Studio Code.

## Install

```bash
cp -r to-vscode ~/.vscode/extensions/to-language-0.2.0
```

Restart VS Code (Cmd+Shift+P > "Reload Window").

## Features

### Syntax Highlighting
- Keywords, strings, numbers, comments
- String interpolation inside `{}`
- Function and class name highlighting
- Type hint coloring

### Real-time Error Checking
- Red squiggles on syntax errors as you type
- Error messages shown on hover
- Powered by `to check` running in the background

### Autocomplete (Ctrl+Space)
- All keywords and built-in functions
- After `use ` — module suggestions
- After `.` — method suggestions for lists, strings, dicts
- After `:` — type hint suggestions
- After `->` — return type suggestions
- Your own functions, classes, and variables

### Hover Documentation
- Hover any keyword for docs and examples
- Hover built-in functions for signatures
- Hover your own functions for parameter info

### Go to Definition (Cmd+Click / F12)
- Jump to function definitions
- Jump to class definitions
- Jump to variable assignments

### Document Outline (Cmd+Shift+O)
- Lists all functions and classes in the sidebar

## Configuration

In VS Code settings:

| Setting | Default | Description |
|---------|---------|-------------|
| `to.executablePath` | `"to"` | Path to the `to` binary |
| `to.enableDiagnostics` | `true` | Enable real-time error checking |
