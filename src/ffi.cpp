#include "ffi.h"
#include "error.h"
#ifndef __EMSCRIPTEN__
#include <dlfcn.h>
#include <iostream>
#include <sstream>
#include <cstring>

// ========================
// FFI Type System
// ========================
// Supported C types for FFI calls:
//   "void"    - no return value
//   "int"     - int / long
//   "double"  - double
//   "float"   - float
//   "string"  - const char* (returned as To string)
//   "pointer" - void* (returned as int address)
//   "bool"    - int treated as boolean

// Convert a To value to a C argument based on type hint
union CValue {
    long long intVal;
    double doubleVal;
    float floatVal;
    const char* strVal;
    void* ptrVal;
};

static CValue toValueToCArg(ToValuePtr val, const std::string& type) {
    CValue c;
    memset(&c, 0, sizeof(c));

    if (type == "int" || type == "long") {
        if (val->type == ToValue::Type::INT) c.intVal = val->intVal;
        else if (val->type == ToValue::Type::FLOAT) c.intVal = (long long)val->floatVal;
        else if (val->type == ToValue::Type::BOOL) c.intVal = val->boolVal ? 1 : 0;
        else throw ToRuntimeError("Cannot convert " + val->typeName() + " to C int");
    } else if (type == "double") {
        if (val->type == ToValue::Type::FLOAT) c.doubleVal = val->floatVal;
        else if (val->type == ToValue::Type::INT) c.doubleVal = (double)val->intVal;
        else throw ToRuntimeError("Cannot convert " + val->typeName() + " to C double");
    } else if (type == "float") {
        if (val->type == ToValue::Type::FLOAT) c.floatVal = (float)val->floatVal;
        else if (val->type == ToValue::Type::INT) c.floatVal = (float)val->intVal;
        else throw ToRuntimeError("Cannot convert " + val->typeName() + " to C float");
    } else if (type == "string") {
        if (val->type == ToValue::Type::STRING) c.strVal = val->strVal.c_str();
        else throw ToRuntimeError("Cannot convert " + val->typeName() + " to C string");
    } else if (type == "pointer") {
        if (val->type == ToValue::Type::INT) c.ptrVal = (void*)val->intVal;
        else if (val->type == ToValue::Type::NONE) c.ptrVal = nullptr;
        else throw ToRuntimeError("Cannot convert " + val->typeName() + " to C pointer");
    } else {
        throw ToRuntimeError("Unknown C type: " + type);
    }
    return c;
}

// ========================
// Dynamic function calling
// ========================
// We support up to 6 arguments with common type combinations.
// This uses a dispatch table approach since we can't use libffi.

