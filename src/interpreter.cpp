#include "interpreter.h"
#include "builtins.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "http.h"
#include "ffi.h"
#include "to_stdlib.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <filesystem>

Interpreter::Interpreter(const std::string& filename)
    : filename(filename), globalEnv(std::make_shared<Environment>()) {
    registerBuiltins(globalEnv);
}

void Interpreter::pushFrame(const std::string& name, const std::string& file, int line) {
    std::lock_guard<std::mutex> lock(callStackMutex);
    callStack.push_back({name, file, line});
}

void Interpreter::popFrame() {
    std::lock_guard<std::mutex> lock(callStackMutex);
    if (!callStack.empty()) callStack.pop_back();
}

std::string Interpreter::formatStackTrace() const {
    // Note: caller should hold lock or be in single-threaded context
    if (callStack.empty()) return "";
    std::string result = "Stack trace (most recent call last):\n";
    for (size_t i = 0; i < callStack.size(); i++) {
        auto& frame = callStack[i];
        result += "  " + frame.filename + ":" + std::to_string(frame.line) +
                  " in " + frame.functionName + "\n";
    }
    return result;
}

void Interpreter::debugPrompt(ASTNodePtr node, EnvPtr env) {
    // Check if we should break
    bool shouldBreak = stepMode;
    if (!shouldBreak) {
        for (int bp : breakpoints) {
            if (node->line == bp) { shouldBreak = true; break; }
        }
    }
    if (!shouldBreak) return;

    std::cout << "\n[debug] Paused at line " << node->line << "\n";

    while (true) {
        std::cout << "debug> ";
        std::string cmd;
        if (!std::getline(std::cin, cmd)) break;

        // Trim
        size_t s = cmd.find_first_not_of(" \t");
        if (s == std::string::npos) continue;
        cmd = cmd.substr(s);

        if (cmd == "n" || cmd == "next" || cmd == "s" || cmd == "step") {
            stepMode = true;
            break;
        } else if (cmd == "c" || cmd == "continue") {
            stepMode = false;
            break;
        } else if (cmd == "q" || cmd == "quit") {
            exit(0);
        } else if (cmd == "stack" || cmd == "bt") {
            std::cout << formatStackTrace();
        } else if (cmd.substr(0, 2) == "p " || cmd.substr(0, 6) == "print ") {
            // Print a variable
            std::string varName = (cmd[0] == 'p' && cmd[1] == ' ') ?
                cmd.substr(2) : cmd.substr(6);
            // Trim varName
            size_t vs = varName.find_first_not_of(" \t");
            if (vs != std::string::npos) varName = varName.substr(vs);
            size_t ve = varName.find_last_not_of(" \t");
            if (ve != std::string::npos) varName = varName.substr(0, ve + 1);
            auto val = env->get(varName);
            if (val) {
                std::cout << varName << " = " << val->toString() << " (" << val->typeName() << ")\n";
            } else {
                std::cout << "Variable '" << varName << "' not found\n";
            }
        } else if (cmd.substr(0, 2) == "b " || cmd.substr(0, 6) == "break ") {
            std::string lineStr = (cmd[0] == 'b' && cmd[1] == ' ') ?
                cmd.substr(2) : cmd.substr(6);
            try {
                int bp = std::stoi(lineStr);
                breakpoints.push_back(bp);
                std::cout << "Breakpoint set at line " << bp << "\n";
            } catch (...) {
                std::cout << "Usage: break <line_number>\n";
            }
        } else if (cmd == "locals" || cmd == "vars") {
            // This would need env to expose its variables — simplified
            std::cout << "(Use 'p <varname>' to inspect specific variables)\n";
        } else if (cmd == "help" || cmd == "h") {
            std::cout << "Debugger commands:\n"
                      << "  n/next/s/step  — execute next line\n"
                      << "  c/continue     — continue until next breakpoint\n"
                      << "  p <var>        — print variable value\n"
                      << "  b <line>       — set breakpoint at line\n"
                      << "  stack/bt       — show call stack\n"
                      << "  q/quit         — exit\n"
                      << "  help           — show this help\n";
        } else {
            std::cout << "Unknown command. Type 'help' for available commands.\n";
        }
    }
}

void Interpreter::execute(ASTNodePtr program) {
    if (program->type != NodeType::Program) {
        throw ToRuntimeError("Expected program node");
    }
    pushFrame("<main>", filename, 1);
    for (auto& stmt : program->statements) {
        execStatement(stmt, globalEnv);
    }
    popFrame();
}

void Interpreter::execStatement(ASTNodePtr node, EnvPtr env) {
    // Debugger hook
    if (debugMode) {
        debugPrompt(node, env);
    }

    switch (node->type) {
        case NodeType::PrintStmt: execPrint(node, env); break;
        case NodeType::Assignment: execAssignment(node, env); break;
        case NodeType::ConstDecl: execConstDecl(node, env); break;
        case NodeType::IfBlock: execIf(node, env); break;
        case NodeType::GivenBlock: execGiven(node, env); break;
        case NodeType::WhileLoop: execWhile(node, env); break;
        case NodeType::ThroughLoop: execThrough(node, env); break;
        case NodeType::FunctionDef: execFunctionDef(node, env); break;
        case NodeType::ClassDef: execClassDef(node, env); break;
        case NodeType::TryCatchFinally: execTryCatch(node, env); break;
        case NodeType::ImportStmt: execImport(node, env); break;
        case NodeType::EnumDef: {
            // Create a dict-like object with enum values
            auto enumObj = ToValue::makeDict({});
            for (size_t i = 0; i < node->enumValues.size(); i++) {
                enumObj->dictVal.push_back({node->enumValues[i], ToValue::makeString(node->enumValues[i])});
            }
            // Add values() method
            auto values = std::make_shared<std::vector<std::string>>(node->enumValues);
            enumObj->dictVal.push_back({"values", ToValue::makeBuiltin(
                [values](std::vector<ToValuePtr> args) -> ToValuePtr {
                    std::vector<ToValuePtr> list;
                    for (auto& v : *values) list.push_back(ToValue::makeString(v));
                    return ToValue::makeList(std::move(list));
                }
            )});
            // Add has() method
            enumObj->dictVal.push_back({"has", ToValue::makeBuiltin(
                [values](std::vector<ToValuePtr> args) -> ToValuePtr {
                    if (args.empty() || args[0]->type != ToValue::Type::STRING)
                        throw ToRuntimeError("enum.has() takes 1 string argument");
                    for (auto& v : *values) {
                        if (v == args[0]->strVal) return ToValue::makeBool(true);
                    }
                    return ToValue::makeBool(false);
                }
            )});
            env->define(node->name, enumObj);
            break;
        }
        case NodeType::ShapeDef: {
            // Store shape definition as a dict with method signatures
            auto shapeObj = ToValue::makeDict({});
            shapeObj->dictVal.push_back({"__shape__", ToValue::makeBool(true)});
            shapeObj->dictVal.push_back({"__name__", ToValue::makeString(node->name)});
            for (auto& [name, types] : node->shapeMethodSigs) {
                shapeObj->dictVal.push_back({name, ToValue::makeBool(true)});
            }
            env->define(node->name, shapeObj);
            break;
        }
        case NodeType::AssertStmt: {
            auto val = eval(node->value, env);
            if (!val->isTruthy()) {
                throw ToRuntimeError("Assertion failed at line " + std::to_string(node->line), node->line);
            }
            break;
        }
        case NodeType::DestructureList: {
            auto val = eval(node->value, env);
            if (val->type != ToValue::Type::LIST)
                throw ToRuntimeError("Cannot destructure non-list value", node->line);
            auto& list = val->listVal;
            for (size_t i = 0; i < node->destructNames.size(); i++) {
                if (i < list.size()) {
                    env->define(node->destructNames[i], list[i]);
                } else {
                    env->define(node->destructNames[i], ToValue::makeNone());
                }
            }
            if (!node->restName.empty()) {
                std::vector<ToValuePtr> rest;
                for (size_t i = node->destructNames.size(); i < list.size(); i++) {
                    rest.push_back(list[i]);
                }
                env->define(node->restName, ToValue::makeList(std::move(rest)));
            }
            break;
        }
        case NodeType::DestructureDict: {
            auto val = eval(node->value, env);
            if (val->type != ToValue::Type::DICT)
                throw ToRuntimeError("Cannot destructure non-dict value", node->line);
            for (auto& name : node->destructNames) {
                bool found = false;
                for (auto& [k, v] : val->dictVal) {
                    if (k == name) {
                        env->define(name, v);
                        found = true;
                        break;
                    }
                }
                if (!found) env->define(name, ToValue::makeNone());
            }
            break;
        }
        case NodeType::ReturnStmt: {
            ToValuePtr val = node->value ? eval(node->value, env) : ToValue::makeNone();
            throw ReturnException(val);
        }
        case NodeType::BreakStmt: throw BreakSignal();
        case NodeType::ContinueStmt: throw ContinueSignal();
        case NodeType::ExpressionStmt: eval(node->value, env); break;
        default:
            throw ToRuntimeError("Unknown statement type at line " + std::to_string(node->line));
    }
}

