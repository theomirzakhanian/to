# fs (filesystem)

```
use fs
```

## Functions

| Function | Description |
|----------|-------------|
| `fs.read(path)` | Read entire file as string |
| `fs.write(path, content)` | Write string to file |
| `fs.append(path, content)` | Append to file |
| `fs.exists(path)` | Check if path exists |
| `fs.is_file(path)` | Check if path is a file |
| `fs.is_dir(path)` | Check if path is a directory |
| `fs.list(path)` | List directory contents |
| `fs.walk(path)` | Recursively list all files |
| `fs.mkdir(path)` | Create directory (and parents) |
| `fs.remove(path)` | Delete a file |
| `fs.copy(src, dest)` | Copy a file |
| `fs.rename(old, new)` | Rename/move a file |
| `fs.size(path)` | File size in bytes |
| `fs.ext(path)` | File extension (`.txt`) |
| `fs.basename(path)` | Filename without directory |
| `fs.dirname(path)` | Directory without filename |
| `fs.cwd()` | Current working directory |

## Example

```
use fs

~ Read and write files
fs.write("output.txt", "Hello!")
content = fs.read("output.txt")
print content   ~ Hello!

~ Check files
if fs.exists("config.json"):
  config = fs.read("config.json")

~ List directory
through fs.list(".") as file:
  print "{file} ({fs.size(file)} bytes)"

~ Walk recursively
through fs.walk("src") as path:
  if fs.ext(path) == ".to":
    print "Found: {path}"
```
