#include "builtins.h"
#include "error.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <fstream>

void registerBuiltins(EnvPtr env) {
    // input(prompt) — read a line from stdin
    env->define("input", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
        if (!args.empty()) {
            std::cout << args[0]->toString();
        }
        std::string line;
        std::getline(std::cin, line);
        return ToValue::makeString(line);
    }));

    // len(x) — length of string/list/dict
    env->define("len", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
        if (args.size() != 1) throw ToRuntimeError("len() takes exactly 1 argument");
        auto& v = args[0];
        switch (v->type) {
            case ToValue::Type::STRING: return ToValue::makeInt(v->strVal.size());
            case ToValue::Type::LIST: return ToValue::makeInt(v->listVal.size());
            case ToValue::Type::DICT: return ToValue::makeInt(v->dictVal.size());
            default: throw ToRuntimeError("len() not supported for type " + v->typeName());
        }
    }));

    // type(x) — return type name as string
    env->define("type", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
        if (args.size() != 1) throw ToRuntimeError("type() takes exactly 1 argument");
        return ToValue::makeString(args[0]->typeName());
    }));

    // int(x) — convert to int
    env->define("int", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
        if (args.size() != 1) throw ToRuntimeError("int() takes exactly 1 argument");
        auto& v = args[0];
        switch (v->type) {
            case ToValue::Type::INT: return v;
            case ToValue::Type::FLOAT: return ToValue::makeInt(static_cast<int64_t>(v->floatVal));
            case ToValue::Type::STRING: {
                try { return ToValue::makeInt(std::stoll(v->strVal)); }
                catch (...) { throw ToRuntimeError("Cannot convert '" + v->strVal + "' to int"); }
            }
            case ToValue::Type::BOOL: return ToValue::makeInt(v->boolVal ? 1 : 0);
            default: throw ToRuntimeError("Cannot convert " + v->typeName() + " to int");
        }
    }));

    // float(x) — convert to float
    env->define("float", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
        if (args.size() != 1) throw ToRuntimeError("float() takes exactly 1 argument");
        auto& v = args[0];
        switch (v->type) {
            case ToValue::Type::FLOAT: return v;
            case ToValue::Type::INT: return ToValue::makeFloat(static_cast<double>(v->intVal));
            case ToValue::Type::STRING: {
                try { return ToValue::makeFloat(std::stod(v->strVal)); }
                catch (...) { throw ToRuntimeError("Cannot convert '" + v->strVal + "' to float"); }
            }
            default: throw ToRuntimeError("Cannot convert " + v->typeName() + " to float");
        }
    }));

    // str(x) — convert to string
    env->define("str", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
        if (args.size() != 1) throw ToRuntimeError("str() takes exactly 1 argument");
        return ToValue::makeString(args[0]->toString());
    }));

    // range(start, end) or range(end)
    env->define("range", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
        int64_t start = 0, end = 0;
        if (args.size() == 1) {
            if (args[0]->type != ToValue::Type::INT) throw ToRuntimeError("range() requires int arguments");
            end = args[0]->intVal;
        } else if (args.size() == 2) {
            if (args[0]->type != ToValue::Type::INT || args[1]->type != ToValue::Type::INT)
                throw ToRuntimeError("range() requires int arguments");
            start = args[0]->intVal;
            end = args[1]->intVal;
        } else {
            throw ToRuntimeError("range() takes 1 or 2 arguments");
        }
        std::vector<ToValuePtr> list;
        for (int64_t i = start; i < end; i++) {
            list.push_back(ToValue::makeInt(i));
        }
        return ToValue::makeList(std::move(list));
    }));

    // abs(x)
    env->define("abs", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
        if (args.size() != 1) throw ToRuntimeError("abs() takes exactly 1 argument");
        if (args[0]->type == ToValue::Type::INT) return ToValue::makeInt(std::abs(args[0]->intVal));
        if (args[0]->type == ToValue::Type::FLOAT) return ToValue::makeFloat(std::fabs(args[0]->floatVal));
        throw ToRuntimeError("abs() requires a number");
    }));

    // min(a, b) or min(list)
    env->define("min", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
        if (args.size() == 1 && args[0]->type == ToValue::Type::LIST) {
            auto& list = args[0]->listVal;
            if (list.empty()) throw ToRuntimeError("min() of empty list");
            auto result = list[0];
            for (size_t i = 1; i < list.size(); i++) {
                if (list[i]->type == ToValue::Type::INT && result->type == ToValue::Type::INT) {
                    if (list[i]->intVal < result->intVal) result = list[i];
                } else if (list[i]->type == ToValue::Type::FLOAT || result->type == ToValue::Type::FLOAT) {
                    double a = result->type == ToValue::Type::INT ? (double)result->intVal : result->floatVal;
                    double b = list[i]->type == ToValue::Type::INT ? (double)list[i]->intVal : list[i]->floatVal;
                    if (b < a) result = list[i];
                }
            }
            return result;
        }
        if (args.size() == 2) {
            double a = args[0]->type == ToValue::Type::INT ? (double)args[0]->intVal : args[0]->floatVal;
            double b = args[1]->type == ToValue::Type::INT ? (double)args[1]->intVal : args[1]->floatVal;
            return a <= b ? args[0] : args[1];
        }
        throw ToRuntimeError("min() requires 1 list or 2 arguments");
    }));

    // max(a, b) or max(list)
    env->define("max", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
        if (args.size() == 1 && args[0]->type == ToValue::Type::LIST) {
            auto& list = args[0]->listVal;
            if (list.empty()) throw ToRuntimeError("max() of empty list");
            auto result = list[0];
            for (size_t i = 1; i < list.size(); i++) {
                if (list[i]->type == ToValue::Type::INT && result->type == ToValue::Type::INT) {
                    if (list[i]->intVal > result->intVal) result = list[i];
                } else if (list[i]->type == ToValue::Type::FLOAT || result->type == ToValue::Type::FLOAT) {
                    double a = result->type == ToValue::Type::INT ? (double)result->intVal : result->floatVal;
                    double b = list[i]->type == ToValue::Type::INT ? (double)list[i]->intVal : list[i]->floatVal;
                    if (b > a) result = list[i];
                }
            }
            return result;
        }
        if (args.size() == 2) {
            double a = args[0]->type == ToValue::Type::INT ? (double)args[0]->intVal : args[0]->floatVal;
            double b = args[1]->type == ToValue::Type::INT ? (double)args[1]->intVal : args[1]->floatVal;
            return a >= b ? args[0] : args[1];
        }
        throw ToRuntimeError("max() requires 1 list or 2 arguments");
    }));

    // round(x)
    env->define("round", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
        if (args.size() != 1) throw ToRuntimeError("round() takes exactly 1 argument");
        if (args[0]->type == ToValue::Type::INT) return args[0];
        if (args[0]->type == ToValue::Type::FLOAT) return ToValue::makeInt(static_cast<int64_t>(std::round(args[0]->floatVal)));
        throw ToRuntimeError("round() requires a number");
    }));

    // string methods — these will be handled in the interpreter via member access
    // but we add a few module-level utilities

    // read_file(path) — read file contents
    env->define("read_file", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
        if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
            throw ToRuntimeError("read_file() takes exactly 1 string argument");
        std::ifstream file(args[0]->strVal);
        if (!file.is_open()) throw ToRuntimeError("Could not open file: " + args[0]->strVal);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return ToValue::makeString(content);
    }));

    // write_file(path, content) — write to file
    env->define("write_file", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
        if (args.size() != 2 || args[0]->type != ToValue::Type::STRING)
            throw ToRuntimeError("write_file() takes exactly 2 arguments (path, content)");
        std::ofstream file(args[0]->strVal);
        if (!file.is_open()) throw ToRuntimeError("Could not open file for writing: " + args[0]->strVal);
        file << args[1]->toString();
        return ToValue::makeNone();
    }));
}
