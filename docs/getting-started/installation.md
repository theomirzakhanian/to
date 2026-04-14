# Installation

## Requirements

- A C++ compiler (GCC, Clang, or MSVC) with C++17 support
- CMake 3.16 or later

## Build from Source

```bash
git clone https://github.com/theomirzakhanian/to.git
cd to
mkdir build && cd build
cmake .. && cmake --build .
```

## Add to PATH

**macOS / Linux:**
```bash
sudo cp build/to /usr/local/bin/to
```

Or add to your local bin:
```bash
mkdir -p ~/.local/bin
cp build/to ~/.local/bin/to
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

## Verify

```bash
to version
```

You should see:
```
To v0.2.0
```

## VS Code Extension

For syntax highlighting and editor support:

```bash
cp -r to-vscode ~/.vscode/extensions/to-language-0.2.0
```

Restart VS Code, and `.to` files will have full language support.
