#pragma once
#include "environment.h"
#include <string>
#include <dlfcn.h>

// Represents a loaded shared library
struct ToLibrary {
    void* handle;
    std::string path;

    ToLibrary(void* h, const std::string& p) : handle(h), path(p) {}
    ~ToLibrary() { if (handle) dlclose(handle); }
};

// Register the ffi module
void registerFFIModule(EnvPtr env);
