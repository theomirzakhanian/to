cmake_minimum_required(VERSION 3.16)
project(to LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(to
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/interpreter.cpp
    src/environment.cpp
    src/codegen.cpp
    src/error.cpp
    src/builtins.cpp
    src/http.cpp
    src/ffi.cpp
    src/to_stdlib.cpp
    src/pkg.cpp
    src/formatter.cpp
)

target_include_directories(to PRIVATE src include)
