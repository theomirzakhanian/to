// ============================================================
// vm.cpp — the bytecode compiler and virtual machine (`to fast`)
// ------------------------------------------------------------
// Design rule: the VM never reimplements a language semantic. Every
// operator goes through applyBinaryOp, every method through
// callBuiltinMethod, every index through indexGet — the same functions
// the tree-walking interpreter uses. Constructs that are declarative or
// rare (classes, try/catch, imports, pattern matching, generators,
// decorators, async) are handed straight to the interpreter with
// EXEC_AST, so `to fast` runs the whole language rather than a subset
// of it that silently drops the rest.
// ============================================================
#include "vm.h"
#include "interpreter.h"
#include "methods.h"
#include "builtins.h"
#include "error.h"
#include <iostream>
#include <cstdio>

// ========================
// Chunk
// ========================

size_t Chunk::emit(OpCode op, int line) {
    code.push_back((uint8_t)op);
    lines.push_back(line);
    return code.size() - 1;
}

size_t Chunk::emitWithArg(OpCode op, uint16_t arg, int line) {
    size_t pos = emit(op, line);
    code.push_back((uint8_t)(arg >> 8));
    lines.push_back(line);
    code.push_back((uint8_t)(arg & 0xff));
    lines.push_back(line);
    return pos;
}

size_t Chunk::emitJump(OpCode op, int line) {
    emitWithArg(op, 0xffff, line);
    return code.size() - 2;  // offset of the placeholder
}

void Chunk::patchJump(size_t offset) {
    patchJumpTo(offset, code.size());
}

void Chunk::patchJumpTo(size_t offset, size_t target) {
    // Jump operands are absolute byte offsets — simpler to reason about than
    // relative deltas, and a chunk never exceeds 64K instructions in practice.
    if (target > 0xffff) throw ToRuntimeError("Program too large for the bytecode VM");
    code[offset] = (uint8_t)(target >> 8);
    code[offset + 1] = (uint8_t)(target & 0xff);
}

void Chunk::emitLoop(size_t loopStart, int line) {
    emitWithArg(OpCode::LOOP, (uint16_t)loopStart, line);
}

uint16_t Chunk::addConstant(ToValuePtr val) {
    constants.push_back(std::move(val));
    return (uint16_t)(constants.size() - 1);
}

uint16_t Chunk::addName(const std::string& name) {
    for (size_t i = 0; i < names.size(); i++)
        if (names[i] == name) return (uint16_t)i;
    names.push_back(name);
    return (uint16_t)(names.size() - 1);
}

uint16_t Chunk::addAst(ASTNodePtr node) {
    asts.push_back(AstSlot{std::move(node), -1, -1});
    return (uint16_t)(asts.size() - 1);
}

// ========================
// Compiler
// ========================

Compiler::Compiler() : currentChunk(&mainChunk) {}

Chunk Compiler::compile(ASTNodePtr program, bool isTopLevel,
                        const std::string& fnName, size_t arity) {
    mainChunk = Chunk();
    currentChunk = &mainChunk;
    selfName = fnName;
    selfArity = arity;
    loopStack.clear();

    if (program->type == NodeType::Program) {
        compileBlock(program->statements);
    } else {
        compileStatement(program);
    }

    if (isTopLevel) {
        currentChunk->emit(OpCode::HALT, 0);
    } else {
        currentChunk->emit(OpCode::NONE, 0);
        currentChunk->emit(OpCode::RETURN, 0);
    }
    return std::move(mainChunk);
}

void Compiler::compileBlock(const std::vector<ASTNodePtr>& stmts) {
    for (auto& stmt : stmts) compileStatement(stmt);
}

void Compiler::deferToInterpreter(ASTNodePtr node, bool isStatement) {
    uint16_t slot = currentChunk->addAst(node);
    if (isStatement && !loopStack.empty()) loopStack.back().astSlots.push_back(slot);
    currentChunk->emitWithArg(isStatement ? OpCode::EXEC_AST : OpCode::EVAL_AST, slot, node->line);
}

bool Compiler::isTailCall(ASTNodePtr node) const {
    return !selfName.empty() && node && node->type == NodeType::CallExpr &&
           node->callee && node->callee->type == NodeType::Identifier &&
           node->callee->name == selfName && node->arguments.size() == selfArity;
}

