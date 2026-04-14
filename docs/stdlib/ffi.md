# ffi (C Foreign Function Interface)

```
use ffi
```

Call any C library from To.

## Functions

| Function | Description |
|----------|-------------|
| `ffi.open(path)` | Load a shared library, returns a library object |
| `ffi.sizeof(type)` | Size of a C type in bytes |
| `ffi.nullptr` | Null pointer constant (0) |

## Library Object Methods

| Method | Description |
|--------|-------------|
| `lib.call(name, args, returnType)` | Call a C function (types auto-inferred) |
| `lib.call(name, args, argTypes, returnType)` | Call with explicit types |
| `lib.has(name)` | Check if a symbol exists |
| `lib.symbol(name)` | Get raw function pointer |
| `lib.close()` | Close the library |

## C Types

`"int"`, `"long"`, `"float"`, `"double"`, `"string"` (char\*), `"pointer"` (void\*), `"void"`, `"bool"`

## Example

```
use ffi

~ Load math library
libm = ffi.open("libm")
print libm.call("sqrt", [16.0], "double")     ~ 4.0
print libm.call("pow", [2.0, 10.0], "double")  ~ 1024.0

~ Load C standard library
libc = ffi.open("libc")
print libc.call("strlen", ["hello"], "int")     ~ 5
print libc.call("atoi", ["42"], "int")          ~ 42

~ Check symbols
print libm.has("sqrt")           ~ true
print libm.has("nonexistent")    ~ false

~ Type sizes
print ffi.sizeof("int")       ~ 4
print ffi.sizeof("pointer")   ~ 8

libm.close()
```

## Notes

- Library names are auto-resolved: `"libm"` tries `libm.dylib`, `libm.so`, etc.
- Argument types are auto-inferred from To values when not specified
- Supports up to 6 arguments per call
