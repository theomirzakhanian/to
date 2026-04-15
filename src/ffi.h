#pragma once
#include "environment.h"
#include <string>

#ifndef __EMSCRIPTEN__
#include <dlfcn.h>

struct ToLibrary {
    void* handle;
    std::string path;
    ToLibrary(void* h, const std::string& p) : handle(h), path(p) {}
    ~ToLibrary() { if (handle) dlclose(handle); }
};
#endif

void registerFFIModule(EnvPtr env);
