#include "error.h"

ToError::ToError(const std::string& filename, int line, int column, const std::string& message,
                 const std::string& hint)
    : std::runtime_error(message), filename(filename), line(line), column(column),
      detail(message), hint(hint) {}

std::string ToError::format() const {
    std::string result;
    if (!stackTrace.empty()) {
        result += stackTrace;
    }
    result += filename + ":" + std::to_string(line);
    if (column > 0) result += ":" + std::to_string(column);
    result += ": error: " + detail;
    if (!hint.empty()) {
        result += "\n  hint: " + hint;
    }
    return result;
}

ToRuntimeError::ToRuntimeError(const std::string& message, int line, int column)
    : ToError("<runtime>", line, column, message) {}
