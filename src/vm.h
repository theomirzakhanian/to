#pragma once
#include "environment.h"
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <cstdint>

// ========================
// Bytecode instruction set
// ========================

enum class OpCode : uint8_t {
    // Stack
    CONST,          // push constants[arg]
    POP,            // discard top
    DUP,            // duplicate top
    DUP2,           // duplicate the top two:  [a, b] -> [a, b, a, b]

    // Variables
    LOAD,           // push the value of names[arg]
    STORE,          // pop and assign names[arg]
    STORE_CONST,    // pop and define names[arg] as a constant

    // Arithmetic and comparison — all routed through applyBinaryOp
    ADD, SUB, MUL, DIV, MOD, NEG, NOT,
    EQ, NEQ, LT, LTE, GT, GTE,

    CONCAT,         // string interpolation: join two values as text

    // Control flow
    JUMP,               // unconditional
    JUMP_IF_FALSE,      // pop, jump when falsy
    JUMP_IF_FALSE_KEEP, // peek, jump when falsy, leave the value  (for `and`)
    JUMP_IF_TRUE_KEEP,  // peek, jump when truthy, leave the value (for `or`)
    LOOP,               // backward jump

    // Calls
    CALL,            // call a value:   [callee, args...] -> result
    CALL_METHOD,     // call a method:  [receiver, args...] -> result
    CALL_METHOD_OPT, // same, but `receiver?.m()` yields none for a none receiver
    TAIL_CALL,       // self-recursive tail call: reuse the current frame
    RETURN,

    // Scopes — a `through` loop gives each iteration its own scope so a
    // closure made inside the body captures that iteration's value.
    SCOPE_BEGIN,
    SCOPE_TRUNC,    // drop scopes back down to depth arg

    // Constructors
    MAKE_LIST, MAKE_TUPLE, MAKE_SET, MAKE_DICT, MAKE_RANGE,

    // Access
    INDEX_GET, INDEX_SET,
    SLICE,          // [obj, start, end, step] -> slice
    GET_MEMBER, GET_MEMBER_OPT, SET_MEMBER,

    // Iteration
    GET_ITER,       // [iterable] -> iterator
    ITER_NEXT,      // [iterator] -> [value, true] | [false]

    // Statements
    PRINT,
    ASSERT,

    // Constants
    NONE, TRUE_, FALSE_,

    // Hand a subtree to the tree-walking interpreter. Used for the parts of
    // the language that are declarative or rare enough that compiling them
    // would add risk without adding speed — classes, try/catch, imports,
    // pattern matching, generators, decorators, async.
    EXEC_AST,       // run a statement in the current scope
    EVAL_AST,       // evaluate an expression in the current scope, push it

    HALT,
};

// A statement handed to the tree-walker, plus where to resume if that
// statement breaks or continues the loop it sits in.
struct AstSlot {
    ASTNodePtr node;
    int breakTarget = -1;    // byte offset, or -1 to let the signal escape
    int continueTarget = -1;
};

// ========================
// Chunk — bytecode plus everything it refers to
// ========================

struct Chunk {
    std::vector<uint8_t> code;
    std::vector<ToValuePtr> constants;
    std::vector<std::string> names;
    std::vector<int> lines;          // source line per byte, for error messages
    std::vector<AstSlot> asts;

    size_t emit(OpCode op, int line);
    size_t emitWithArg(OpCode op, uint16_t arg, int line);
    size_t emitJump(OpCode op, int line);   // returns the offset to patch
    void patchJump(size_t offset);
    void patchJumpTo(size_t offset, size_t target);
    void emitLoop(size_t loopStart, int line);

    uint16_t addConstant(ToValuePtr val);
    uint16_t addName(const std::string& name);
    uint16_t addAst(ASTNodePtr node);

    int lineAt(size_t offset) const { return offset < lines.size() ? lines[offset] : 0; }
    void disassemble(const std::string& name) const;
};

// ========================
// Compiler — AST to bytecode
// ========================

class Compiler {
public:
    Compiler();

    // `selfName` enables tail-call optimisation for a function compiling its
    // own body; leave it empty at the top level.
    Chunk compile(ASTNodePtr program, bool isTopLevel = true,
                  const std::string& selfName = "", size_t selfArity = 0);

private:
    Chunk* currentChunk;
    Chunk mainChunk;
    std::string selfName;
    size_t selfArity = 0;

    struct LoopInfo {
        size_t loopStart;
        std::vector<size_t> breakJumps;
        std::vector<size_t> continueJumps;
        std::vector<uint16_t> astSlots;  // EXEC_AST slots that may break/continue us
        uint16_t scopeDepth = 0;         // scope nesting outside this loop's body
    };
    std::vector<LoopInfo> loopStack;
    uint16_t scopeDepth = 0;

    void compileStatement(ASTNodePtr node);
    void compileExpression(ASTNodePtr node);
    void compileBlock(const std::vector<ASTNodePtr>& stmts);
    void compileAssignment(ASTNodePtr node);
    void compileCall(ASTNodePtr node);
    // Emit EXEC_AST/EVAL_AST for a subtree the compiler does not handle.
    void deferToInterpreter(ASTNodePtr node, bool isStatement);
    bool isTailCall(ASTNodePtr node) const;
};

// ========================
// VM
// ========================

struct CallFrame {
    Chunk* chunk;
    size_t ip;
    size_t stackBase;                  // where this frame's result belongs
    EnvPtr env;
    std::shared_ptr<ToFunction> func;  // set for function frames, for tail calls
    std::vector<EnvPtr> scopes;        // enclosing scopes, innermost last
};

class VM {
public:
    VM();
    ~VM();

    void run(Chunk& chunk);

    EnvPtr getGlobalEnv() { return globalEnv; }

private:
    static constexpr size_t STACK_MAX = 65536;
    static constexpr size_t FRAMES_MAX = 1024;

    std::vector<ToValuePtr> stack;
    size_t stackTop = 0;

    std::vector<CallFrame> frames;
    size_t frameCount = 0;

    EnvPtr globalEnv;
    std::shared_ptr<class Interpreter> treeWalker;

    void push(ToValuePtr val);
    ToValuePtr pop();
    ToValuePtr& peek(int offset = 0);

    Chunk* chunkFor(const std::shared_ptr<ToFunction>& func);
    // True when a function must run on the tree-walker to keep its semantics.
    static bool needsInterpreter(const std::shared_ptr<ToFunction>& func);

    void execute();
};