void Interpreter::execPrint(ASTNodePtr node, EnvPtr env) {
    auto val = eval(node->value, env);
    std::cout << val->toString() << std::endl;
}

void Interpreter::execAssignment(ASTNodePtr node, EnvPtr env) {
    auto val = eval(node->value, env);
    auto target = node->target;

    // Handle compound assignment (+=, -=, etc.)
    if (node->assignOp != "=") {
        ToValuePtr existing;
        if (target->type == NodeType::Identifier) {
            existing = env->get(target->name);
            if (!existing) throw ToRuntimeError("Undefined variable '" + target->name + "'", target->line);
        } else if (target->type == NodeType::MemberAccess) {
            existing = evalMemberAccess(target, env);
        } else if (target->type == NodeType::IndexExpr) {
            existing = evalIndexExpr(target, env);
        } else {
            throw ToRuntimeError("Invalid assignment target", target->line);
        }

        // Compute new value
        if (node->assignOp == "+=") {
            if (existing->type == ToValue::Type::INT && val->type == ToValue::Type::INT)
                val = ToValue::makeInt(existing->intVal + val->intVal);
            else if (existing->type == ToValue::Type::FLOAT || val->type == ToValue::Type::FLOAT) {
                double a = existing->type == ToValue::Type::INT ? (double)existing->intVal : existing->floatVal;
                double b = val->type == ToValue::Type::INT ? (double)val->intVal : val->floatVal;
                val = ToValue::makeFloat(a + b);
            } else if (existing->type == ToValue::Type::STRING) {
                val = ToValue::makeString(existing->strVal + val->toString());
            } else {
                throw ToRuntimeError("Cannot use += with " + existing->typeName(), target->line);
            }
        } else if (node->assignOp == "-=") {
            if (existing->type == ToValue::Type::INT && val->type == ToValue::Type::INT)
                val = ToValue::makeInt(existing->intVal - val->intVal);
            else {
                double a = existing->type == ToValue::Type::INT ? (double)existing->intVal : existing->floatVal;
                double b = val->type == ToValue::Type::INT ? (double)val->intVal : val->floatVal;
                val = ToValue::makeFloat(a - b);
            }
        } else if (node->assignOp == "*=") {
            if (existing->type == ToValue::Type::INT && val->type == ToValue::Type::INT)
                val = ToValue::makeInt(existing->intVal * val->intVal);
            else {
                double a = existing->type == ToValue::Type::INT ? (double)existing->intVal : existing->floatVal;
                double b = val->type == ToValue::Type::INT ? (double)val->intVal : val->floatVal;
                val = ToValue::makeFloat(a * b);
            }
        } else if (node->assignOp == "/=") {
            double a = existing->type == ToValue::Type::INT ? (double)existing->intVal : existing->floatVal;
            double b = val->type == ToValue::Type::INT ? (double)val->intVal : val->floatVal;
            if (b == 0) throw ToRuntimeError("Division by zero", target->line);
            if (existing->type == ToValue::Type::INT && val->type == ToValue::Type::INT && existing->intVal % val->intVal == 0)
                val = ToValue::makeInt(existing->intVal / val->intVal);
            else
                val = ToValue::makeFloat(a / b);
        }
    }

    // Assign to target
    if (target->type == NodeType::Identifier) {
        if (env->isConst(target->name)) {
            throw ToRuntimeError("Cannot reassign constant '" + target->name + "'", target->line);
        }
        env->set(target->name, val);
    } else if (target->type == NodeType::MemberAccess) {
        // obj.member = val
        auto obj = eval(target->object, env);
        if (obj->type == ToValue::Type::INSTANCE) {
            obj->instanceVal->fields[target->member] = val;
        } else if (obj->type == ToValue::Type::DICT) {
            for (auto& pair : obj->dictVal) {
                if (pair.first == target->member) {
                    pair.second = val;
                    return;
                }
            }
            obj->dictVal.push_back({target->member, val});
        } else {
            throw ToRuntimeError("Cannot set member on " + obj->typeName(), target->line);
        }
    } else if (target->type == NodeType::IndexExpr) {
        auto obj = eval(target->object, env);
        auto idx = eval(target->indexExpr, env);
        if (obj->type == ToValue::Type::LIST) {
            if (idx->type != ToValue::Type::INT)
                throw ToRuntimeError("List index must be an integer", target->line);
            int64_t i = idx->intVal;
            if (i < 0) i += obj->listVal.size();
            if (i < 0 || i >= (int64_t)obj->listVal.size())
                throw ToRuntimeError("Index out of bounds", target->line);
            obj->listVal[i] = val;
        } else {
            throw ToRuntimeError("Cannot index " + obj->typeName(), target->line);
        }
    } else {
        throw ToRuntimeError("Invalid assignment target", target->line);
    }
}

void Interpreter::execConstDecl(ASTNodePtr node, EnvPtr env) {
    auto val = eval(node->value, env);
    env->defineConst(node->name, val);
}