void Compiler::compileStatement(ASTNodePtr node) {
    if (!node) return;
    switch (node->type) {
        case NodeType::ExpressionStmt:
            compileExpression(node->value);
            currentChunk->emit(OpCode::POP, node->line);
            break;

        case NodeType::PrintStmt:
            compileExpression(node->value);
            currentChunk->emit(OpCode::PRINT, node->line);
            break;

        case NodeType::Assignment:
            compileAssignment(node);
            break;

        case NodeType::ConstDecl: {
            compileExpression(node->value);
            currentChunk->emitWithArg(OpCode::STORE_CONST, currentChunk->addName(node->name), node->line);
            break;
        }

        case NodeType::IfBlock: {
            // Each branch jumps to a single shared exit once it has run.
            std::vector<size_t> exitJumps;

            compileExpression(node->condition);
            size_t nextBranch = currentChunk->emitJump(OpCode::JUMP_IF_FALSE, node->line);
            compileBlock(node->body);
            exitJumps.push_back(currentChunk->emitJump(OpCode::JUMP, node->line));

            for (auto& branch : node->orBranches) {
                currentChunk->patchJump(nextBranch);
                compileExpression(branch.condition);
                nextBranch = currentChunk->emitJump(OpCode::JUMP_IF_FALSE, node->line);
                compileBlock(branch.body);
                exitJumps.push_back(currentChunk->emitJump(OpCode::JUMP, node->line));
            }

            currentChunk->patchJump(nextBranch);
            if (!node->elseBody.empty()) compileBlock(node->elseBody);
            for (size_t j : exitJumps) currentChunk->patchJump(j);
            break;
        }

        case NodeType::WhileLoop: {
            LoopInfo loop;
            loop.loopStart = currentChunk->code.size();
            loop.scopeDepth = scopeDepth;
            loopStack.push_back(std::move(loop));

            compileExpression(node->condition);
            size_t exitJump = currentChunk->emitJump(OpCode::JUMP_IF_FALSE, node->line);
            compileBlock(node->body);
            currentChunk->emitLoop(loopStack.back().loopStart, node->line);
            currentChunk->patchJump(exitJump);

            auto& info = loopStack.back();
            size_t exitPad = currentChunk->code.size();
            for (size_t bj : info.breakJumps) currentChunk->patchJumpTo(bj, exitPad);
            for (size_t cj : info.continueJumps) currentChunk->patchJumpTo(cj, info.loopStart);
            for (uint16_t slot : info.astSlots) {
                currentChunk->asts[slot].breakTarget = (int)exitPad;
                currentChunk->asts[slot].continueTarget = (int)info.loopStart;
            }
            loopStack.pop_back();
            break;
        }

        case NodeType::ThroughLoop: {
            compileExpression(node->iterable);
            currentChunk->emit(OpCode::GET_ITER, node->line);

            LoopInfo loop;
            loop.loopStart = currentChunk->code.size();
            loop.scopeDepth = scopeDepth;
            loopStack.push_back(std::move(loop));

            currentChunk->emit(OpCode::DUP, node->line);
            currentChunk->emit(OpCode::ITER_NEXT, node->line);
            size_t exitJump = currentChunk->emitJump(OpCode::JUMP_IF_FALSE, node->line);

            // Each pass gets its own scope, so `to adder(): ...` closures made
            // in the body capture this iteration's loop variable, not the last.
            currentChunk->emit(OpCode::SCOPE_BEGIN, node->line);
            scopeDepth++;
            currentChunk->emitWithArg(OpCode::STORE, currentChunk->addName(node->loopVar), node->line);
            compileBlock(node->body);
            scopeDepth--;

            size_t continuePad = currentChunk->code.size();
            currentChunk->emitWithArg(OpCode::SCOPE_TRUNC, scopeDepth, node->line);
            currentChunk->emitLoop(loopStack.back().loopStart, node->line);

            // Break and normal exit share a landing pad: the iterator is still
            // on the stack there and is popped once, by whichever path arrives.
            currentChunk->patchJump(exitJump);
            size_t exitPad = currentChunk->code.size();
            currentChunk->emitWithArg(OpCode::SCOPE_TRUNC, scopeDepth, node->line);

            auto& info = loopStack.back();
            for (size_t bj : info.breakJumps) currentChunk->patchJumpTo(bj, exitPad);
            for (size_t cj : info.continueJumps) currentChunk->patchJumpTo(cj, continuePad);
            for (uint16_t slot : info.astSlots) {
                currentChunk->asts[slot].breakTarget = (int)exitPad;
                currentChunk->asts[slot].continueTarget = (int)continuePad;
            }
            loopStack.pop_back();
            currentChunk->emit(OpCode::POP, node->line);
            break;
        }

        case NodeType::ReturnStmt: {
            if (node->value && isTailCall(node->value)) {
                for (auto& arg : node->value->arguments) compileExpression(arg);
                currentChunk->emitWithArg(OpCode::TAIL_CALL,
                                          (uint16_t)node->value->arguments.size(), node->line);
                break;
            }
            if (node->value) compileExpression(node->value);
            else currentChunk->emit(OpCode::NONE, node->line);
            currentChunk->emit(OpCode::RETURN, node->line);
            break;
        }

        case NodeType::BreakStmt:
            if (loopStack.empty()) throw ToRuntimeError("'break' outside of a loop", node->line);
            loopStack.back().breakJumps.push_back(currentChunk->emitJump(OpCode::JUMP, node->line));
            break;

        case NodeType::ContinueStmt:
            if (loopStack.empty()) throw ToRuntimeError("'continue' outside of a loop", node->line);
            loopStack.back().continueJumps.push_back(currentChunk->emitJump(OpCode::JUMP, node->line));
            break;

        case NodeType::AssertStmt: {
            compileExpression(node->value);
            uint16_t msg = currentChunk->addConstant(
                ToValue::makeString("Assertion failed at line " + std::to_string(node->line)));
            currentChunk->emitWithArg(OpCode::ASSERT, msg, node->line);
            break;
        }

        default:
            // Classes, try/catch, imports, `given`, enums, shapes,
            // destructuring, function definitions (which may carry decorators
            // or a generator body) — the interpreter owns these.
            deferToInterpreter(node, true);
            break;
    }
}