static ToValuePtr callCFunction(void* funcPtr, const std::vector<ToValuePtr>& args,
                                 const std::vector<std::string>& argTypes,
                                 const std::string& returnType) {
    if (args.size() != argTypes.size()) {
        throw ToRuntimeError("Argument count mismatch: expected " +
            std::to_string(argTypes.size()) + " types but got " +
            std::to_string(args.size()) + " values");
    }

    // Convert arguments
    std::vector<CValue> cArgs;
    for (size_t i = 0; i < args.size(); i++) {
        cArgs.push_back(toValueToCArg(args[i], argTypes[i]));
    }

    // Dispatch based on return type and argument patterns
    // This is the core challenge without libffi — we enumerate common patterns

    size_t n = cArgs.size();

    if (returnType == "void") {
        if (n == 0) {
            ((void(*)())funcPtr)();
        } else if (n == 1 && argTypes[0] == "int") {
            ((void(*)(long long))funcPtr)(cArgs[0].intVal);
        } else if (n == 1 && argTypes[0] == "pointer") {
            ((void(*)(void*))funcPtr)(cArgs[0].ptrVal);
        } else if (n == 1 && argTypes[0] == "string") {
            ((void(*)(const char*))funcPtr)(cArgs[0].strVal);
        } else if (n == 2 && argTypes[0] == "pointer" && argTypes[1] == "int") {
            ((void(*)(void*, long long))funcPtr)(cArgs[0].ptrVal, cArgs[1].intVal);
        } else {
            throw ToRuntimeError("Unsupported void function signature with " + std::to_string(n) + " args");
        }
        return ToValue::makeNone();
    }

    if (returnType == "int" || returnType == "long") {
        long long result = 0;
        if (n == 0) {
            result = ((long long(*)())funcPtr)();
        } else if (n == 1 && argTypes[0] == "int") {
            result = ((long long(*)(long long))funcPtr)(cArgs[0].intVal);
        } else if (n == 1 && argTypes[0] == "double") {
            result = ((long long(*)(double))funcPtr)(cArgs[0].doubleVal);
        } else if (n == 1 && argTypes[0] == "string") {
            result = ((long long(*)(const char*))funcPtr)(cArgs[0].strVal);
        } else if (n == 1 && argTypes[0] == "pointer") {
            result = ((long long(*)(void*))funcPtr)(cArgs[0].ptrVal);
        } else if (n == 2 && argTypes[0] == "int" && argTypes[1] == "int") {
            result = ((long long(*)(long long, long long))funcPtr)(cArgs[0].intVal, cArgs[1].intVal);
        } else if (n == 2 && argTypes[0] == "string" && argTypes[1] == "int") {
            result = ((long long(*)(const char*, long long))funcPtr)(cArgs[0].strVal, cArgs[1].intVal);
        } else if (n == 2 && argTypes[0] == "string" && argTypes[1] == "string") {
            result = ((long long(*)(const char*, const char*))funcPtr)(cArgs[0].strVal, cArgs[1].strVal);
        } else if (n == 2 && argTypes[0] == "pointer" && argTypes[1] == "string") {
            result = ((long long(*)(void*, const char*))funcPtr)(cArgs[0].ptrVal, cArgs[1].strVal);
        } else if (n == 2 && argTypes[0] == "pointer" && argTypes[1] == "pointer") {
            result = ((long long(*)(void*, void*))funcPtr)(cArgs[0].ptrVal, cArgs[1].ptrVal);
        } else if (n == 3 && argTypes[0] == "string" && argTypes[1] == "string" && argTypes[2] == "int") {
            result = ((long long(*)(const char*, const char*, long long))funcPtr)(cArgs[0].strVal, cArgs[1].strVal, cArgs[2].intVal);
        } else if (n == 3 && argTypes[0] == "pointer" && argTypes[1] == "pointer" && argTypes[2] == "int") {
            result = ((long long(*)(void*, void*, long long))funcPtr)(cArgs[0].ptrVal, cArgs[1].ptrVal, cArgs[2].intVal);
        } else {
            throw ToRuntimeError("Unsupported int function signature");
        }
        return ToValue::makeInt(result);
    }

    if (returnType == "double") {
        double result = 0;
        if (n == 0) {
            result = ((double(*)())funcPtr)();
        } else if (n == 1 && argTypes[0] == "double") {
            result = ((double(*)(double))funcPtr)(cArgs[0].doubleVal);
        } else if (n == 1 && argTypes[0] == "int") {
            result = ((double(*)(long long))funcPtr)(cArgs[0].intVal);
        } else if (n == 1 && argTypes[0] == "string") {
            result = ((double(*)(const char*))funcPtr)(cArgs[0].strVal);
        } else if (n == 1 && argTypes[0] == "pointer") {
            result = ((double(*)(void*))funcPtr)(cArgs[0].ptrVal);
        } else if (n == 2 && argTypes[0] == "double" && argTypes[1] == "double") {
            result = ((double(*)(double, double))funcPtr)(cArgs[0].doubleVal, cArgs[1].doubleVal);
        } else if (n == 2 && argTypes[0] == "int" && argTypes[1] == "int") {
            result = ((double(*)(long long, long long))funcPtr)(cArgs[0].intVal, cArgs[1].intVal);
        } else {
            throw ToRuntimeError("Unsupported double function signature");
        }
        return ToValue::makeFloat(result);
    }

    if (returnType == "float") {
        float result = 0;
        if (n == 0) {
            result = ((float(*)())funcPtr)();
        } else if (n == 1 && argTypes[0] == "float") {
            result = ((float(*)(float))funcPtr)(cArgs[0].floatVal);
        } else if (n == 1 && argTypes[0] == "double") {
            result = ((float(*)(double))funcPtr)(cArgs[0].doubleVal);
        } else if (n == 2 && argTypes[0] == "float" && argTypes[1] == "float") {
            result = ((float(*)(float, float))funcPtr)(cArgs[0].floatVal, cArgs[1].floatVal);
        } else {
            throw ToRuntimeError("Unsupported float function signature");
        }
        return ToValue::makeFloat((double)result);
    }

    if (returnType == "string") {
        const char* result = nullptr;
        if (n == 0) {
            result = ((const char*(*)())funcPtr)();
        } else if (n == 1 && argTypes[0] == "string") {
            result = ((const char*(*)(const char*))funcPtr)(cArgs[0].strVal);
        } else if (n == 1 && argTypes[0] == "int") {
            result = ((const char*(*)(long long))funcPtr)(cArgs[0].intVal);
        } else if (n == 1 && argTypes[0] == "pointer") {
            result = ((const char*(*)(void*))funcPtr)(cArgs[0].ptrVal);
        } else if (n == 2 && argTypes[0] == "string" && argTypes[1] == "string") {
            result = ((const char*(*)(const char*, const char*))funcPtr)(cArgs[0].strVal, cArgs[1].strVal);
        } else if (n == 2 && argTypes[0] == "pointer" && argTypes[1] == "int") {
            result = ((const char*(*)(void*, long long))funcPtr)(cArgs[0].ptrVal, cArgs[1].intVal);
        } else {
            throw ToRuntimeError("Unsupported string function signature");
        }
        if (result == nullptr) return ToValue::makeNone();
        return ToValue::makeString(result);
    }

    if (returnType == "pointer") {
        void* result = nullptr;
        if (n == 0) {
            result = ((void*(*)())funcPtr)();
        } else if (n == 1 && argTypes[0] == "string") {
            result = ((void*(*)(const char*))funcPtr)(cArgs[0].strVal);
        } else if (n == 1 && argTypes[0] == "int") {
            result = ((void*(*)(long long))funcPtr)(cArgs[0].intVal);
        } else if (n == 1 && argTypes[0] == "pointer") {
            result = ((void*(*)(void*))funcPtr)(cArgs[0].ptrVal);
        } else if (n == 2 && argTypes[0] == "pointer" && argTypes[1] == "int") {
            result = ((void*(*)(void*, long long))funcPtr)(cArgs[0].ptrVal, cArgs[1].intVal);
        } else if (n == 2 && argTypes[0] == "pointer" && argTypes[1] == "string") {
            result = ((void*(*)(void*, const char*))funcPtr)(cArgs[0].ptrVal, cArgs[1].strVal);
        } else if (n == 2 && argTypes[0] == "pointer" && argTypes[1] == "pointer") {
            result = ((void*(*)(void*, void*))funcPtr)(cArgs[0].ptrVal, cArgs[1].ptrVal);
        } else if (n == 2 && argTypes[0] == "string" && argTypes[1] == "string") {
            result = ((void*(*)(const char*, const char*))funcPtr)(cArgs[0].strVal, cArgs[1].strVal);
        } else if (n == 3 && argTypes[0] == "pointer" && argTypes[1] == "pointer" && argTypes[2] == "int") {
            result = ((void*(*)(void*, void*, long long))funcPtr)(cArgs[0].ptrVal, cArgs[1].ptrVal, cArgs[2].intVal);
        } else if (n == 3 && argTypes[0] == "pointer" && argTypes[1] == "int" && argTypes[2] == "int") {
            result = ((void*(*)(void*, long long, long long))funcPtr)(cArgs[0].ptrVal, cArgs[1].intVal, cArgs[2].intVal);
        } else if (n == 3 && argTypes[0] == "string" && argTypes[1] == "string" && argTypes[2] == "pointer") {
            result = ((void*(*)(const char*, const char*, void*))funcPtr)(cArgs[0].strVal, cArgs[1].strVal, cArgs[2].ptrVal);
        } else {
            throw ToRuntimeError("Unsupported pointer function signature");
        }
        if (result == nullptr) return ToValue::makeInt(0);
        return ToValue::makeInt((long long)result);
    }

    if (returnType == "bool") {
        auto intResult = callCFunction(funcPtr, args, argTypes, "int");
        return ToValue::makeBool(intResult->intVal != 0);
    }

    throw ToRuntimeError("Unknown return type: " + returnType);
}