void Interpreter::execIf(ASTNodePtr node, EnvPtr env) {
    auto cond = eval(node->condition, env);
    if (cond->isTruthy()) {
        execBlock(node->body, env);
        return;
    }
    for (auto& branch : node->orBranches) {
        auto brCond = eval(branch.condition, env);
        if (brCond->isTruthy()) {
            execBlock(branch.body, env);
            return;
        }
    }
    if (!node->elseBody.empty()) {
        execBlock(node->elseBody, env);
    }
}

void Interpreter::execGiven(ASTNodePtr node, EnvPtr env) {
    auto val = eval(node->givenExpr, env);

    for (auto& branch : node->givenBranches) {
        if (!branch.condition) {
            // else branch
            execBlock(branch.body, env);
            return;
        }
        auto branchVal = eval(branch.condition, env);
        // Compare
        bool match = false;
        if (val->type == branchVal->type) {
            switch (val->type) {
                case ToValue::Type::INT: match = val->intVal == branchVal->intVal; break;
                case ToValue::Type::FLOAT: match = val->floatVal == branchVal->floatVal; break;
                case ToValue::Type::STRING: match = val->strVal == branchVal->strVal; break;
                case ToValue::Type::BOOL: match = val->boolVal == branchVal->boolVal; break;
                default: match = false;
            }
        }
        if (match) {
            execBlock(branch.body, env);
            return;
        }
    }
}

void Interpreter::execWhile(ASTNodePtr node, EnvPtr env) {
    while (true) {
        auto cond = eval(node->condition, env);
        if (!cond->isTruthy()) break;
        try {
            execBlock(node->body, env);
        } catch (BreakSignal&) {
            break;
        } catch (ContinueSignal&) {
            continue;
        }
    }
}

void Interpreter::execThrough(ASTNodePtr node, EnvPtr env) {
    auto iterable = eval(node->iterable, env);

    std::vector<ToValuePtr> items;
    if (iterable->type == ToValue::Type::LIST) {
        items = iterable->listVal;
    } else if (iterable->type == ToValue::Type::DICT) {
        // Iterate over keys by default
        for (auto& pair : iterable->dictVal) {
            items.push_back(ToValue::makeString(pair.first));
        }
    } else if (iterable->type == ToValue::Type::STRING) {
        // Iterate over characters
        for (char c : iterable->strVal) {
            items.push_back(ToValue::makeString(std::string(1, c)));
        }
    } else {
        throw ToRuntimeError("Cannot iterate over " + iterable->typeName(), node->line);
    }

    for (auto& item : items) {
        auto loopEnv = env->createChild();
        loopEnv->define(node->loopVar, item);
        try {
            for (auto& stmt : node->body) {
                execStatement(stmt, loopEnv);
            }
        } catch (BreakSignal&) {
            break;
        } catch (ContinueSignal&) {
            continue;
        }
    }
}

void Interpreter::execFunctionDef(ASTNodePtr node, EnvPtr env) {
    auto func = std::make_shared<ToFunction>();
    func->name = node->name;
    func->params = node->params;
    func->paramTypes = node->paramTypes;
    func->returnTypeHint = node->returnTypeHint;
    func->body = node->body;
    func->closure = env;
    env->define(node->name, ToValue::makeFunction(func));
}

void Interpreter::execClassDef(ASTNodePtr node, EnvPtr env) {
    auto klass = std::make_shared<ToClass>();
    klass->name = node->name;
    klass->methods = node->methods;
    klass->closure = env;

    // Inheritance: copy parent methods that aren't overridden
    if (!node->parentClass.empty()) {
        auto parentVal = env->get(node->parentClass);
        if (!parentVal || parentVal->type != ToValue::Type::CLASS)
            throw ToRuntimeError("Parent class '" + node->parentClass + "' not found", node->line);
        klass->parent = parentVal->classVal;
        // Prepend parent methods that aren't overridden by child
        std::vector<MethodDef> merged;
        for (auto& pm : parentVal->classVal->methods) {
            bool overridden = false;
            for (auto& cm : klass->methods) {
                if (cm.name == pm.name) { overridden = true; break; }
            }
            if (!overridden) merged.push_back(pm);
        }
        for (auto& cm : klass->methods) merged.push_back(cm);
        klass->methods = std::move(merged);
    }

    // Verify shape compliance if class fits a shape
    if (!node->fitsShape.empty()) {
        auto shapeVal = env->get(node->fitsShape);
        if (!shapeVal || shapeVal->type != ToValue::Type::DICT)
            throw ToRuntimeError("Shape '" + node->fitsShape + "' not found", node->line);
        // Check each required method exists in the class
        for (auto& [key, val] : shapeVal->dictVal) {
            if (key.substr(0, 2) == "__") continue; // skip meta keys
            bool found = false;
            for (auto& m : klass->methods) {
                if (m.name == key) { found = true; break; }
            }
            if (!found) {
                throw ToRuntimeError("Class '" + node->name + "' fits '" + node->fitsShape +
                    "' but is missing method '" + key + "'", node->line);
            }
        }
    }

    env->define(node->name, ToValue::makeClass(klass));
}

void Interpreter::execTryCatch(ASTNodePtr node, EnvPtr env) {
    try {
        execBlock(node->tryBody, env);
    } catch (ReturnException&) {
        // Execute finally, then rethrow
        if (!node->finallyBody.empty()) {
            execBlock(node->finallyBody, env);
        }
        throw;
    } catch (ToRuntimeError& e) {
        if (!node->catchClause.errorName.empty()) {
            auto catchEnv = env->createChild();
            catchEnv->define(node->catchClause.errorName, ToValue::makeString(e.detail));
            execBlock(node->catchClause.body, catchEnv);
        }
    } catch (std::runtime_error& e) {
        if (!node->catchClause.errorName.empty()) {
            auto catchEnv = env->createChild();
            catchEnv->define(node->catchClause.errorName, ToValue::makeString(e.what()));
            execBlock(node->catchClause.body, catchEnv);
        }
    }

    if (!node->finallyBody.empty()) {
        execBlock(node->finallyBody, env);
    }
}

