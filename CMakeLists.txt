cmake_minimum_required(VERSION 3.16)
project(to LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(NOT MSVC)
    add_compile_options(-Wall -Wextra)
endif()

add_executable(to
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/interpreter.cpp
    src/environment.cpp
    src/value.cpp
    src/methods.cpp
    src/codegen.cpp
    src/error.cpp
    src/builtins.cpp
    src/http.cpp
    src/ffi.cpp
    src/to_stdlib.cpp
    src/pkg.cpp
    src/formatter.cpp
    src/vm.cpp
)

target_include_directories(to PRIVATE src include)
