#pragma once
#include "ast.h"
#include "environment.h"
#include <string>
#include <vector>
#include <mutex>

// Stack frame for stack traces
struct StackFrame {
    std::string functionName;
    std::string filename;
    int line;
};

class Interpreter {
public:
    Interpreter(const std::string& filename = "<stdin>");

    void execute(ASTNodePtr program);
    ToValuePtr eval(ASTNodePtr node, EnvPtr env);

    EnvPtr getGlobalEnv() { return globalEnv; }
    ToValuePtr callFunction(ToValuePtr callee, const std::vector<ToValuePtr>& args, int line);

    // Generator support
    ToValuePtr createGenerator(std::shared_ptr<ToFunction> func, const std::vector<ToValuePtr>& args);
    ToValuePtr nextGeneratorValue(ToValuePtr gen);

    // Call stack for stack traces (mutex-protected for async)
    std::vector<StackFrame> callStack;
    std::mutex callStackMutex;
    void pushFrame(const std::string& name, const std::string& file, int line);
    void popFrame();
    std::string formatStackTrace() const;

    // Debugger support
    bool debugMode = false;
    int breakLine = -1; // -1 = no breakpoint
    std::vector<int> breakpoints;
    bool stepMode = false;
    void debugPrompt(ASTNodePtr node, EnvPtr env);

    // Statement execution (public for VM hybrid calls)
    void execStatement(ASTNodePtr node, EnvPtr env);

private:
    std::string filename;
    EnvPtr globalEnv;
    void execPrint(ASTNodePtr node, EnvPtr env);
    void execAssignment(ASTNodePtr node, EnvPtr env);
    void execConstDecl(ASTNodePtr node, EnvPtr env);
    void execIf(ASTNodePtr node, EnvPtr env);
    void execGiven(ASTNodePtr node, EnvPtr env);
    void execWhile(ASTNodePtr node, EnvPtr env);
    void execThrough(ASTNodePtr node, EnvPtr env);
    void execFunctionDef(ASTNodePtr node, EnvPtr env);
    void execClassDef(ASTNodePtr node, EnvPtr env);
    void execTryCatch(ASTNodePtr node, EnvPtr env);
    void execImport(ASTNodePtr node, EnvPtr env);
    void execBlock(const std::vector<ASTNodePtr>& stmts, EnvPtr env);

    // Expression evaluation
    ToValuePtr evalBinaryExpr(ASTNodePtr node, EnvPtr env);
    ToValuePtr evalUnaryExpr(ASTNodePtr node, EnvPtr env);
    ToValuePtr evalCallExpr(ASTNodePtr node, EnvPtr env);
    ToValuePtr evalMemberAccess(ASTNodePtr node, EnvPtr env);
    ToValuePtr evalIndexExpr(ASTNodePtr node, EnvPtr env);
    ToValuePtr evalStringInterp(ASTNodePtr node, EnvPtr env);
    ToValuePtr evalListLiteral(ASTNodePtr node, EnvPtr env);
    ToValuePtr evalDictLiteral(ASTNodePtr node, EnvPtr env);
    ToValuePtr evalRangeLiteral(ASTNodePtr node, EnvPtr env);

    // Member method handling
    ToValuePtr callListMethod(ToValuePtr list, const std::string& method, const std::vector<ToValuePtr>& args);
    ToValuePtr callStringMethod(ToValuePtr str, const std::string& method, const std::vector<ToValuePtr>& args);
    ToValuePtr callDictMethod(ToValuePtr dict, const std::string& method, const std::vector<ToValuePtr>& args);

    // Module registration
    void registerWebModule(EnvPtr env, Interpreter* interp);
    void registerJsonModule(EnvPtr env);
};

// Return signal for function returns
class ReturnException : public std::exception {
public:
    ToValuePtr value;
    explicit ReturnException(ToValuePtr v) : value(std::move(v)) {}
};

// Tail call signal for TCO
class TailCallSignal : public std::exception {
public:
    std::vector<ToValuePtr> args;
    explicit TailCallSignal(std::vector<ToValuePtr> a) : args(std::move(a)) {}
};

// Yield signal (raised inside a generator function body)
class YieldSignal : public std::exception {
public:
    ToValuePtr value;
    explicit YieldSignal(ToValuePtr v) : value(std::move(v)) {}
};