void Interpreter::execImport(ASTNodePtr node, EnvPtr env) {
    std::string modulePath = node->modulePath;

    // Check if it's a file path (has .to extension or quotes)
    std::string filepath;
    if (modulePath.size() > 3 && modulePath.substr(modulePath.size() - 3) == ".to") {
        filepath = modulePath;
    } else {
        // Try as a stdlib module
        std::filesystem::path basePath = std::filesystem::path(filename).parent_path();
        filepath = (basePath / (modulePath + ".to")).string();
        if (!std::filesystem::exists(filepath)) {
            // Try stdlib path
            // For now, just create an empty module for known stdlib modules
            if (modulePath == "math" || modulePath == "io" || modulePath == "string" || modulePath == "web" || modulePath == "json" || modulePath == "ffi" || modulePath == "time" || modulePath == "fs" || modulePath == "regex") {
                // Register basic module builtins
                if (modulePath == "io") {
                    auto ioModule = ToValue::makeDict({});
                    ioModule->dictVal.push_back({"read", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
                        if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
                            throw ToRuntimeError("io.read() takes exactly 1 string argument");
                        std::ifstream file(args[0]->strVal);
                        if (!file.is_open()) throw ToRuntimeError("Could not open file: " + args[0]->strVal);
                        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                        return ToValue::makeString(content);
                    })});
                    ioModule->dictVal.push_back({"write", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
                        if (args.size() != 2 || args[0]->type != ToValue::Type::STRING)
                            throw ToRuntimeError("io.write() takes exactly 2 arguments (path, content)");
                        std::ofstream file(args[0]->strVal);
                        if (!file.is_open()) throw ToRuntimeError("Could not open file: " + args[0]->strVal);
                        file << args[1]->toString();
                        return ToValue::makeNone();
                    })});
                    env->define("io", ioModule);
                } else if (modulePath == "math") {
                    auto mathModule = ToValue::makeDict({});
                    mathModule->dictVal.push_back({"pi", ToValue::makeFloat(3.14159265358979323846)});
                    mathModule->dictVal.push_back({"e", ToValue::makeFloat(2.71828182845904523536)});
                    mathModule->dictVal.push_back({"sqrt", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
                        if (args.size() != 1) throw ToRuntimeError("math.sqrt() takes 1 argument");
                        double v = args[0]->type == ToValue::Type::INT ? (double)args[0]->intVal : args[0]->floatVal;
                        return ToValue::makeFloat(std::sqrt(v));
                    })});
                    mathModule->dictVal.push_back({"sin", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
                        if (args.size() != 1) throw ToRuntimeError("math.sin() takes 1 argument");
                        double v = args[0]->type == ToValue::Type::INT ? (double)args[0]->intVal : args[0]->floatVal;
                        return ToValue::makeFloat(std::sin(v));
                    })});
                    mathModule->dictVal.push_back({"cos", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
                        if (args.size() != 1) throw ToRuntimeError("math.cos() takes 1 argument");
                        double v = args[0]->type == ToValue::Type::INT ? (double)args[0]->intVal : args[0]->floatVal;
                        return ToValue::makeFloat(std::cos(v));
                    })});
                    mathModule->dictVal.push_back({"floor", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
                        if (args.size() != 1) throw ToRuntimeError("math.floor() takes 1 argument");
                        double v = args[0]->type == ToValue::Type::INT ? (double)args[0]->intVal : args[0]->floatVal;
                        return ToValue::makeInt(static_cast<int64_t>(std::floor(v)));
                    })});
                    mathModule->dictVal.push_back({"ceil", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
                        if (args.size() != 1) throw ToRuntimeError("math.ceil() takes 1 argument");
                        double v = args[0]->type == ToValue::Type::INT ? (double)args[0]->intVal : args[0]->floatVal;
                        return ToValue::makeInt(static_cast<int64_t>(std::ceil(v)));
                    })});
                    mathModule->dictVal.push_back({"pow", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) -> ToValuePtr {
                        if (args.size() != 2) throw ToRuntimeError("math.pow() takes 2 arguments");
                        double base = args[0]->type == ToValue::Type::INT ? (double)args[0]->intVal : args[0]->floatVal;
                        double exp = args[1]->type == ToValue::Type::INT ? (double)args[1]->intVal : args[1]->floatVal;
                        return ToValue::makeFloat(std::pow(base, exp));
                    })});
                    env->define("math", mathModule);
                } else if (modulePath == "web") {
                    registerWebModule(env, this);
                } else if (modulePath == "json") {
                    registerJsonModule(env);
                } else if (modulePath == "ffi") {
                    registerFFIModule(env);
                } else if (modulePath == "time") {
                    registerTimeModule(env);
                } else if (modulePath == "fs") {
                    registerFSModule(env);
                } else if (modulePath == "regex") {
                    registerRegexModule(env);
                }
                if (!node->importNames.empty()) {
                    auto mod = env->get(modulePath);
                    if (mod && mod->type == ToValue::Type::DICT) {
                        for (auto& name : node->importNames) {
                            for (auto& pair : mod->dictVal) {
                                if (pair.first == name) {
                                    env->define(name, pair.second);
                                    break;
                                }
                            }
                        }
                    }
                }
                return;
            }
            throw ToRuntimeError("Module not found: " + modulePath, node->line);
        }
    }

    // Read and execute the module file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw ToRuntimeError("Could not open module: " + filepath, node->line);
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    Lexer lexer(content, filepath);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, filepath);
    auto moduleAST = parser.parse();

    // Execute in a new environment that shares builtins
    auto moduleEnv = std::make_shared<Environment>(globalEnv);
    Interpreter moduleInterp(filepath);
    moduleInterp.execute(moduleAST);

    if (node->importNames.empty()) {
        // Import entire module — not applicable for file imports
        // Just import everything into current scope
        // This is simplified — a full implementation would use module objects
    } else {
        // Import specific names
        for (auto& name : node->importNames) {
            auto val = moduleInterp.getGlobalEnv()->get(name);
            if (!val) {
                throw ToRuntimeError("'" + name + "' not found in module " + modulePath, node->line);
            }
            env->define(name, val);
        }
    }
}

void Interpreter::execBlock(const std::vector<ASTNodePtr>& stmts, EnvPtr env) {
    auto blockEnv = env->createChild();
    for (auto& stmt : stmts) {
        execStatement(stmt, blockEnv);
    }
}

// ========================
// Expression evaluation
// ========================

