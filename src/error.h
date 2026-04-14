#pragma once
#include <string>
#include <stdexcept>

class ToError : public std::runtime_error {
public:
    std::string filename;
    int line;
    int column;
    std::string detail;
    std::string hint;
    std::string stackTrace;

    ToError(const std::string& filename, int line, int column, const std::string& message,
            const std::string& hint = "");
    std::string format() const;
};

class ToRuntimeError : public ToError {
public:
    ToRuntimeError(const std::string& message, int line = 0, int column = 0);
};

// Special signal types for control flow
class ReturnSignal : public std::exception {
public:
    struct Value;
};

class BreakSignal : public std::exception {};
class ContinueSignal : public std::exception {};