void Compiler::compileAssignment(ASTNodePtr node) {
    auto target = node->target;
    bool compound = !node->assignOp.empty() && node->assignOp != "=";

    // The right-hand side is compiled exactly once, whatever the target is.
    auto emitValue = [&]() {
        if (!compound) {
            compileExpression(node->value);
            return;
        }
        std::string op = node->assignOp.substr(0, 1);
        compileExpression(node->value);
        switch (op[0]) {
            case '+': currentChunk->emit(OpCode::ADD, node->line); break;
            case '-': currentChunk->emit(OpCode::SUB, node->line); break;
            case '*': currentChunk->emit(OpCode::MUL, node->line); break;
            case '/': currentChunk->emit(OpCode::DIV, node->line); break;
            default: break;
        }
    };

    if (target->type == NodeType::Identifier) {
        uint16_t name = currentChunk->addName(target->name);
        if (compound) currentChunk->emitWithArg(OpCode::LOAD, name, node->line);
        emitValue();
        currentChunk->emitWithArg(OpCode::STORE, name, node->line);
        return;
    }

    if (target->type == NodeType::MemberAccess) {
        uint16_t member = currentChunk->addName(target->member);
        compileExpression(target->object);          // [obj]
        if (compound) {
            currentChunk->emit(OpCode::DUP, node->line);                         // [obj, obj]
            currentChunk->emitWithArg(OpCode::GET_MEMBER, member, node->line);   // [obj, old]
        }
        emitValue();                                                            // [obj, val]
        currentChunk->emitWithArg(OpCode::SET_MEMBER, member, node->line);
        return;
    }

    if (target->type == NodeType::IndexExpr) {
        compileExpression(target->object);          // [obj]
        compileExpression(target->indexExpr);       // [obj, idx]
        if (compound) {
            currentChunk->emit(OpCode::DUP2, node->line);       // [obj, idx, obj, idx]
            currentChunk->emit(OpCode::INDEX_GET, node->line);  // [obj, idx, old]
        }
        emitValue();                                // [obj, idx, new]
        currentChunk->emit(OpCode::INDEX_SET, node->line);
        return;
    }

    throw ToRuntimeError("Invalid assignment target", node->line);
}

void Compiler::compileCall(ASTNodePtr node) {
    // obj.method(args) is one instruction, not a member read followed by a
    // call: built-in methods are not first-class values.
    if (node->callee && node->callee->type == NodeType::MemberAccess) {
        compileExpression(node->callee->object);
        for (auto& arg : node->arguments) compileExpression(arg);
        uint16_t name = currentChunk->addName(node->callee->member);
        currentChunk->emitWithArg(
            node->callee->optionalChain ? OpCode::CALL_METHOD_OPT : OpCode::CALL_METHOD,
            name, node->line);
        // The argument count rides along in a second operand.
        currentChunk->code.push_back((uint8_t)node->arguments.size());
        currentChunk->lines.push_back(node->line);
        return;
    }

    compileExpression(node->callee);
    for (auto& arg : node->arguments) compileExpression(arg);
    currentChunk->emitWithArg(OpCode::CALL, (uint16_t)node->arguments.size(), node->line);
}