ToValuePtr Interpreter::eval(ASTNodePtr node, EnvPtr env) {
    switch (node->type) {
        case NodeType::IntegerLiteral:
            return ToValue::makeInt(node->intValue);
        case NodeType::FloatLiteral:
            return ToValue::makeFloat(node->floatValue);
        case NodeType::StringLiteral:
            return ToValue::makeString(node->stringValue);
        case NodeType::BoolLiteral:
            return ToValue::makeBool(node->boolValue);
        case NodeType::NoneLiteral:
            return ToValue::makeNone();
        case NodeType::Identifier: {
            auto val = env->get(node->name);
            if (!val) throw ToRuntimeError("Undefined variable '" + node->name + "'", node->line);
            return val;
        }
        case NodeType::BinaryExpr:
            return evalBinaryExpr(node, env);
        case NodeType::UnaryExpr:
            return evalUnaryExpr(node, env);
        case NodeType::CallExpr:
            return evalCallExpr(node, env);
        case NodeType::MemberAccess:
            return evalMemberAccess(node, env);
        case NodeType::IndexExpr:
            return evalIndexExpr(node, env);
        case NodeType::StringInterpolation:
            return evalStringInterp(node, env);
        case NodeType::ListLiteral:
            return evalListLiteral(node, env);
        case NodeType::DictLiteral:
            return evalDictLiteral(node, env);
        case NodeType::RangeLiteral:
            return evalRangeLiteral(node, env);
        case NodeType::LambdaExpr: {
            auto func = std::make_shared<ToFunction>();
            func->name = "<lambda>";
            func->params = node->params;
            // Wrap the expression in a return statement
            auto retNode = std::make_shared<ASTNode>();
            retNode->type = NodeType::ReturnStmt;
            retNode->value = node->value;
            retNode->line = node->line;
            func->body = {retNode};
            func->closure = env;
            return ToValue::makeFunction(func);
        }
        case NodeType::AsyncExpr: {
            // async func(args) — run in a new thread, return a future
            auto exprNode = node->value;
            // We need to evaluate the call but in a thread
            // First evaluate the callee and args in the current thread
            if (exprNode->type != NodeType::CallExpr) {
                throw ToRuntimeError("async requires a function call", node->line);
            }
            auto callee = eval(exprNode->callee, env);
            std::vector<ToValuePtr> args;
            for (auto& arg : exprNode->arguments) {
                args.push_back(eval(arg, env));
            }
            int line = node->line;
            auto interp = this;
            auto fut = std::make_shared<std::future<ToValuePtr>>(
                std::async(std::launch::async, [interp, callee, args, line]() -> ToValuePtr {
                    return interp->callFunction(callee, args, line);
                })
            );
            return ToValue::makeFuture(fut);
        }
        case NodeType::AwaitExpr: {
            auto val = eval(node->value, env);
            if (val->type != ToValue::Type::FUTURE) {
                // If it's not a future, just return it (already resolved)
                return val;
            }
            // Block until the future completes
            return val->futureVal->get();
        }
        case NodeType::PipeExpr: {
            // left then func(args) -> func(left, args...)
            // or left then func -> func(left)
            auto leftVal = eval(node->left, env);
            auto rightNode = node->right;

            if (rightNode->type == NodeType::CallExpr) {
                // func(args) — prepend left as first argument
                auto callee = eval(rightNode->callee, env);
                std::vector<ToValuePtr> args = {leftVal};
                for (auto& arg : rightNode->arguments) {
                    args.push_back(eval(arg, env));
                }
                return callFunction(callee, args, node->line);
            } else {
                // Just a function name — call with left as sole argument
                auto callee = eval(rightNode, env);
                return callFunction(callee, {leftVal}, node->line);
            }
        }
        default:
            throw ToRuntimeError("Cannot evaluate node type", node->line);
    }
}

ToValuePtr Interpreter::evalBinaryExpr(ASTNodePtr node, EnvPtr env) {
    // Short-circuit for logical operators
    if (node->op == "or") {
        auto left = eval(node->left, env);
        if (left->isTruthy()) return left;
        return eval(node->right, env);
    }
    if (node->op == "and") {
        auto left = eval(node->left, env);
        if (!left->isTruthy()) return left;
        return eval(node->right, env);
    }

    auto left = eval(node->left, env);

    // Check for range operator (..)
    if (node->op == "..") {
        auto right = eval(node->right, env);
        if (left->type != ToValue::Type::INT || right->type != ToValue::Type::INT) {
            throw ToRuntimeError("Range operator (..) requires integers", node->line);
        }
        std::vector<ToValuePtr> list;
        for (int64_t i = left->intVal; i < right->intVal; i++) {
            list.push_back(ToValue::makeInt(i));
        }
        return ToValue::makeList(std::move(list));
    }

    auto right = eval(node->right, env);

    // String concatenation
    if (node->op == "+" && (left->type == ToValue::Type::STRING || right->type == ToValue::Type::STRING)) {
        return ToValue::makeString(left->toString() + right->toString());
    }

    // Numeric operations
    if ((left->type == ToValue::Type::INT || left->type == ToValue::Type::FLOAT) &&
        (right->type == ToValue::Type::INT || right->type == ToValue::Type::FLOAT)) {

        bool useFloat = left->type == ToValue::Type::FLOAT || right->type == ToValue::Type::FLOAT;
        double lv = left->type == ToValue::Type::INT ? (double)left->intVal : left->floatVal;
        double rv = right->type == ToValue::Type::INT ? (double)right->intVal : right->floatVal;

        if (node->op == "+") {
            if (!useFloat) return ToValue::makeInt(left->intVal + right->intVal);
            return ToValue::makeFloat(lv + rv);
        }
        if (node->op == "-") {
            if (!useFloat) return ToValue::makeInt(left->intVal - right->intVal);
            return ToValue::makeFloat(lv - rv);
        }
        if (node->op == "*") {
            if (!useFloat) return ToValue::makeInt(left->intVal * right->intVal);
            return ToValue::makeFloat(lv * rv);
        }
        if (node->op == "/") {
            if (rv == 0) throw ToRuntimeError("Division by zero", node->line);
            if (!useFloat && left->intVal % right->intVal == 0)
                return ToValue::makeInt(left->intVal / right->intVal);
            return ToValue::makeFloat(lv / rv);
        }
        if (node->op == "%") {
            if (right->intVal == 0) throw ToRuntimeError("Modulo by zero", node->line);
            if (!useFloat) return ToValue::makeInt(left->intVal % right->intVal);
            return ToValue::makeFloat(std::fmod(lv, rv));
        }

        // Comparisons
        if (node->op == "==") return ToValue::makeBool(lv == rv);
        if (node->op == "!=") return ToValue::makeBool(lv != rv);
        if (node->op == "<") return ToValue::makeBool(lv < rv);
        if (node->op == "<=") return ToValue::makeBool(lv <= rv);
        if (node->op == ">") return ToValue::makeBool(lv > rv);
        if (node->op == ">=") return ToValue::makeBool(lv >= rv);
    }

    // Equality for non-numeric types
    if (node->op == "==") {
        if (left->type != right->type) return ToValue::makeBool(false);
        switch (left->type) {
            case ToValue::Type::STRING: return ToValue::makeBool(left->strVal == right->strVal);
            case ToValue::Type::BOOL: return ToValue::makeBool(left->boolVal == right->boolVal);
            case ToValue::Type::NONE: return ToValue::makeBool(true);
            default: return ToValue::makeBool(false);
        }
    }
    if (node->op == "!=") {
        if (left->type != right->type) return ToValue::makeBool(true);
        switch (left->type) {
            case ToValue::Type::STRING: return ToValue::makeBool(left->strVal != right->strVal);
            case ToValue::Type::BOOL: return ToValue::makeBool(left->boolVal != right->boolVal);
            case ToValue::Type::NONE: return ToValue::makeBool(false);
            default: return ToValue::makeBool(true);
        }
    }

    // String comparison
    if (left->type == ToValue::Type::STRING && right->type == ToValue::Type::STRING) {
        if (node->op == "<") return ToValue::makeBool(left->strVal < right->strVal);
        if (node->op == "<=") return ToValue::makeBool(left->strVal <= right->strVal);
        if (node->op == ">") return ToValue::makeBool(left->strVal > right->strVal);
        if (node->op == ">=") return ToValue::makeBool(left->strVal >= right->strVal);
    }

    throw ToRuntimeError("Unsupported operation '" + node->op + "' between " +
        left->typeName() + " and " + right->typeName(), node->line);
}