// ========================
// Library object methods
// ========================

// Create a To library object (dict with bound methods)
static ToValuePtr makeLibraryObject(std::shared_ptr<ToLibrary> lib) {
    auto obj = ToValue::makeDict({});

    // lib.call(funcName, args, argTypes, returnType)
    // Example: lib.call("sqrt", [16.0], ["double"], "double")
    obj->dictVal.push_back({"call", ToValue::makeBuiltin(
        [lib](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() < 3 || args.size() > 4)
                throw ToRuntimeError("lib.call() takes 3-4 arguments: (name, args, argTypes, returnType) or (name, args, returnType)");

            if (args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("lib.call() function name must be a string");

            std::string funcName = args[0]->strVal;

            // Look up the function
            void* funcPtr = dlsym(lib->handle, funcName.c_str());
            if (!funcPtr) {
                throw ToRuntimeError("Function '" + funcName + "' not found in " + lib->path +
                    ": " + std::string(dlerror()));
            }

            std::vector<ToValuePtr> callArgs;
            std::vector<std::string> argTypes;
            std::string returnType;

            if (args.size() == 4) {
                // call(name, args, argTypes, returnType)
                if (args[1]->type != ToValue::Type::LIST)
                    throw ToRuntimeError("lib.call() args must be a list");
                if (args[2]->type != ToValue::Type::LIST)
                    throw ToRuntimeError("lib.call() argTypes must be a list");
                if (args[3]->type != ToValue::Type::STRING)
                    throw ToRuntimeError("lib.call() returnType must be a string");

                callArgs = args[1]->listVal;
                for (auto& t : args[2]->listVal) {
                    if (t->type != ToValue::Type::STRING)
                        throw ToRuntimeError("argTypes must be strings");
                    argTypes.push_back(t->strVal);
                }
                returnType = args[3]->strVal;
            } else {
                // call(name, args, returnType) — auto-infer arg types
                if (args[1]->type != ToValue::Type::LIST)
                    throw ToRuntimeError("lib.call() args must be a list");
                if (args[2]->type != ToValue::Type::STRING)
                    throw ToRuntimeError("lib.call() returnType must be a string");

                callArgs = args[1]->listVal;
                returnType = args[2]->strVal;

                // Auto-infer types from To values
                for (auto& val : callArgs) {
                    switch (val->type) {
                        case ToValue::Type::INT: argTypes.push_back("int"); break;
                        case ToValue::Type::FLOAT: argTypes.push_back("double"); break;
                        case ToValue::Type::STRING: argTypes.push_back("string"); break;
                        case ToValue::Type::BOOL: argTypes.push_back("int"); break;
                        case ToValue::Type::NONE: argTypes.push_back("pointer"); break;
                        default: throw ToRuntimeError("Cannot auto-infer C type for " + val->typeName());
                    }
                }
            }

            return callCFunction(funcPtr, callArgs, argTypes, returnType);
        }
    )});

    // lib.symbol(name) — get a raw function pointer as int
    obj->dictVal.push_back({"symbol", ToValue::makeBuiltin(
        [lib](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("lib.symbol() takes 1 string argument");
            void* sym = dlsym(lib->handle, args[0]->strVal.c_str());
            if (!sym) throw ToRuntimeError("Symbol not found: " + args[0]->strVal);
            return ToValue::makeInt((long long)sym);
        }
    )});

    // lib.has(name) — check if a symbol exists
    obj->dictVal.push_back({"has", ToValue::makeBuiltin(
        [lib](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("lib.has() takes 1 string argument");
            dlerror(); // clear any old errors
            void* sym = dlsym(lib->handle, args[0]->strVal.c_str());
            return ToValue::makeBool(sym != nullptr);
        }
    )});

    // lib.close() — close the library
    obj->dictVal.push_back({"close", ToValue::makeBuiltin(
        [lib](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (lib->handle) {
                dlclose(lib->handle);
                lib->handle = nullptr;
            }
            return ToValue::makeNone();
        }
    )});

    // lib.path — the library path
    obj->dictVal.push_back({"path", ToValue::makeString(lib->path)});

    return obj;
}