void Compiler::compileExpression(ASTNodePtr node) {
    if (!node) {
        currentChunk->emit(OpCode::NONE, 0);
        return;
    }
    switch (node->type) {
        case NodeType::IntegerLiteral:
            currentChunk->emitWithArg(OpCode::CONST,
                currentChunk->addConstant(ToValue::makeInt(node->intValue)), node->line);
            break;
        case NodeType::FloatLiteral:
            currentChunk->emitWithArg(OpCode::CONST,
                currentChunk->addConstant(ToValue::makeFloat(node->floatValue)), node->line);
            break;
        case NodeType::StringLiteral:
            currentChunk->emitWithArg(OpCode::CONST,
                currentChunk->addConstant(ToValue::makeString(node->stringValue)), node->line);
            break;
        case NodeType::BoolLiteral:
            currentChunk->emit(node->boolValue ? OpCode::TRUE_ : OpCode::FALSE_, node->line);
            break;
        case NodeType::NoneLiteral:
            currentChunk->emit(OpCode::NONE, node->line);
            break;
        case NodeType::Identifier:
            currentChunk->emitWithArg(OpCode::LOAD, currentChunk->addName(node->name), node->line);
            break;

        case NodeType::BinaryExpr: {
            // `and` and `or` must not evaluate their right side eagerly.
            if (node->op == "and" || node->op == "or") {
                compileExpression(node->left);
                size_t shortCircuit = currentChunk->emitJump(
                    node->op == "and" ? OpCode::JUMP_IF_FALSE_KEEP : OpCode::JUMP_IF_TRUE_KEEP,
                    node->line);
                currentChunk->emit(OpCode::POP, node->line);
                compileExpression(node->right);
                currentChunk->patchJump(shortCircuit);
                break;
            }
            compileExpression(node->left);
            compileExpression(node->right);
            if (node->op == "..") currentChunk->emit(OpCode::MAKE_RANGE, node->line);
            else if (node->op == "+") currentChunk->emit(OpCode::ADD, node->line);
            else if (node->op == "-") currentChunk->emit(OpCode::SUB, node->line);
            else if (node->op == "*") currentChunk->emit(OpCode::MUL, node->line);
            else if (node->op == "/") currentChunk->emit(OpCode::DIV, node->line);
            else if (node->op == "%") currentChunk->emit(OpCode::MOD, node->line);
            else if (node->op == "==") currentChunk->emit(OpCode::EQ, node->line);
            else if (node->op == "!=") currentChunk->emit(OpCode::NEQ, node->line);
            else if (node->op == "<") currentChunk->emit(OpCode::LT, node->line);
            else if (node->op == "<=") currentChunk->emit(OpCode::LTE, node->line);
            else if (node->op == ">") currentChunk->emit(OpCode::GT, node->line);
            else if (node->op == ">=") currentChunk->emit(OpCode::GTE, node->line);
            else throw ToRuntimeError("Unknown operator '" + node->op + "'", node->line);
            break;
        }

        case NodeType::UnaryExpr:
            compileExpression(node->operand);
            if (node->op == "-") currentChunk->emit(OpCode::NEG, node->line);
            else if (node->op == "not") currentChunk->emit(OpCode::NOT, node->line);
            else throw ToRuntimeError("Unknown unary operator '" + node->op + "'", node->line);
            break;

        case NodeType::CallExpr:
            compileCall(node);
            break;

        case NodeType::MemberAccess:
            compileExpression(node->object);
            currentChunk->emitWithArg(
                node->optionalChain ? OpCode::GET_MEMBER_OPT : OpCode::GET_MEMBER,
                currentChunk->addName(node->member), node->line);
            break;

        case NodeType::IndexExpr:
            compileExpression(node->object);
            // xs[a..b] is a slice, not a lookup by a range value.
            if (node->indexExpr->type == NodeType::BinaryExpr && node->indexExpr->op == "..") {
                compileExpression(node->indexExpr->left);
                compileExpression(node->indexExpr->right);
                currentChunk->emit(OpCode::NONE, node->line);
                currentChunk->emit(OpCode::SLICE, node->line);
                break;
            }
            compileExpression(node->indexExpr);
            currentChunk->emit(OpCode::INDEX_GET, node->line);
            break;

        case NodeType::SliceExpr:
            compileExpression(node->object);
            if (node->rangeStart) compileExpression(node->rangeStart);
            else currentChunk->emit(OpCode::NONE, node->line);
            if (node->rangeEnd) compileExpression(node->rangeEnd);
            else currentChunk->emit(OpCode::NONE, node->line);
            if (node->indexExpr) compileExpression(node->indexExpr);
            else currentChunk->emit(OpCode::NONE, node->line);
            currentChunk->emit(OpCode::SLICE, node->line);
            break;

        case NodeType::ListLiteral:
            for (auto& e : node->elements) compileExpression(e);
            currentChunk->emitWithArg(OpCode::MAKE_LIST, (uint16_t)node->elements.size(), node->line);
            break;

        case NodeType::TupleLiteral:
            for (auto& e : node->elements) compileExpression(e);
            currentChunk->emitWithArg(OpCode::MAKE_TUPLE, (uint16_t)node->elements.size(), node->line);
            break;

        case NodeType::SetLiteral:
            for (auto& e : node->elements) compileExpression(e);
            currentChunk->emitWithArg(OpCode::MAKE_SET, (uint16_t)node->elements.size(), node->line);
            break;

        case NodeType::DictLiteral:
            for (auto& entry : node->entries) {
                if (entry.keyExpr) compileExpression(entry.keyExpr);
                else currentChunk->emitWithArg(OpCode::CONST,
                        currentChunk->addConstant(ToValue::makeString(entry.key)), node->line);
                compileExpression(entry.value);
            }
            currentChunk->emitWithArg(OpCode::MAKE_DICT, (uint16_t)node->entries.size(), node->line);
            break;

        case NodeType::StringInterpolation:
            if (node->elements.empty()) {
                currentChunk->emitWithArg(OpCode::CONST,
                    currentChunk->addConstant(ToValue::makeString("")), node->line);
                break;
            }
            for (size_t i = 0; i < node->elements.size(); i++) {
                compileExpression(node->elements[i]);
                if (i > 0) currentChunk->emit(OpCode::CONCAT, node->line);
            }
            break;

        default:
            // Lambdas, async/await, pipes — evaluated by the interpreter in
            // this frame's scope, so closures and futures behave identically.
            deferToInterpreter(node, false);
            break;
    }
}

// ========================
// VM
// ========================

VM::VM() : stack(STACK_MAX), frames(FRAMES_MAX), globalEnv(std::make_shared<Environment>()) {
    registerBuiltins(globalEnv);
    treeWalker = std::make_shared<Interpreter>("<vm>");
}

VM::~VM() = default;

void VM::push(ToValuePtr val) {
    if (stackTop >= STACK_MAX) throw ToRuntimeError("Stack overflow");
    stack[stackTop++] = std::move(val);
}

ToValuePtr VM::pop() {
    if (stackTop == 0) throw ToRuntimeError("Stack underflow");
    return std::move(stack[--stackTop]);
}