ToValuePtr Interpreter::evalUnaryExpr(ASTNodePtr node, EnvPtr env) {
    auto operand = eval(node->operand, env);
    if (node->op == "-") {
        if (operand->type == ToValue::Type::INT) return ToValue::makeInt(-operand->intVal);
        if (operand->type == ToValue::Type::FLOAT) return ToValue::makeFloat(-operand->floatVal);
        throw ToRuntimeError("Cannot negate " + operand->typeName(), node->line);
    }
    if (node->op == "not") {
        return ToValue::makeBool(!operand->isTruthy());
    }
    throw ToRuntimeError("Unknown unary operator: " + node->op, node->line);
}

ToValuePtr Interpreter::evalCallExpr(ASTNodePtr node, EnvPtr env) {
    // Evaluate arguments
    std::vector<ToValuePtr> args;
    for (auto& arg : node->arguments) {
        args.push_back(eval(arg, env));
    }

    // If callee is a member access, handle method calls
    if (node->callee->type == NodeType::MemberAccess) {
        auto obj = eval(node->callee->object, env);
        std::string method = node->callee->member;

        // List methods
        if (obj->type == ToValue::Type::LIST) {
            return callListMethod(obj, method, args);
        }
        // String methods
        if (obj->type == ToValue::Type::STRING) {
            return callStringMethod(obj, method, args);
        }
        // Dict methods
        if (obj->type == ToValue::Type::DICT) {
            // Check if the member is a callable
            for (auto& pair : obj->dictVal) {
                if (pair.first == method) {
                    if (pair.second->type == ToValue::Type::BUILTIN) {
                        return pair.second->builtinVal(args);
                    }
                    if (pair.second->type == ToValue::Type::FUNCTION) {
                        return callFunction(pair.second, args, node->line);
                    }
                }
            }
            return callDictMethod(obj, method, args);
        }
        // Instance methods
        if (obj->type == ToValue::Type::INSTANCE) {
            auto& inst = obj->instanceVal;
            // Look up method in class
            for (auto& m : inst->klass->methods) {
                if (m.name == method) {
                    auto methodEnv = inst->klass->closure->createChild();
                    methodEnv->define("my", obj);
                    // Bind params
                    if (args.size() != m.params.size()) {
                        throw ToRuntimeError("Method '" + method + "' expects " +
                            std::to_string(m.params.size()) + " arguments, got " +
                            std::to_string(args.size()), node->line);
                    }
                    for (size_t i = 0; i < m.params.size(); i++) {
                        methodEnv->define(m.params[i], args[i]);
                    }
                    try {
                        for (auto& stmt : m.body) {
                            execStatement(stmt, methodEnv);
                        }
                    } catch (ReturnException& e) {
                        return e.value;
                    }
                    return ToValue::makeNone();
                }
            }
            // Check fields (might be a function stored in instance)
            auto it = inst->fields.find(method);
            if (it != inst->fields.end()) {
                return callFunction(it->second, args, node->line);
            }
            throw ToRuntimeError("'" + inst->klass->name + "' has no method '" + method + "'", node->line);
        }
        throw ToRuntimeError("Cannot call method on " + obj->typeName(), node->line);
    }

    auto callee = eval(node->callee, env);
    return callFunction(callee, args, node->line);
}

ToValuePtr Interpreter::callFunction(ToValuePtr callee, const std::vector<ToValuePtr>& args, int line) {
    if (callee->type == ToValue::Type::BUILTIN) {
        return callee->builtinVal(args);
    }

    if (callee->type == ToValue::Type::FUNCTION) {
        auto& func = callee->funcVal;
        if (args.size() != func->params.size()) {
            throw ToRuntimeError("Function '" + func->name + "' expects " +
                std::to_string(func->params.size()) + " arguments, got " +
                std::to_string(args.size()), line);
        }
        // Type-check arguments if hints are present
        for (size_t i = 0; i < func->paramTypes.size(); i++) {
            auto& hint = func->paramTypes[i];
            if (!hint.empty()) {
                auto& val = args[i];
                bool ok = false;
                if (hint == "int") ok = val->type == ToValue::Type::INT;
                else if (hint == "float" || hint == "number") ok = val->type == ToValue::Type::FLOAT || val->type == ToValue::Type::INT;
                else if (hint == "string" || hint == "str") ok = val->type == ToValue::Type::STRING;
                else if (hint == "bool") ok = val->type == ToValue::Type::BOOL;
                else if (hint == "list") ok = val->type == ToValue::Type::LIST;
                else if (hint == "dict") ok = val->type == ToValue::Type::DICT;
                else if (hint == "any") ok = true;
                else ok = true; // unknown hint = no check
                if (!ok) {
                    throw ToRuntimeError("Function '" + func->name + "' parameter '" +
                        func->params[i] + "' expects " + hint + ", got " + val->typeName(), line);
                }
            }
        }
        pushFrame(func->name, filename, line);
        auto funcEnv = func->closure->createChild();
        for (size_t i = 0; i < func->params.size(); i++) {
            funcEnv->define(func->params[i], args[i]);
        }
        try {
            for (auto& stmt : func->body) {
                execStatement(stmt, funcEnv);
            }
        } catch (ReturnException& e) {
            popFrame();
            // Type-check return value if hint is present

            if (!func->returnTypeHint.empty()) {
                auto& hint = func->returnTypeHint;
                auto& val = e.value;
                bool ok = false;
                if (hint == "int") ok = val->type == ToValue::Type::INT;
                else if (hint == "float" || hint == "number") ok = val->type == ToValue::Type::FLOAT || val->type == ToValue::Type::INT;
                else if (hint == "string" || hint == "str") ok = val->type == ToValue::Type::STRING;
                else if (hint == "bool") ok = val->type == ToValue::Type::BOOL;
                else if (hint == "list") ok = val->type == ToValue::Type::LIST;
                else if (hint == "dict") ok = val->type == ToValue::Type::DICT;
                else if (hint == "none") ok = val->type == ToValue::Type::NONE;
                else if (hint == "any") ok = true;
                else ok = true;
                if (!ok) {
                    throw ToRuntimeError("Function '" + func->name + "' should return " +
                        hint + ", but returned " + val->typeName(), line);
                }
            }
            return e.value;
        } catch (ToRuntimeError& e) {
            if (e.stackTrace.empty()) {
                e.stackTrace = formatStackTrace();
            }
            popFrame();
            throw;
        }
        popFrame();
        return ToValue::makeNone();
    }

    if (callee->type == ToValue::Type::CLASS) {
        // Instantiate class
        auto& klass = callee->classVal;
        auto instance = std::make_shared<ToInstance>();
        instance->klass = klass;
        auto instVal = ToValue::makeInstance(instance);

        // Call init if it exists
        for (auto& m : klass->methods) {
            if (m.name == "init") {
                auto initEnv = klass->closure->createChild();
                initEnv->define("my", instVal);
                if (args.size() != m.params.size()) {
                    throw ToRuntimeError("init() expects " +
                        std::to_string(m.params.size()) + " arguments, got " +
                        std::to_string(args.size()), line);
                }
                for (size_t i = 0; i < m.params.size(); i++) {
                    initEnv->define(m.params[i], args[i]);
                }
                try {
                    for (auto& stmt : m.body) {
                        execStatement(stmt, initEnv);
                    }
                } catch (ReturnException&) {
                    // init shouldn't return, but handle it gracefully
                }
                break;
            }
        }

        return instVal;
    }

    throw ToRuntimeError("Not callable", line);
}