// ========================
// FFI Module Registration
// ========================

void registerFFIModule(EnvPtr env) {
    auto ffiModule = ToValue::makeDict({});

    // ffi.open(path) — load a shared library
    ffiModule->dictVal.push_back({"open", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("ffi.open() takes 1 string argument (library path)");

            std::string path = args[0]->strVal;

            // Try the path as-is, then with common library extensions
            void* handle = dlopen(path.c_str(), RTLD_LAZY);

            if (!handle) {
                // Try with .dylib extension (macOS)
                handle = dlopen((path + ".dylib").c_str(), RTLD_LAZY);
            }
            if (!handle) {
                // Try with .so extension (Linux)
                handle = dlopen((path + ".so").c_str(), RTLD_LAZY);
            }
            if (!handle) {
                // Try as a framework or lib prefix
                handle = dlopen(("lib" + path + ".dylib").c_str(), RTLD_LAZY);
            }
            if (!handle) {
                handle = dlopen(("lib" + path + ".so").c_str(), RTLD_LAZY);
            }
            if (!handle) {
                throw ToRuntimeError("Could not load library '" + path + "': " + std::string(dlerror()));
            }

            auto lib = std::make_shared<ToLibrary>(handle, path);
            return makeLibraryObject(lib);
        }
    )});

    // ffi.sizeof(type) — get size of a C type
    ffiModule->dictVal.push_back({"sizeof", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("ffi.sizeof() takes 1 string argument");
            std::string type = args[0]->strVal;
            if (type == "int") return ToValue::makeInt(sizeof(int));
            if (type == "long") return ToValue::makeInt(sizeof(long));
            if (type == "long long") return ToValue::makeInt(sizeof(long long));
            if (type == "float") return ToValue::makeInt(sizeof(float));
            if (type == "double") return ToValue::makeInt(sizeof(double));
            if (type == "pointer" || type == "ptr") return ToValue::makeInt(sizeof(void*));
            if (type == "char") return ToValue::makeInt(sizeof(char));
            throw ToRuntimeError("Unknown type: " + type);
        }
    )});

    // ffi.nullptr — null pointer constant
    ffiModule->dictVal.push_back({"nullptr", ToValue::makeInt(0)});

    env->define("ffi", ffiModule);
}
#else
// WASM stub
void registerFFIModule(EnvPtr env) {
    auto ffiModule = ToValue::makeDict({});
    ffiModule->dictVal.push_back({"open", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr>) -> ToValuePtr {
            throw ToRuntimeError("FFI is not available in the browser");
        }
    )});
    env->define("ffi", ffiModule);
}
#endif