ToValuePtr& VM::peek(int offset) {
    if (stackTop <= (size_t)offset) throw ToRuntimeError("Stack underflow");
    return stack[stackTop - 1 - offset];
}

bool VM::needsInterpreter(const std::shared_ptr<ToFunction>& func) {
    if (func->isGenerator) return true;
    if (!func->returnTypeHint.empty()) return true;
    for (auto& hint : func->paramTypes)
        if (!hint.empty()) return true;
    return false;
}

Chunk* VM::chunkFor(const std::shared_ptr<ToFunction>& func) {
    if (func->compiledChunk) return static_cast<Chunk*>(func->compiledChunk.get());

    Compiler compiler;
    auto body = ASTNode::makeProgram(func->body);
    auto chunk = std::make_shared<Chunk>(
        compiler.compile(body, false, func->name, func->params.size()));
    func->compiledChunk = chunk;
    return chunk.get();
}

void VM::run(Chunk& chunk) {
    stackTop = 0;
    frameCount = 0;
    frames[frameCount++] = CallFrame{&chunk, 0, 0, globalEnv, nullptr, {}};
    execute();
}

void VM::execute() {
    while (true) {
        CallFrame* frame = &frames[frameCount - 1];
        Chunk* chunk = frame->chunk;
        auto& code = chunk->code;

        // Reads the two-byte operand that follows the opcode.
        auto readArg = [&]() -> uint16_t {
            uint16_t hi = code[frame->ip++];
            uint16_t lo = code[frame->ip++];
            return (uint16_t)((hi << 8) | lo);
        };

        // Unwinds the current frame, leaving `result` where the caller expects it.
        // Returns false once the outermost frame has returned.
        auto returnFrom = [&](ToValuePtr result) -> bool {
            size_t base = frame->stackBase;
            frameCount--;
            stackTop = base;
            if (frameCount == 0) return false;
            push(std::move(result));
            return true;
        };

        size_t opOffset = frame->ip;
        OpCode op = (OpCode)code[frame->ip++];

        try {
            switch (op) {
                case OpCode::CONST:   push(chunk->constants[readArg()]); break;
                case OpCode::POP:     pop(); break;
                case OpCode::DUP:     push(peek()); break;
                case OpCode::DUP2: {
                    auto under = peek(1);
                    auto over = peek(0);
                    push(std::move(under));
                    push(std::move(over));
                    break;
                }
                case OpCode::NONE:    push(ToValue::makeNone()); break;
                case OpCode::TRUE_:   push(ToValue::makeBool(true)); break;
                case OpCode::FALSE_:  push(ToValue::makeBool(false)); break;

                case OpCode::LOAD: {
                    const std::string& name = chunk->names[readArg()];
                    auto val = frame->env->get(name);
                    if (!val) throw ToRuntimeError("Undefined variable '" + name + "'",
                                                   chunk->lineAt(opOffset));
                    push(std::move(val));
                    break;
                }
                case OpCode::STORE:
                    frame->env->set(chunk->names[readArg()], pop());
                    break;
                case OpCode::STORE_CONST:
                    frame->env->defineConst(chunk->names[readArg()], pop());
                    break;

                case OpCode::ADD: case OpCode::SUB: case OpCode::MUL:
                case OpCode::DIV: case OpCode::MOD:
                case OpCode::EQ:  case OpCode::NEQ: case OpCode::LT:
                case OpCode::LTE: case OpCode::GT:  case OpCode::GTE: {
                    // The opcodes are laid out to map straight onto BinOp.
                    BinOp tag = op <= OpCode::MOD
                        ? (BinOp)((int)op - (int)OpCode::ADD)
                        : (BinOp)((int)op - (int)OpCode::EQ + (int)BinOp::Eq);
                    auto right = pop();
                    auto left = pop();
                    push(applyBinaryOp(tag, left, right, treeWalker.get(),
                                       chunk->lineAt(opOffset)));
                    break;
                }
                case OpCode::NEG:
                    push(applyUnaryOp("-", pop(), chunk->lineAt(opOffset)));
                    break;
                case OpCode::NOT:
                    push(ToValue::makeBool(!pop()->isTruthy()));
                    break;
                case OpCode::CONCAT: {
                    auto right = pop();
                    auto left = pop();
                    push(ToValue::makeString(left->toString() + right->toString()));
                    break;
                }

                case OpCode::JUMP: frame->ip = readArg(); break;
                case OpCode::LOOP: frame->ip = readArg(); break;
                case OpCode::JUMP_IF_FALSE: {
                    uint16_t target = readArg();
                    if (!pop()->isTruthy()) frame->ip = target;
                    break;
                }
                case OpCode::JUMP_IF_FALSE_KEEP: {
                    uint16_t target = readArg();
                    if (!peek()->isTruthy()) frame->ip = target;
                    break;
                }
                case OpCode::JUMP_IF_TRUE_KEEP: {
                    uint16_t target = readArg();
                    if (peek()->isTruthy()) frame->ip = target;
                    break;
                }

                case OpCode::MAKE_LIST: case OpCode::MAKE_TUPLE: case OpCode::MAKE_SET: {
                    uint16_t count = readArg();
                    std::vector<ToValuePtr> items(count);
                    for (int i = count - 1; i >= 0; i--) items[i] = pop();
                    push(op == OpCode::MAKE_LIST  ? ToValue::makeList(std::move(items))
                       : op == OpCode::MAKE_TUPLE ? ToValue::makeTuple(std::move(items))
                                                  : ToValue::makeSet(std::move(items)));
                    break;
                }
                case OpCode::MAKE_DICT: {
                    uint16_t count = readArg();
                    std::vector<ToValuePtr> flat(count * 2);
                    for (int i = count * 2 - 1; i >= 0; i--) flat[i] = pop();
                    ToDict dict;
                    dict.reserve(count);
                    for (uint16_t i = 0; i < count; i++) dict.setKey(flat[i * 2], flat[i * 2 + 1]);
                    push(ToValue::makeDict(std::move(dict)));
                    break;
                }
                case OpCode::MAKE_RANGE: {
                    auto end = pop();
                    auto start = pop();
                    if (start->type != ToValue::Type::INT || end->type != ToValue::Type::INT)
                        throw ToRuntimeError("Range operator (..) requires integers",
                                             chunk->lineAt(opOffset));
                    std::vector<ToValuePtr> items;
                    if (end->intVal > start->intVal)
                        items.reserve((size_t)(end->intVal - start->intVal));
                    for (int64_t i = start->intVal; i < end->intVal; i++)
                        items.push_back(ToValue::makeInt(i));
                    push(ToValue::makeList(std::move(items)));
                    break;
                }

                case OpCode::INDEX_GET: {
                    auto index = pop();
                    auto obj = pop();
                    push(indexGet(obj, index, chunk->lineAt(opOffset)));
                    break;
                }
                case OpCode::INDEX_SET: {
                    auto val = pop();
                    auto index = pop();
                    auto obj = pop();
                    indexSet(obj, index, std::move(val), chunk->lineAt(opOffset));
                    break;
                }
                case OpCode::SLICE: {
                    auto step = pop();
                    auto end = pop();
                    auto start = pop();
                    auto obj = pop();
                    push(sliceValue(obj, start, end, step, chunk->lineAt(opOffset)));
                    break;
                }

                case OpCode::GET_MEMBER: case OpCode::GET_MEMBER_OPT: {
                    const std::string& name = chunk->names[readArg()];
                    auto obj = pop();
                    if (op == OpCode::GET_MEMBER_OPT && obj->type == ToValue::Type::NONE) {
                        push(ToValue::makeNone());
                        break;
                    }
                    auto prop = getBuiltinProperty(obj, name);
                    if (prop) { push(std::move(prop)); break; }
                    if (obj->type == ToValue::Type::DICT)
                        throw ToRuntimeError("This dict has no key '" + name +
                                             "' — use get(\"" + name + "\", fallback) when it "
                                             "might be missing", chunk->lineAt(opOffset));
                    if (obj->length() >= 0)
                        reportUnknownMember(obj, name, chunk->lineAt(opOffset));
                    if (obj->type == ToValue::Type::INSTANCE)
                        throw ToRuntimeError("'" + obj->instanceVal->klass->name +
                                             "' instance has no field '" + name + "'",
                                             chunk->lineAt(opOffset));
                    throw ToRuntimeError("Cannot access member '" + name + "' on " +
                                         obj->typeName(), chunk->lineAt(opOffset));
                }
                case OpCode::SET_MEMBER: {
                    const std::string& name = chunk->names[readArg()];
                    auto val = pop();
                    auto obj = pop();
                    if (obj->type == ToValue::Type::INSTANCE)
                        obj->instanceVal->fields[name] = std::move(val);
                    else if (obj->type == ToValue::Type::DICT)
                        obj->dictVal.set(name, std::move(val));
                    else
                        throw ToRuntimeError("Cannot set member '" + name + "' on " +
                                             obj->typeName(), chunk->lineAt(opOffset));
                    break;
                }

                case OpCode::GET_ITER: {
                    auto iterable = pop();
                    if (iterable->type == ToValue::Type::GENERATOR) {
                        // A one-element iterator marks the lazy case.
                        push(ToValue::makeList({iterable}));
                        break;
                    }
                    if (iterable->length() < 0)
                        throw ToRuntimeError("Cannot iterate over " + iterable->typeName(),
                                             chunk->lineAt(opOffset));
                    push(ToValue::makeList({ToValue::makeList(iterable->elements()),
                                            ToValue::makeInt(0)}));
                    break;
                }
                case OpCode::ITER_NEXT: {
                    auto iter = pop();
                    if (iter->listVal.size() == 1) {
                        auto gen = iter->listVal[0];
                        auto value = treeWalker->nextGeneratorValue(gen);
                        if (gen->generatorVal->state->done &&
                            !gen->generatorVal->state->hasValue) {
                            push(ToValue::makeBool(false));
                        } else {
                            push(std::move(value));
                            push(ToValue::makeBool(true));
                        }
                        break;
                    }
                    auto& items = iter->listVal[0]->listVal;
                    int64_t idx = iter->listVal[1]->intVal;
                    if (idx < (int64_t)items.size()) {
                        push(items[idx]);
                        iter->listVal[1] = ToValue::makeInt(idx + 1);
                        push(ToValue::makeBool(true));
                    } else {
                        push(ToValue::makeBool(false));
                    }
                    break;
                }

                case OpCode::PRINT:
                    std::cout << pop()->toString() << std::endl;
                    break;

                case OpCode::ASSERT: {
                    uint16_t msg = readArg();
                    if (!pop()->isTruthy())
                        throw ToRuntimeError(chunk->constants[msg]->strVal, chunk->lineAt(opOffset));
                    break;
                }

                case OpCode::CALL: {
                    uint16_t argc = readArg();
                    std::vector<ToValuePtr> args(argc);
                    for (int i = argc - 1; i >= 0; i--) args[i] = pop();
                    auto callee = pop();
                    int line = chunk->lineAt(opOffset);

                    if (callee->type == ToValue::Type::BUILTIN) {
                        push(callee->builtinVal(std::move(args)));
                        break;
                    }
                    if (callee->type == ToValue::Type::FUNCTION && !needsInterpreter(callee->funcVal)) {
                        auto& func = callee->funcVal;
                        if (args.size() != func->params.size())
                            throw ToRuntimeError("Function '" + func->name + "' expects " +
                                std::to_string(func->params.size()) + " arguments, got " +
                                std::to_string(args.size()), line);
                        if (frameCount >= FRAMES_MAX) throw ToRuntimeError("Call stack overflow", line);
                        Chunk* target = chunkFor(func);
                        auto funcEnv = (func->closure ? func->closure : globalEnv)->createChild();
                        for (size_t i = 0; i < func->params.size(); i++)
                            funcEnv->define(func->params[i], std::move(args[i]));
                        frames[frameCount++] = CallFrame{target, 0, stackTop, funcEnv, func, {}};
                        break;
                    }
                    // Generators, typed functions, classes and anything else
                    // keep the interpreter's exact behaviour.
                    push(treeWalker->callFunction(callee, args, line));
                    break;
                }

                case OpCode::SCOPE_BEGIN:
                    frame->scopes.push_back(frame->env);
                    frame->env = frame->env->createChild();
                    break;

                case OpCode::SCOPE_TRUNC: {
                    uint16_t depth = readArg();
                    if (frame->scopes.size() > depth) {
                        frame->env = frame->scopes[depth];
                        frame->scopes.resize(depth);
                    }
                    break;
                }

                case OpCode::CALL_METHOD: case OpCode::CALL_METHOD_OPT: {
                    uint16_t nameIdx = readArg();
                    uint8_t argc = code[frame->ip++];
                    const std::string& name = chunk->names[nameIdx];
                    std::vector<ToValuePtr> args(argc);
                    for (int i = argc - 1; i >= 0; i--) args[i] = pop();
                    auto recv = pop();
                    int line = chunk->lineAt(opOffset);

                    if (op == OpCode::CALL_METHOD_OPT && recv->type == ToValue::Type::NONE) {
                        push(ToValue::makeNone());
                        break;
                    }
                    if (recv->type == ToValue::Type::GENERATOR) {
                        if (name == "next") { push(treeWalker->nextGeneratorValue(recv)); break; }
                        if (name == "to_list") {
                            std::vector<ToValuePtr> all;
                            while (true) {
                                auto v = treeWalker->nextGeneratorValue(recv);
                                if (v->type == ToValue::Type::NONE &&
                                    recv->generatorVal->state->done) break;
                                all.push_back(std::move(v));
                            }
                            push(ToValue::makeList(std::move(all)));
                            break;
                        }
                        throw ToRuntimeError("Generator has no method '" + name + "'", line);
                    }
                    if (recv->type == ToValue::Type::INSTANCE) {
                        bool found = false;
                        auto result = treeWalker->callInstanceMethod(recv, name, args, line, &found);
                        if (found) { push(std::move(result)); break; }
                        auto it = recv->instanceVal->fields.find(name);
                        if (it != recv->instanceVal->fields.end()) {
                            push(treeWalker->callFunction(it->second, args, line));
                            break;
                        }
                        throw ToRuntimeError("'" + recv->instanceVal->klass->name +
                                             "' has no method '" + name + "'", line);
                    }
                    if (recv->type == ToValue::Type::DICT) {
                        // Module members and stored callables shadow built-ins.
                        auto member = recv->dictVal.get(name);
                        if (member && (member->type == ToValue::Type::BUILTIN ||
                                       member->type == ToValue::Type::FUNCTION)) {
                            push(treeWalker->callFunction(member, args, line));
                            break;
                        }
                    }
                    push(callBuiltinMethod(recv, name, args, treeWalker.get(), line));
                    break;
                }

                case OpCode::TAIL_CALL: {
                    uint16_t argc = readArg();
                    std::vector<ToValuePtr> args(argc);
                    for (int i = argc - 1; i >= 0; i--) args[i] = pop();
                    auto& func = frame->func;
                    // Rebind the parameters in a fresh scope and restart the body,
                    // so self-recursion runs in constant stack space.
                    auto funcEnv = (func->closure ? func->closure : globalEnv)->createChild();
                    for (size_t i = 0; i < func->params.size() && i < args.size(); i++)
                        funcEnv->define(func->params[i], std::move(args[i]));
                    frame->env = funcEnv;
                    frame->ip = 0;
                    stackTop = frame->stackBase;
                    break;
                }

                case OpCode::RETURN:
                    if (!returnFrom(pop())) return;
                    break;

                case OpCode::EXEC_AST: {
                    uint16_t slot = readArg();
                    auto& entry = chunk->asts[slot];
                    size_t depth = stackTop;
                    try {
                        treeWalker->execStatement(entry.node, frame->env);
                    } catch (BreakSignal&) {
                        stackTop = depth;
                        if (entry.breakTarget < 0) throw;
                        frame->ip = (size_t)entry.breakTarget;
                    } catch (ContinueSignal&) {
                        stackTop = depth;
                        if (entry.continueTarget < 0) throw;
                        frame->ip = (size_t)entry.continueTarget;
                    } catch (ReturnException& e) {
                        stackTop = depth;
                        if (!returnFrom(e.value)) return;
                    }
                    break;
                }
                case OpCode::EVAL_AST: {
                    uint16_t slot = readArg();
                    push(treeWalker->eval(chunk->asts[slot].node, frame->env));
                    break;
                }

                case OpCode::HALT:
                    return;

                default:
                    throw ToRuntimeError("Unknown opcode " + std::to_string((int)op),
                                         chunk->lineAt(opOffset));
            }
        } catch (ToRuntimeError& e) {
            if (e.line == 0) {
                ToRuntimeError located(e.detail.empty() ? e.what() : e.detail,
                                       chunk->lineAt(opOffset));
                throw located;
            }
            throw;
        }
    }
}