ToValuePtr Interpreter::evalMemberAccess(ASTNodePtr node, EnvPtr env) {
    auto obj = eval(node->object, env);

    // Instance field access
    if (obj->type == ToValue::Type::INSTANCE) {
        auto it = obj->instanceVal->fields.find(node->member);
        if (it != obj->instanceVal->fields.end()) return it->second;
        // Could be a method — return a bound method? For now, error
        throw ToRuntimeError("'" + obj->instanceVal->klass->name +
            "' instance has no field '" + node->member + "'", node->line);
    }

    // Dict access
    if (obj->type == ToValue::Type::DICT) {
        for (auto& pair : obj->dictVal) {
            if (pair.first == node->member) return pair.second;
        }
        throw ToRuntimeError("Dictionary has no key '" + node->member + "'", node->line);
    }

    // List property access
    if (obj->type == ToValue::Type::LIST) {
        if (node->member == "length") return ToValue::makeInt(obj->listVal.size());
    }

    // String property access
    if (obj->type == ToValue::Type::STRING) {
        if (node->member == "length") return ToValue::makeInt(obj->strVal.size());
    }

    throw ToRuntimeError("Cannot access member '" + node->member + "' on " + obj->typeName(), node->line);
}

ToValuePtr Interpreter::evalIndexExpr(ASTNodePtr node, EnvPtr env) {
    auto obj = eval(node->object, env);

    // Check for slice syntax: obj[start..end]
    bool isSlice = (node->indexExpr->type == NodeType::BinaryExpr && node->indexExpr->op == "..");
    if (isSlice) {
        auto startVal = eval(node->indexExpr->left, env);
        auto endVal = eval(node->indexExpr->right, env);
        if (startVal->type != ToValue::Type::INT || endVal->type != ToValue::Type::INT)
            throw ToRuntimeError("Slice bounds must be integers", node->line);
        int64_t s = startVal->intVal, e = endVal->intVal;

        if (obj->type == ToValue::Type::STRING) {
            int64_t len = obj->strVal.size();
            if (s < 0) s += len;
            if (e < 0) e += len;
            if (s < 0) s = 0;
            if (e > len) e = len;
            if (s >= e) return ToValue::makeString("");
            return ToValue::makeString(obj->strVal.substr(s, e - s));
        }
        if (obj->type == ToValue::Type::LIST) {
            int64_t len = obj->listVal.size();
            if (s < 0) s += len;
            if (e < 0) e += len;
            if (s < 0) s = 0;
            if (e > len) e = len;
            std::vector<ToValuePtr> slice;
            for (int64_t i = s; i < e; i++) slice.push_back(obj->listVal[i]);
            return ToValue::makeList(std::move(slice));
        }
        throw ToRuntimeError("Cannot slice " + obj->typeName(), node->line);
    }

    auto idx = eval(node->indexExpr, env);

    if (obj->type == ToValue::Type::LIST) {
        if (idx->type != ToValue::Type::INT)
            throw ToRuntimeError("List index must be an integer", node->line);
        int64_t i = idx->intVal;
        if (i < 0) i += obj->listVal.size();
        if (i < 0 || i >= (int64_t)obj->listVal.size())
            throw ToRuntimeError("Index out of bounds: " + std::to_string(idx->intVal), node->line);
        return obj->listVal[i];
    }
    if (obj->type == ToValue::Type::STRING) {
        if (idx->type != ToValue::Type::INT)
            throw ToRuntimeError("String index must be an integer", node->line);
        int64_t i = idx->intVal;
        if (i < 0) i += obj->strVal.size();
        if (i < 0 || i >= (int64_t)obj->strVal.size())
            throw ToRuntimeError("Index out of bounds", node->line);
        return ToValue::makeString(std::string(1, obj->strVal[i]));
    }
    if (obj->type == ToValue::Type::DICT) {
        if (idx->type != ToValue::Type::STRING)
            throw ToRuntimeError("Dict key must be a string", node->line);
        for (auto& pair : obj->dictVal) {
            if (pair.first == idx->strVal) return pair.second;
        }
        throw ToRuntimeError("Key not found: '" + idx->strVal + "'", node->line);
    }

    throw ToRuntimeError("Cannot index " + obj->typeName(), node->line);
}

ToValuePtr Interpreter::evalStringInterp(ASTNodePtr node, EnvPtr env) {
    std::string result;
    for (auto& elem : node->elements) {
        auto val = eval(elem, env);
        result += val->toString();
    }
    return ToValue::makeString(result);
}

ToValuePtr Interpreter::evalListLiteral(ASTNodePtr node, EnvPtr env) {
    std::vector<ToValuePtr> items;
    for (auto& elem : node->elements) {
        items.push_back(eval(elem, env));
    }
    return ToValue::makeList(std::move(items));
}

ToValuePtr Interpreter::evalDictLiteral(ASTNodePtr node, EnvPtr env) {
    std::vector<std::pair<std::string, ToValuePtr>> entries;
    for (auto& entry : node->entries) {
        entries.push_back({entry.key, eval(entry.value, env)});
    }
    return ToValue::makeDict(std::move(entries));
}

ToValuePtr Interpreter::evalRangeLiteral(ASTNodePtr node, EnvPtr env) {
    auto start = eval(node->rangeStart, env);
    auto end = eval(node->rangeEnd, env);
    if (start->type != ToValue::Type::INT || end->type != ToValue::Type::INT)
        throw ToRuntimeError("Range requires integer bounds", node->line);
    std::vector<ToValuePtr> list;
    for (int64_t i = start->intVal; i < end->intVal; i++) {
        list.push_back(ToValue::makeInt(i));
    }
    return ToValue::makeList(std::move(list));
}

// ========================
// Built-in methods on types
// ========================

