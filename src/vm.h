#pragma once
#include "environment.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

// ========================
// Bytecode instruction set
// ========================

enum class OpCode : uint8_t {
    // Stack ops
    CONST,          // push constant from pool
    POP,            // discard top of stack
    DUP,            // duplicate top

    // Variables
    LOAD,           // push variable value (index into names)
    STORE,          // pop value, store in variable
    LOAD_CONST_VAR, // load a const variable
    STORE_CONST,    // define a const variable

    // Arithmetic
    ADD, SUB, MUL, DIV, MOD, NEG,

    // Comparison
    EQ, NEQ, LT, LTE, GT, GTE,

    // Logical
    AND, OR, NOT,

    // String
    CONCAT,         // string concatenation for interpolation

    // Control flow
    JUMP,           // unconditional jump (16-bit offset)
    JUMP_IF_FALSE,  // pop, jump if falsy
    JUMP_IF_TRUE,   // pop, jump if truthy
    LOOP,           // backward jump

    // Functions
    CALL,           // call function (arg count follows)
    RETURN,         // return from function
    MAKE_FUNCTION,  // create function from code object

    // Lists & Dicts
    MAKE_LIST,      // create list (count follows)
    MAKE_DICT,      // create dict (count follows, key-value pairs)
    INDEX_GET,      // obj[index]
    INDEX_SET,      // obj[index] = value

    // Member access
    GET_MEMBER,     // obj.member (name index follows)
    SET_MEMBER,     // obj.member = value
    GET_MEMBER_OPT, // obj?.member (optional chaining)

    // Classes
    MAKE_CLASS,     // create class (name, method count)
    MAKE_INSTANCE,  // instantiate class (arg count follows)

    // Iteration
    GET_ITER,       // get iterator from iterable
    ITER_NEXT,      // get next value, push (pushes sentinel on done)
    MAKE_RANGE,     // create range from two ints

    // Built-in ops
    PRINT,          // print top of stack

    // Special
    NONE,           // push none
    TRUE_,          // push true
    FALSE_,         // push false

    HALT,           // end of program
};

// ========================
// Chunk — a sequence of bytecodes + constants
// ========================

struct Chunk {
    std::vector<uint8_t> code;
    std::vector<ToValuePtr> constants;     // constant pool
    std::vector<std::string> names;        // variable/member names
    std::vector<int> lines;                // source line per instruction

    // Emit helpers
    size_t emit(OpCode op, int line);
    size_t emitWithArg(OpCode op, uint16_t arg, int line);
    size_t emitJump(OpCode op, int line);  // returns offset to patch later
    void patchJump(size_t offset);
    void emitLoop(size_t loopStart, int line);

    // Add a constant, return its index
    uint16_t addConstant(ToValuePtr val);
    uint16_t addName(const std::string& name);

    // Debug
    void disassemble(const std::string& name) const;
};

// ========================
// Compiler — AST to bytecode
// ========================

class Compiler {
public:
    Compiler();

    Chunk compile(ASTNodePtr program, bool isTopLevel = true);

private:
    Chunk* currentChunk;
    Chunk mainChunk;

    // Loop tracking for break/continue
    struct LoopInfo {
        size_t loopStart;
        std::vector<size_t> breakJumps;
    };
    std::vector<LoopInfo> loopStack;

    void compileNode(ASTNodePtr node);
    void compileStatement(ASTNodePtr node);
    void compileExpression(ASTNodePtr node);
    void compileBlock(const std::vector<ASTNodePtr>& stmts);
};

// ========================
// VM — executes bytecode
// ========================

// Call frame for the VM
struct CallFrame {
    Chunk* chunk;
    size_t ip;
    size_t stackBase; // stack offset for this frame's locals
    EnvPtr env;       // variable scope for this frame
};

class VM {
public:
    VM();

    void run(Chunk& chunk);

    EnvPtr getGlobalEnv() { return globalEnv; }

    // Compile a function body to a Chunk
    Chunk compileFunction(std::shared_ptr<ToFunction> func);

private:
    static constexpr size_t STACK_MAX = 16384;
    static constexpr size_t FRAMES_MAX = 256;
    ToValuePtr stack[STACK_MAX];
    size_t stackTop = 0;

    CallFrame frames[FRAMES_MAX];
    size_t frameCount = 0;

    EnvPtr globalEnv;
    std::shared_ptr<class Interpreter> treeWalker;

    // Cache compiled functions
    std::unordered_map<std::string, Chunk> functionChunks;

    void push(ToValuePtr val);
    ToValuePtr pop();
    ToValuePtr peek(int offset = 0);

    uint16_t readShort(Chunk& chunk, size_t& ip);
    void executeFrame(); // run current frame
};