// ========================
// Disassembler (to debug the compiler)
// ========================

void Chunk::disassemble(const std::string& name) const {
    std::cout << "== " << name << " ==\n";
    size_t offset = 0;
    while (offset < code.size()) {
        printf("%04zu %4d ", offset, lineAt(offset));
        OpCode op = (OpCode)code[offset];
        auto arg = [&]() { return (uint16_t)((code[offset + 1] << 8) | code[offset + 2]); };
        switch (op) {
            case OpCode::CONST:   printf("CONST %s\n", constants[arg()]->repr().c_str()); offset += 3; break;
            case OpCode::LOAD:    printf("LOAD %s\n", names[arg()].c_str()); offset += 3; break;
            case OpCode::STORE:   printf("STORE %s\n", names[arg()].c_str()); offset += 3; break;
            case OpCode::STORE_CONST: printf("STORE_CONST %s\n", names[arg()].c_str()); offset += 3; break;
            case OpCode::GET_MEMBER: printf("GET_MEMBER %s\n", names[arg()].c_str()); offset += 3; break;
            case OpCode::GET_MEMBER_OPT: printf("GET_MEMBER_OPT %s\n", names[arg()].c_str()); offset += 3; break;
            case OpCode::SET_MEMBER: printf("SET_MEMBER %s\n", names[arg()].c_str()); offset += 3; break;
            case OpCode::CALL:    printf("CALL %d\n", arg()); offset += 3; break;
            case OpCode::TAIL_CALL: printf("TAIL_CALL %d\n", arg()); offset += 3; break;
            case OpCode::CALL_METHOD:
                printf("CALL_METHOD %s/%d\n", names[arg()].c_str(), code[offset + 3]);
                offset += 4; break;
            case OpCode::JUMP:    printf("JUMP -> %d\n", arg()); offset += 3; break;
            case OpCode::LOOP:    printf("LOOP -> %d\n", arg()); offset += 3; break;
            case OpCode::JUMP_IF_FALSE: printf("JUMP_IF_FALSE -> %d\n", arg()); offset += 3; break;
            case OpCode::JUMP_IF_FALSE_KEEP: printf("JUMP_IF_FALSE_KEEP -> %d\n", arg()); offset += 3; break;
            case OpCode::JUMP_IF_TRUE_KEEP: printf("JUMP_IF_TRUE_KEEP -> %d\n", arg()); offset += 3; break;
            case OpCode::MAKE_LIST:  printf("MAKE_LIST %d\n", arg()); offset += 3; break;
            case OpCode::MAKE_TUPLE: printf("MAKE_TUPLE %d\n", arg()); offset += 3; break;
            case OpCode::MAKE_SET:   printf("MAKE_SET %d\n", arg()); offset += 3; break;
            case OpCode::MAKE_DICT:  printf("MAKE_DICT %d\n", arg()); offset += 3; break;
            case OpCode::ASSERT:     printf("ASSERT\n"); offset += 3; break;
            case OpCode::EXEC_AST:   printf("EXEC_AST %d\n", arg()); offset += 3; break;
            case OpCode::EVAL_AST:   printf("EVAL_AST %d\n", arg()); offset += 3; break;
            default: {
                static const char* simple[] = {"POP", "DUP", "ADD", "SUB", "MUL", "DIV", "MOD",
                                               "NEG", "NOT", "EQ", "NEQ", "LT", "LTE", "GT", "GTE",
                                               "CONCAT", "RETURN", "INDEX_GET", "INDEX_SET",
                                               "SLICE", "GET_ITER", "ITER_NEXT", "PRINT",
                                               "NONE", "TRUE", "FALSE", "MAKE_RANGE", "HALT"};
                (void)simple;
                printf("OP_%d\n", (int)op);
                offset += 1;
                break;
            }
        }
    }
}
