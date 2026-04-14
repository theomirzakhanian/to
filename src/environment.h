#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include <functional>
#include <variant>
#include <sstream>
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "ast.h"

// Forward declarations
class Environment;
using EnvPtr = std::shared_ptr<Environment>;

// Value types for the interpreter
struct ToValue;
using ToValuePtr = std::shared_ptr<ToValue>;
using BuiltinFn = std::function<ToValuePtr(std::vector<ToValuePtr>)>;

struct ToFunction {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> paramTypes; // optional type hints
    std::string returnTypeHint;
    std::vector<ASTNodePtr> body;
    EnvPtr closure;
    bool isGenerator = false; // set if body contains 'yield'
};

struct ToClass {
    std::string name;
    std::shared_ptr<ToClass> parent; // inheritance
    std::vector<MethodDef> methods;
    EnvPtr closure;
};

struct ToInstance {
    std::shared_ptr<ToClass> klass;
    std::unordered_map<std::string, ToValuePtr> fields;
};

// Generator — backed by a coroutine-style thread that pauses on yield
struct ToGenerator {
    std::shared_ptr<std::thread> thread;
    std::shared_ptr<std::mutex> mtx;
    std::shared_ptr<std::condition_variable> cv;
    std::shared_ptr<std::shared_ptr<ToValue>> currentValue; // the yielded value
    std::shared_ptr<std::atomic<bool>> hasValue;             // set true when producer yields
    std::shared_ptr<std::atomic<bool>> consumerReady;        // set true when consumer asks for next
    std::shared_ptr<std::atomic<bool>> done;                 // producer finished
    std::shared_ptr<std::string> error;                      // error message if thrown
    ~ToGenerator();
};

struct ToValue {
    enum class Type { INT, FLOAT, STRING, BOOL, NONE, LIST, DICT, FUNCTION, BUILTIN, CLASS, INSTANCE, FUTURE, GENERATOR };
    Type type;

    int64_t intVal = 0;
    double floatVal = 0.0;
    std::string strVal;
    bool boolVal = false;
    std::vector<ToValuePtr> listVal;
    std::vector<std::pair<std::string, ToValuePtr>> dictVal;
    std::shared_ptr<ToFunction> funcVal;
    BuiltinFn builtinVal;
    std::shared_ptr<ToClass> classVal;
    std::shared_ptr<ToInstance> instanceVal;
    std::shared_ptr<std::future<ToValuePtr>> futureVal;
    std::shared_ptr<ToGenerator> generatorVal;