ToValuePtr Interpreter::callListMethod(ToValuePtr list, const std::string& method, const std::vector<ToValuePtr>& args) {
    if (method == "add") {
        if (args.size() != 1) throw ToRuntimeError("list.add() takes exactly 1 argument");
        list->listVal.push_back(args[0]);
        return ToValue::makeNone();
    }
    if (method == "remove") {
        if (args.size() != 1) throw ToRuntimeError("list.remove() takes exactly 1 argument");
        if (args[0]->type != ToValue::Type::INT)
            throw ToRuntimeError("list.remove() index must be an integer");
        int64_t idx = args[0]->intVal;
        if (idx < 0) idx += list->listVal.size();
        if (idx < 0 || idx >= (int64_t)list->listVal.size())
            throw ToRuntimeError("list.remove() index out of bounds");
        list->listVal.erase(list->listVal.begin() + idx);
        return ToValue::makeNone();
    }
    if (method == "pop") {
        if (list->listVal.empty()) throw ToRuntimeError("pop from empty list");
        auto val = list->listVal.back();
        list->listVal.pop_back();
        return val;
    }
    if (method == "contains") {
        if (args.size() != 1) throw ToRuntimeError("list.contains() takes exactly 1 argument");
        for (auto& item : list->listVal) {
            if (item->type == args[0]->type) {
                bool match = false;
                switch (item->type) {
                    case ToValue::Type::INT: match = item->intVal == args[0]->intVal; break;
                    case ToValue::Type::STRING: match = item->strVal == args[0]->strVal; break;
                    case ToValue::Type::BOOL: match = item->boolVal == args[0]->boolVal; break;
                    default: break;
                }
                if (match) return ToValue::makeBool(true);
            }
        }
        return ToValue::makeBool(false);
    }
    if (method == "join") {
        std::string sep = "";
        if (!args.empty() && args[0]->type == ToValue::Type::STRING) sep = args[0]->strVal;
        std::string result;
        for (size_t i = 0; i < list->listVal.size(); i++) {
            if (i > 0) result += sep;
            result += list->listVal[i]->toString();
        }
        return ToValue::makeString(result);
    }
    if (method == "reverse") {
        std::reverse(list->listVal.begin(), list->listVal.end());
        return ToValue::makeNone();
    }
    throw ToRuntimeError("List has no method '" + method + "'");
}

ToValuePtr Interpreter::callStringMethod(ToValuePtr str, const std::string& method, const std::vector<ToValuePtr>& args) {
    if (method == "upper") {
        std::string result = str->strVal;
        for (auto& c : result) c = toupper(c);
        return ToValue::makeString(result);
    }
    if (method == "lower") {
        std::string result = str->strVal;
        for (auto& c : result) c = tolower(c);
        return ToValue::makeString(result);
    }
    if (method == "trim") {
        std::string s = str->strVal;
        s.erase(0, s.find_first_not_of(" \t\n\r"));
        s.erase(s.find_last_not_of(" \t\n\r") + 1);
        return ToValue::makeString(s);
    }
    if (method == "split") {
        std::string sep = " ";
        if (!args.empty() && args[0]->type == ToValue::Type::STRING) sep = args[0]->strVal;
        std::vector<ToValuePtr> parts;
        std::string s = str->strVal;
        size_t start = 0;
        size_t pos;
        while ((pos = s.find(sep, start)) != std::string::npos) {
            parts.push_back(ToValue::makeString(s.substr(start, pos - start)));
            start = pos + sep.size();
        }
        parts.push_back(ToValue::makeString(s.substr(start)));
        return ToValue::makeList(std::move(parts));
    }
    if (method == "contains") {
        if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
            throw ToRuntimeError("string.contains() takes exactly 1 string argument");
        return ToValue::makeBool(str->strVal.find(args[0]->strVal) != std::string::npos);
    }
    if (method == "replace") {
        if (args.size() != 2 || args[0]->type != ToValue::Type::STRING || args[1]->type != ToValue::Type::STRING)
            throw ToRuntimeError("string.replace() takes 2 string arguments");
        std::string result = str->strVal;
        std::string from = args[0]->strVal;
        std::string to = args[1]->strVal;
        size_t pos = 0;
        while ((pos = result.find(from, pos)) != std::string::npos) {
            result.replace(pos, from.length(), to);
            pos += to.length();
        }
        return ToValue::makeString(result);
    }
    if (method == "starts_with") {
        if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
            throw ToRuntimeError("string.starts_with() takes 1 string argument");
        return ToValue::makeBool(str->strVal.find(args[0]->strVal) == 0);
    }
    if (method == "ends_with") {
        if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
            throw ToRuntimeError("string.ends_with() takes 1 string argument");
        auto& s = str->strVal;
        auto& suffix = args[0]->strVal;
        if (suffix.size() > s.size()) return ToValue::makeBool(false);
        return ToValue::makeBool(s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0);
    }
    throw ToRuntimeError("String has no method '" + method + "'");
}

ToValuePtr Interpreter::callDictMethod(ToValuePtr dict, const std::string& method, const std::vector<ToValuePtr>& args) {
    if (method == "keys") {
        std::vector<ToValuePtr> keys;
        for (auto& pair : dict->dictVal) {
            keys.push_back(ToValue::makeString(pair.first));
        }
        return ToValue::makeList(std::move(keys));
    }
    if (method == "values") {
        std::vector<ToValuePtr> values;
        for (auto& pair : dict->dictVal) {
            values.push_back(pair.second);
        }
        return ToValue::makeList(std::move(values));
    }
    if (method == "has") {
        if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
            throw ToRuntimeError("dict.has() takes exactly 1 string argument");
        for (auto& pair : dict->dictVal) {
            if (pair.first == args[0]->strVal) return ToValue::makeBool(true);
        }
        return ToValue::makeBool(false);
    }
    throw ToRuntimeError("Dict has no method '" + method + "'");
}

// ========================
// Web Module
// ========================

void Interpreter::registerWebModule(EnvPtr env, Interpreter* interp) {
    auto server = std::make_shared<HttpServer>();
    auto webModule = ToValue::makeDict({});

    // web.serve(port, handler) — start HTTP server
    webModule->dictVal.push_back({"serve", ToValue::makeBuiltin(
        [interp, server](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 2)
                throw ToRuntimeError("web.serve() takes 2 arguments (port, handler)");
            if (args[0]->type != ToValue::Type::INT)
                throw ToRuntimeError("web.serve() port must be an integer");
            if (args[1]->type != ToValue::Type::FUNCTION && args[1]->type != ToValue::Type::BUILTIN)
                throw ToRuntimeError("web.serve() handler must be a function");

            int port = (int)args[0]->intVal;
            auto handlerVal = args[1];

            server->serve(port, [interp, handlerVal](ToValuePtr req) -> ToValuePtr {
                return interp->callFunction(handlerVal, {req}, 0);
            });
            return ToValue::makeNone();
        }
    )});

    // web.json(value) — serialize To value to JSON string
    webModule->dictVal.push_back({"json", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 1) throw ToRuntimeError("web.json() takes 1 argument");
            return ToValue::makeString(jsonStringify(args[0]));
        }
    )});

    // web.parse_json(string) — parse JSON string to To value
    webModule->dictVal.push_back({"parse_json", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("web.parse_json() takes 1 string argument");
            JsonParser parser(args[0]->strVal);
            return parser.parse();
        }
    )});

    env->define("web", webModule);
}

// ========================
// JSON Module
// ========================

void Interpreter::registerJsonModule(EnvPtr env) {
    auto jsonModule = ToValue::makeDict({});

    jsonModule->dictVal.push_back({"stringify", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 1) throw ToRuntimeError("json.stringify() takes 1 argument");
            return ToValue::makeString(jsonStringify(args[0]));
        }
    )});

    jsonModule->dictVal.push_back({"parse", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("json.parse() takes 1 string argument");
            JsonParser parser(args[0]->strVal);
            return parser.parse();
        }
    )});

    env->define("json", jsonModule);
}