    // Factory methods
    static ToValuePtr makeInt(int64_t v) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::INT;
        val->intVal = v;
        return val;
    }
    static ToValuePtr makeFloat(double v) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::FLOAT;
        val->floatVal = v;
        return val;
    }
    static ToValuePtr makeString(const std::string& v) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::STRING;
        val->strVal = v;
        return val;
    }
    static ToValuePtr makeBool(bool v) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::BOOL;
        val->boolVal = v;
        return val;
    }
    static ToValuePtr makeNone() {
        auto val = std::make_shared<ToValue>();
        val->type = Type::NONE;
        return val;
    }
    static ToValuePtr makeList(std::vector<ToValuePtr> v) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::LIST;
        val->listVal = std::move(v);
        return val;
    }
    static ToValuePtr makeDict(std::vector<std::pair<std::string, ToValuePtr>> v) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::DICT;
        val->dictVal = std::move(v);
        return val;
    }
    static ToValuePtr makeFunction(std::shared_ptr<ToFunction> f) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::FUNCTION;
        val->funcVal = std::move(f);
        return val;
    }
    static ToValuePtr makeBuiltin(BuiltinFn f) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::BUILTIN;
        val->builtinVal = std::move(f);
        return val;
    }
    static ToValuePtr makeClass(std::shared_ptr<ToClass> c) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::CLASS;
        val->classVal = std::move(c);
        return val;
    }
    static ToValuePtr makeInstance(std::shared_ptr<ToInstance> i) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::INSTANCE;
        val->instanceVal = std::move(i);
        return val;
    }
    static ToValuePtr makeFuture(std::shared_ptr<std::future<ToValuePtr>> f) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::FUTURE;
        val->futureVal = std::move(f);
        return val;
    }
    static ToValuePtr makeGenerator(std::shared_ptr<ToGenerator> g) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::GENERATOR;
        val->generatorVal = std::move(g);
        return val;
    }

    // Convert to string for printing
    std::string toString() const {
        switch (type) {
            case Type::INT: return std::to_string(intVal);
            case Type::FLOAT: {
                std::ostringstream oss;
                oss << floatVal;
                return oss.str();
            }
            case Type::STRING: return strVal;
            case Type::BOOL: return boolVal ? "true" : "false";
            case Type::NONE: return "none";
            case Type::LIST: {
                std::string result = "[";
                for (size_t i = 0; i < listVal.size(); i++) {
                    if (i > 0) result += ", ";
                    if (listVal[i]->type == Type::STRING) {
                        result += "\"" + listVal[i]->toString() + "\"";
                    } else {
                        result += listVal[i]->toString();
                    }
                }
                result += "]";
                return result;
            }
            case Type::DICT: {
                std::string result = "{";
                for (size_t i = 0; i < dictVal.size(); i++) {
                    if (i > 0) result += ", ";
                    result += dictVal[i].first + " = ";
                    if (dictVal[i].second->type == Type::STRING) {
                        result += "\"" + dictVal[i].second->toString() + "\"";
                    } else {
                        result += dictVal[i].second->toString();
                    }
                }
                result += "}";
                return result;
            }
            case Type::FUNCTION: return "<function " + funcVal->name + ">";
            case Type::BUILTIN: return "<builtin function>";
            case Type::CLASS: return "<class " + classVal->name + ">";
            case Type::INSTANCE: return "<instance of " + instanceVal->klass->name + ">";
            case Type::FUTURE: return "<future>";
            case Type::GENERATOR: return "<generator>";
        }
        return "<unknown>";
    }

    // Truthiness
    bool isTruthy() const {
        switch (type) {
            case Type::BOOL: return boolVal;
            case Type::NONE: return false;
            case Type::INT: return intVal != 0;
            case Type::FLOAT: return floatVal != 0.0;
            case Type::STRING: return !strVal.empty();
            case Type::LIST: return !listVal.empty();
            case Type::DICT: return !dictVal.empty();
            default: return true;
        }
    }

    std::string typeName() const {
        switch (type) {
            case Type::INT: return "int";
            case Type::FLOAT: return "float";
            case Type::STRING: return "string";
            case Type::BOOL: return "bool";
            case Type::NONE: return "none";
            case Type::LIST: return "list";
            case Type::DICT: return "dict";
            case Type::FUNCTION: return "function";
            case Type::BUILTIN: return "builtin";
            case Type::CLASS: return "class";
            case Type::INSTANCE: return "instance";
            case Type::FUTURE: return "future";
            case Type::GENERATOR: return "generator";
        }
        return "unknown";
    }
};

class Environment : public std::enable_shared_from_this<Environment> {
public:
    explicit Environment(EnvPtr parent = nullptr) : parent(parent) {}

    ToValuePtr get(const std::string& name) const {
        auto it = values.find(name);
        if (it != values.end()) return it->second;
        if (parent) return parent->get(name);
        return nullptr;
    }

    bool has(const std::string& name) const {
        if (values.count(name)) return true;
        if (parent) return parent->has(name);
        return false;
    }

    void set(const std::string& name, ToValuePtr value) {
        // Check if constant
        if (constants.count(name)) {
            throw std::runtime_error("Cannot reassign constant '" + name + "'");
        }
        // If variable exists in a parent scope, update it there
        if (values.find(name) == values.end() && parent && parent->has(name)) {
            parent->set(name, value);
            return;
        }
        values[name] = value;
    }

    void define(const std::string& name, ToValuePtr value) {
        values[name] = value;
    }

    void defineConst(const std::string& name, ToValuePtr value) {
        values[name] = value;
        constants.insert(name);
    }

    bool isConst(const std::string& name) const {
        if (constants.count(name)) return true;
        if (parent) return parent->isConst(name);
        return false;
    }

    EnvPtr createChild() {
        return std::make_shared<Environment>(shared_from_this());
    }

private:
    EnvPtr parent;
    std::unordered_map<std::string, ToValuePtr> values;
    std::unordered_set<std::string> constants;
};
