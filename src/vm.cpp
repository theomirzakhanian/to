#include "vm.h"
#include "interpreter.h"
#include "builtins.h"
#include "error.h"
#include "ast.h"
#include <iostream>

// ========================
// Chunk implementation
// ========================

size_t Chunk::emit(OpCode op, int line) {
    size_t offset = code.size();
    code.push_back(static_cast<uint8_t>(op));
    lines.push_back(line);
    return offset;
}

size_t Chunk::emitWithArg(OpCode op, uint16_t arg, int line) {
    size_t offset = code.size();
    code.push_back(static_cast<uint8_t>(op));
    code.push_back((arg >> 8) & 0xFF);
    code.push_back(arg & 0xFF);
    lines.push_back(line);
    lines.push_back(line);
    lines.push_back(line);
    return offset;
}

size_t Chunk::emitJump(OpCode op, int line) {
    size_t offset = code.size();
    code.push_back(static_cast<uint8_t>(op));
    code.push_back(0xFF); // placeholder
    code.push_back(0xFF);
    lines.push_back(line);
    lines.push_back(line);
    lines.push_back(line);
    return offset;
}

void Chunk::patchJump(size_t offset) {
    size_t jump = code.size() - offset - 3;
    if (jump > 0xFFFF) {
        throw ToRuntimeError("Jump too large");
    }
    code[offset + 1] = (jump >> 8) & 0xFF;
    code[offset + 2] = jump & 0xFF;
}

void Chunk::emitLoop(size_t loopStart, int line) {
    code.push_back(static_cast<uint8_t>(OpCode::LOOP));
    size_t offset = code.size() - loopStart + 2;
    code.push_back((offset >> 8) & 0xFF);
    code.push_back(offset & 0xFF);
    lines.push_back(line);
    lines.push_back(line);
    lines.push_back(line);
}

uint16_t Chunk::addConstant(ToValuePtr val) {
    constants.push_back(val);
    return (uint16_t)(constants.size() - 1);
}

uint16_t Chunk::addName(const std::string& name) {
    for (size_t i = 0; i < names.size(); i++) {
        if (names[i] == name) return (uint16_t)i;
    }
    names.push_back(name);
    return (uint16_t)(names.size() - 1);
}

void Chunk::disassemble(const std::string& name) const {
    std::cout << "== " << name << " ==\n";
    size_t offset = 0;
    while (offset < code.size()) {
        auto op = static_cast<OpCode>(code[offset]);
        int line = offset < lines.size() ? lines[offset] : 0;
        printf("%04zu  L%d  ", offset, line);

        switch (op) {
            case OpCode::CONST: {
                uint16_t idx = (code[offset+1] << 8) | code[offset+2];
                std::cout << "CONST " << idx << " (" << constants[idx]->toString() << ")\n";
                offset += 3; break;
            }
            case OpCode::LOAD: {
                uint16_t idx = (code[offset+1] << 8) | code[offset+2];
                std::cout << "LOAD " << names[idx] << "\n";
                offset += 3; break;
            }
            case OpCode::STORE: {
                uint16_t idx = (code[offset+1] << 8) | code[offset+2];
                std::cout << "STORE " << names[idx] << "\n";
                offset += 3; break;
            }
            case OpCode::ADD: std::cout << "ADD\n"; offset++; break;
            case OpCode::SUB: std::cout << "SUB\n"; offset++; break;
            case OpCode::MUL: std::cout << "MUL\n"; offset++; break;
            case OpCode::DIV: std::cout << "DIV\n"; offset++; break;
            case OpCode::MOD: std::cout << "MOD\n"; offset++; break;
            case OpCode::NEG: std::cout << "NEG\n"; offset++; break;
            case OpCode::EQ: std::cout << "EQ\n"; offset++; break;
            case OpCode::NEQ: std::cout << "NEQ\n"; offset++; break;
            case OpCode::LT: std::cout << "LT\n"; offset++; break;
            case OpCode::LTE: std::cout << "LTE\n"; offset++; break;
            case OpCode::GT: std::cout << "GT\n"; offset++; break;
            case OpCode::GTE: std::cout << "GTE\n"; offset++; break;
            case OpCode::NOT: std::cout << "NOT\n"; offset++; break;
            case OpCode::POP: std::cout << "POP\n"; offset++; break;
            case OpCode::PRINT: std::cout << "PRINT\n"; offset++; break;
            case OpCode::NONE: std::cout << "NONE\n"; offset++; break;
            case OpCode::TRUE_: std::cout << "TRUE\n"; offset++; break;
            case OpCode::FALSE_: std::cout << "FALSE\n"; offset++; break;
            case OpCode::RETURN: std::cout << "RETURN\n"; offset++; break;
            case OpCode::HALT: std::cout << "HALT\n"; offset++; break;
            case OpCode::JUMP: {
                uint16_t j = (code[offset+1] << 8) | code[offset+2];
                printf("JUMP +%d -> %zu\n", j, offset + 3 + j);
                offset += 3; break;
            }
            case OpCode::JUMP_IF_FALSE: {
                uint16_t j = (code[offset+1] << 8) | code[offset+2];
                printf("JUMP_IF_FALSE +%d -> %zu\n", j, offset + 3 + j);
                offset += 3; break;
            }
            case OpCode::JUMP_IF_TRUE: {
                uint16_t j = (code[offset+1] << 8) | code[offset+2];
                printf("JUMP_IF_TRUE +%d -> %zu\n", j, offset + 3 + j);
                offset += 3; break;
            }
            case OpCode::LOOP: {
                uint16_t j = (code[offset+1] << 8) | code[offset+2];
                printf("LOOP -%d -> %zu\n", j, offset + 3 - j);
                offset += 3; break;
            }
            case OpCode::CALL: {
                uint16_t argc = (code[offset+1] << 8) | code[offset+2];
                printf("CALL %d\n", argc);
                offset += 3; break;
            }
            case OpCode::MAKE_LIST: {
                uint16_t count = (code[offset+1] << 8) | code[offset+2];
                printf("MAKE_LIST %d\n", count);
                offset += 3; break;
            }
            case OpCode::MAKE_DICT: {
                uint16_t count = (code[offset+1] << 8) | code[offset+2];
                printf("MAKE_DICT %d\n", count);
                offset += 3; break;
            }
            case OpCode::GET_MEMBER: {
                uint16_t idx = (code[offset+1] << 8) | code[offset+2];
                std::cout << "GET_MEMBER " << names[idx] << "\n";
                offset += 3; break;
            }
            case OpCode::SET_MEMBER: {
                uint16_t idx = (code[offset+1] << 8) | code[offset+2];
                std::cout << "SET_MEMBER " << names[idx] << "\n";
                offset += 3; break;
            }
            case OpCode::INDEX_GET: std::cout << "INDEX_GET\n"; offset++; break;
            case OpCode::INDEX_SET: std::cout << "INDEX_SET\n"; offset++; break;
            case OpCode::MAKE_RANGE: std::cout << "MAKE_RANGE\n"; offset++; break;
            case OpCode::CONCAT: std::cout << "CONCAT\n"; offset++; break;
            default:
                printf("UNKNOWN(%d)\n", (int)op);
                offset++;
        }
    }
}

// ========================
// Compiler implementation
// ========================

Compiler::Compiler() : currentChunk(&mainChunk) {}

Chunk Compiler::compile(ASTNodePtr program, bool isTopLevel /*= true*/) {
    mainChunk = Chunk();
    currentChunk = &mainChunk;

    if (program->type == NodeType::Program) {
        compileBlock(program->statements);
    }
    if (isTopLevel) {
        currentChunk->emit(OpCode::HALT, 0);
    } else {
        // Functions need an implicit return none
        currentChunk->emit(OpCode::NONE, 0);
        currentChunk->emit(OpCode::RETURN, 0);
    }
    return mainChunk;
}

void Compiler::compileBlock(const std::vector<ASTNodePtr>& stmts) {
    for (auto& stmt : stmts) {
        compileStatement(stmt);
    }
}

void Compiler::compileStatement(ASTNodePtr node) {
    switch (node->type) {
        case NodeType::PrintStmt: {
            compileExpression(node->value);
            currentChunk->emit(OpCode::PRINT, node->line);
            break;
        }
        case NodeType::ExpressionStmt: {
            compileExpression(node->value);
            currentChunk->emit(OpCode::POP, node->line);
            break;
        }
        case NodeType::Assignment: {
            compileExpression(node->value);

            if (node->target->type == NodeType::Identifier) {
                if (node->assignOp != "=") {
                    // Compound assignment: load old value, compute, store
                    uint16_t nameIdx = currentChunk->addName(node->target->name);
                    // We already compiled the RHS. Now load old value and compute
                    // Actually — recompile: push old, push new, op, store
                    // Reset: pop the value we compiled
                    currentChunk->emit(OpCode::POP, node->line);
                    // Load old
                    currentChunk->emitWithArg(OpCode::LOAD, nameIdx, node->line);
                    // Compile new value again
                    compileExpression(node->value);
                    // Apply op
                    if (node->assignOp == "+=") currentChunk->emit(OpCode::ADD, node->line);
                    else if (node->assignOp == "-=") currentChunk->emit(OpCode::SUB, node->line);
                    else if (node->assignOp == "*=") currentChunk->emit(OpCode::MUL, node->line);
                    else if (node->assignOp == "/=") currentChunk->emit(OpCode::DIV, node->line);
                    currentChunk->emitWithArg(OpCode::STORE, nameIdx, node->line);
                } else {
                    uint16_t nameIdx = currentChunk->addName(node->target->name);
                    currentChunk->emitWithArg(OpCode::STORE, nameIdx, node->line);
                }
            } else if (node->target->type == NodeType::MemberAccess) {
                // obj.member = val  (val already on stack)
                compileExpression(node->target->object);
                // Stack: [val, obj]  — but we need obj first. Rearrange:
                // Actually let's recompile in correct order
                currentChunk->emit(OpCode::POP, node->line); // pop val
                compileExpression(node->target->object);
                compileExpression(node->value); // push val again
                uint16_t memberIdx = currentChunk->addName(node->target->member);
                currentChunk->emitWithArg(OpCode::SET_MEMBER, memberIdx, node->line);
            } else if (node->target->type == NodeType::IndexExpr) {
                currentChunk->emit(OpCode::POP, node->line);
                compileExpression(node->target->object);
                compileExpression(node->target->indexExpr);
                compileExpression(node->value);
                currentChunk->emit(OpCode::INDEX_SET, node->line);
            }
            break;
        }
        case NodeType::ConstDecl: {
            compileExpression(node->value);
            uint16_t nameIdx = currentChunk->addName(node->name);
            currentChunk->emitWithArg(OpCode::STORE_CONST, nameIdx, node->line);
            break;
        }
        case NodeType::IfBlock: {
            compileExpression(node->condition);
            size_t thenJump = currentChunk->emitJump(OpCode::JUMP_IF_FALSE, node->line);
            compileBlock(node->body);

            if (!node->orBranches.empty() || !node->elseBody.empty()) {
                size_t endJump = currentChunk->emitJump(OpCode::JUMP, node->line);
                currentChunk->patchJump(thenJump);

                for (size_t i = 0; i < node->orBranches.size(); i++) {
                    compileExpression(node->orBranches[i].condition);
                    size_t orJump = currentChunk->emitJump(OpCode::JUMP_IF_FALSE, node->line);
                    compileBlock(node->orBranches[i].body);
                    // Need to jump to end after each or-branch
                    // Save this jump to patch later
                    size_t orEnd = currentChunk->emitJump(OpCode::JUMP, node->line);
                    currentChunk->patchJump(orJump);
                    // Patch previous endJump to here? No — chain:
                    // Actually, we need to collect all end jumps and patch them
                    // For simplicity, patch orEnd at the very end
                    currentChunk->patchJump(endJump);
                    endJump = orEnd;
                }

                if (!node->elseBody.empty()) {
                    currentChunk->patchJump(endJump);
                    compileBlock(node->elseBody);
                } else {
                    currentChunk->patchJump(endJump);
                }
            } else {
                currentChunk->patchJump(thenJump);
            }
            break;
        }
        case NodeType::WhileLoop: {
            LoopInfo loop;
            loop.loopStart = currentChunk->code.size();
            loopStack.push_back(loop);

            compileExpression(node->condition);
            size_t exitJump = currentChunk->emitJump(OpCode::JUMP_IF_FALSE, node->line);
            compileBlock(node->body);
            currentChunk->emitLoop(loopStack.back().loopStart, node->line);
            currentChunk->patchJump(exitJump);

            // Patch break jumps
            for (auto bj : loopStack.back().breakJumps) {
                currentChunk->patchJump(bj);
            }
            loopStack.pop_back();
            break;
        }
        case NodeType::ThroughLoop: {
            // Compile iterable
            compileExpression(node->iterable);
            currentChunk->emit(OpCode::GET_ITER, node->line);

            LoopInfo loop;
            loop.loopStart = currentChunk->code.size();
            loopStack.push_back(loop);

            // ITER_NEXT pushes value + done flag
            currentChunk->emit(OpCode::DUP, node->line); // dup iterator
            currentChunk->emit(OpCode::ITER_NEXT, node->line);
            size_t exitJump = currentChunk->emitJump(OpCode::JUMP_IF_FALSE, node->line);

            // Store loop variable
            uint16_t varIdx = currentChunk->addName(node->loopVar);
            currentChunk->emitWithArg(OpCode::STORE, varIdx, node->line);

            compileBlock(node->body);
            currentChunk->emitLoop(loopStack.back().loopStart, node->line);
            currentChunk->patchJump(exitJump);
            currentChunk->emit(OpCode::POP, node->line); // pop iterator

            for (auto bj : loopStack.back().breakJumps) {
                currentChunk->patchJump(bj);
            }
            loopStack.pop_back();
            break;
        }
        case NodeType::ReturnStmt: {
            if (node->value) {
                compileExpression(node->value);
            } else {
                currentChunk->emit(OpCode::NONE, node->line);
            }
            currentChunk->emit(OpCode::RETURN, node->line);
            break;
        }
        case NodeType::BreakStmt: {
            if (!loopStack.empty()) {
                size_t breakJump = currentChunk->emitJump(OpCode::JUMP, node->line);
                loopStack.back().breakJumps.push_back(breakJump);
            }
            break;
        }
        case NodeType::ContinueStmt: {
            if (!loopStack.empty()) {
                currentChunk->emitLoop(loopStack.back().loopStart, node->line);
            }
            break;
        }
        case NodeType::FunctionDef: {
            // For now, function defs are stored as constants
            // The VM will use the tree-walker's function infrastructure
            // Full bytecode functions would need call frames — keep it hybrid for now
            uint16_t nameIdx = currentChunk->addName(node->name);
            auto func = std::make_shared<ToFunction>();
            func->name = node->name;
            func->params = node->params;
            func->paramTypes = node->paramTypes;
            func->returnTypeHint = node->returnTypeHint;
            func->body = node->body;
            uint16_t constIdx = currentChunk->addConstant(ToValue::makeFunction(func));
            currentChunk->emitWithArg(OpCode::CONST, constIdx, node->line);
            currentChunk->emitWithArg(OpCode::STORE, nameIdx, node->line);
            break;
        }
        case NodeType::AssertStmt: {
            compileExpression(node->value);
            // If falsy, throw
            size_t okJump = currentChunk->emitJump(OpCode::JUMP_IF_TRUE, node->line);
            // Push error message as constant
            uint16_t msgIdx = currentChunk->addConstant(
                ToValue::makeString("Assertion failed at line " + std::to_string(node->line)));
            currentChunk->emitWithArg(OpCode::CONST, msgIdx, node->line);
            currentChunk->emit(OpCode::PRINT, node->line);
            currentChunk->emit(OpCode::HALT, node->line); // hard stop on assert fail
            currentChunk->patchJump(okJump);
            break;
        }
        default:
            // For unsupported statements, skip (the tree-walker handles them)
            break;
    }
}

void Compiler::compileExpression(ASTNodePtr node) {
    switch (node->type) {
        case NodeType::IntegerLiteral: {
            uint16_t idx = currentChunk->addConstant(ToValue::makeInt(node->intValue));
            currentChunk->emitWithArg(OpCode::CONST, idx, node->line);
            break;
        }
        case NodeType::FloatLiteral: {
            uint16_t idx = currentChunk->addConstant(ToValue::makeFloat(node->floatValue));
            currentChunk->emitWithArg(OpCode::CONST, idx, node->line);
            break;
        }
        case NodeType::StringLiteral: {
            uint16_t idx = currentChunk->addConstant(ToValue::makeString(node->stringValue));
            currentChunk->emitWithArg(OpCode::CONST, idx, node->line);
            break;
        }
        case NodeType::BoolLiteral:
            currentChunk->emit(node->boolValue ? OpCode::TRUE_ : OpCode::FALSE_, node->line);
            break;
        case NodeType::NoneLiteral:
            currentChunk->emit(OpCode::NONE, node->line);
            break;
        case NodeType::Identifier: {
            uint16_t idx = currentChunk->addName(node->name);
            currentChunk->emitWithArg(OpCode::LOAD, idx, node->line);
            break;
        }
        case NodeType::BinaryExpr: {
            if (node->op == "..") {
                compileExpression(node->left);
                compileExpression(node->right);
                currentChunk->emit(OpCode::MAKE_RANGE, node->line);
                break;
            }
            compileExpression(node->left);
            compileExpression(node->right);
            if (node->op == "+") currentChunk->emit(OpCode::ADD, node->line);
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
            break;
        }
        case NodeType::UnaryExpr: {
            compileExpression(node->operand);
            if (node->op == "-") currentChunk->emit(OpCode::NEG, node->line);
            else if (node->op == "not") currentChunk->emit(OpCode::NOT, node->line);
            break;
        }
        case NodeType::CallExpr: {
            compileExpression(node->callee);
            for (auto& arg : node->arguments) {
                compileExpression(arg);
            }
            currentChunk->emitWithArg(OpCode::CALL, (uint16_t)node->arguments.size(), node->line);
            break;
        }
        case NodeType::MemberAccess: {
            compileExpression(node->object);
            uint16_t memberIdx = currentChunk->addName(node->member);
            if (node->optionalChain) {
                currentChunk->emitWithArg(OpCode::GET_MEMBER_OPT, memberIdx, node->line);
            } else {
                currentChunk->emitWithArg(OpCode::GET_MEMBER, memberIdx, node->line);
            }
            break;
        }
        case NodeType::IndexExpr: {
            compileExpression(node->object);
            compileExpression(node->indexExpr);
            currentChunk->emit(OpCode::INDEX_GET, node->line);
            break;
        }
        case NodeType::ListLiteral: {
            for (auto& elem : node->elements) {
                compileExpression(elem);
            }
            currentChunk->emitWithArg(OpCode::MAKE_LIST, (uint16_t)node->elements.size(), node->line);
            break;
        }
        case NodeType::DictLiteral: {
            for (auto& entry : node->entries) {
                uint16_t keyIdx = currentChunk->addConstant(ToValue::makeString(entry.key));
                currentChunk->emitWithArg(OpCode::CONST, keyIdx, node->line);
                compileExpression(entry.value);
            }
            currentChunk->emitWithArg(OpCode::MAKE_DICT, (uint16_t)node->entries.size(), node->line);
            break;
        }
        case NodeType::StringInterpolation: {
            // Compile each element and concatenate
            for (size_t i = 0; i < node->elements.size(); i++) {
                compileExpression(node->elements[i]);
                if (i > 0) {
                    currentChunk->emit(OpCode::CONCAT, node->line);
                }
            }
            break;
        }
        default:
            // Push none for unsupported expressions
            currentChunk->emit(OpCode::NONE, node->line);
            break;
    }
}

// ========================
// VM implementation
// ========================

VM::VM() : globalEnv(std::make_shared<Environment>()) {
    registerBuiltins(globalEnv);
    treeWalker = std::make_shared<Interpreter>("<vm>");
}

Chunk VM::compileFunction(std::shared_ptr<ToFunction> func) {
    // Check cache
    auto it = functionChunks.find(func->name);
    if (it != functionChunks.end()) return it->second;

    Compiler compiler;
    auto program = ASTNode::makeProgram(func->body);
    Chunk chunk = compiler.compile(program, false); // not top-level
    functionChunks[func->name] = chunk;
    return chunk;
}

void VM::push(ToValuePtr val) {
    if (stackTop >= STACK_MAX) {
        throw ToRuntimeError("Stack overflow");
    }
    stack[stackTop++] = val;
}

ToValuePtr VM::pop() {
    if (stackTop == 0) {
        throw ToRuntimeError("Stack underflow");
    }
    return stack[--stackTop];
}

ToValuePtr VM::peek(int offset) {
    if (stackTop <= (size_t)offset) {
        throw ToRuntimeError("Stack underflow on peek");
    }
    return stack[stackTop - 1 - offset];
}

uint16_t VM::readShort(Chunk& chunk, size_t& ip) {
    uint8_t hi = chunk.code[ip++];
    uint8_t lo = chunk.code[ip++];
    return (hi << 8) | lo;
}

void VM::run(Chunk& chunk) {
    // Push initial frame
    frameCount = 0;
    frames[0] = {&chunk, 0, 0, globalEnv};
    frameCount = 1;

    CallFrame* curFrame = &frames[0]; // will be reassigned
restart_frame:
    curFrame = &frames[frameCount - 1];

    while (curFrame->ip < curFrame->chunk->code.size()) {
        auto op = static_cast<OpCode>(curFrame->chunk->code[curFrame->ip++]);

        switch (op) {
            case OpCode::CONST: {
                uint16_t idx = readShort(*curFrame->chunk, curFrame->ip);
                push(curFrame->chunk->constants[idx]);
                break;
            }
            case OpCode::POP: pop(); break;
            case OpCode::DUP: push(peek(0)); break;

            case OpCode::LOAD: {
                uint16_t idx = readShort(*curFrame->chunk, curFrame->ip);
                auto val = curFrame->env->get(curFrame->chunk->names[idx]);
                if (!val) throw ToRuntimeError("Undefined variable '" + curFrame->chunk->names[idx] + "'");
                push(val);
                break;
            }
            case OpCode::STORE: {
                uint16_t idx = readShort(*curFrame->chunk, curFrame->ip);
                auto val = peek(0);
                // If storing a function, set its closure to our global env
                if (val->type == ToValue::Type::FUNCTION && !val->funcVal->closure) {
                    val->funcVal->closure = globalEnv;
                }
                curFrame->env->set(curFrame->chunk->names[idx], val);
                pop();
                break;
            }
            case OpCode::STORE_CONST: {
                uint16_t idx = readShort(*curFrame->chunk, curFrame->ip);
                curFrame->env->defineConst(curFrame->chunk->names[idx], peek(0));
                pop();
                break;
            }

            // Arithmetic
            case OpCode::ADD: {
                auto b = pop(); auto a = pop();
                if (a->type == ToValue::Type::INT && b->type == ToValue::Type::INT)
                    push(ToValue::makeInt(a->intVal + b->intVal));
                else if (a->type == ToValue::Type::STRING || b->type == ToValue::Type::STRING)
                    push(ToValue::makeString(a->toString() + b->toString()));
                else if (a->type == ToValue::Type::FLOAT || b->type == ToValue::Type::FLOAT) {
                    double av = a->type == ToValue::Type::INT ? (double)a->intVal : a->floatVal;
                    double bv = b->type == ToValue::Type::INT ? (double)b->intVal : b->floatVal;
                    push(ToValue::makeFloat(av + bv));
                } else push(ToValue::makeNone());
                break;
            }
            case OpCode::SUB: {
                auto b = pop(); auto a = pop();
                if (a->type == ToValue::Type::INT && b->type == ToValue::Type::INT)
                    push(ToValue::makeInt(a->intVal - b->intVal));
                else {
                    double av = a->type == ToValue::Type::INT ? (double)a->intVal : a->floatVal;
                    double bv = b->type == ToValue::Type::INT ? (double)b->intVal : b->floatVal;
                    push(ToValue::makeFloat(av - bv));
                }
                break;
            }
            case OpCode::MUL: {
                auto b = pop(); auto a = pop();
                if (a->type == ToValue::Type::INT && b->type == ToValue::Type::INT)
                    push(ToValue::makeInt(a->intVal * b->intVal));
                else {
                    double av = a->type == ToValue::Type::INT ? (double)a->intVal : a->floatVal;
                    double bv = b->type == ToValue::Type::INT ? (double)b->intVal : b->floatVal;
                    push(ToValue::makeFloat(av * bv));
                }
                break;
            }
            case OpCode::DIV: {
                auto b = pop(); auto a = pop();
                double bv = b->type == ToValue::Type::INT ? (double)b->intVal : b->floatVal;
                if (bv == 0) throw ToRuntimeError("Division by zero");
                if (a->type == ToValue::Type::INT && b->type == ToValue::Type::INT && a->intVal % b->intVal == 0)
                    push(ToValue::makeInt(a->intVal / b->intVal));
                else {
                    double av = a->type == ToValue::Type::INT ? (double)a->intVal : a->floatVal;
                    push(ToValue::makeFloat(av / bv));
                }
                break;
            }
            case OpCode::MOD: {
                auto b = pop(); auto a = pop();
                if (b->intVal == 0) throw ToRuntimeError("Modulo by zero");
                push(ToValue::makeInt(a->intVal % b->intVal));
                break;
            }
            case OpCode::NEG: {
                auto v = pop();
                if (v->type == ToValue::Type::INT) push(ToValue::makeInt(-v->intVal));
                else push(ToValue::makeFloat(-v->floatVal));
                break;
            }

            // Comparison
            case OpCode::EQ: {
                auto b = pop(); auto a = pop();
                bool eq = false;
                if (a->type != b->type) eq = false;
                else {
                    switch (a->type) {
                        case ToValue::Type::INT: eq = a->intVal == b->intVal; break;
                        case ToValue::Type::FLOAT: eq = a->floatVal == b->floatVal; break;
                        case ToValue::Type::STRING: eq = a->strVal == b->strVal; break;
                        case ToValue::Type::BOOL: eq = a->boolVal == b->boolVal; break;
                        case ToValue::Type::NONE: eq = true; break;
                        default: eq = false;
                    }
                }
                push(ToValue::makeBool(eq));
                break;
            }
            case OpCode::NEQ: {
                auto b = pop(); auto a = pop();
                bool eq = false;
                if (a->type == b->type) {
                    switch (a->type) {
                        case ToValue::Type::INT: eq = a->intVal == b->intVal; break;
                        case ToValue::Type::FLOAT: eq = a->floatVal == b->floatVal; break;
                        case ToValue::Type::STRING: eq = a->strVal == b->strVal; break;
                        case ToValue::Type::BOOL: eq = a->boolVal == b->boolVal; break;
                        case ToValue::Type::NONE: eq = true; break;
                        default: break;
                    }
                }
                push(ToValue::makeBool(!eq));
                break;
            }
            case OpCode::LT: {
                auto b = pop(); auto a = pop();
                double av = a->type == ToValue::Type::INT ? (double)a->intVal : a->floatVal;
                double bv = b->type == ToValue::Type::INT ? (double)b->intVal : b->floatVal;
                push(ToValue::makeBool(av < bv));
                break;
            }
            case OpCode::LTE: {
                auto b = pop(); auto a = pop();
                double av = a->type == ToValue::Type::INT ? (double)a->intVal : a->floatVal;
                double bv = b->type == ToValue::Type::INT ? (double)b->intVal : b->floatVal;
                push(ToValue::makeBool(av <= bv));
                break;
            }
            case OpCode::GT: {
                auto b = pop(); auto a = pop();
                double av = a->type == ToValue::Type::INT ? (double)a->intVal : a->floatVal;
                double bv = b->type == ToValue::Type::INT ? (double)b->intVal : b->floatVal;
                push(ToValue::makeBool(av > bv));
                break;
            }
            case OpCode::GTE: {
                auto b = pop(); auto a = pop();
                double av = a->type == ToValue::Type::INT ? (double)a->intVal : a->floatVal;
                double bv = b->type == ToValue::Type::INT ? (double)b->intVal : b->floatVal;
                push(ToValue::makeBool(av >= bv));
                break;
            }

            // Logical
            case OpCode::NOT: {
                auto v = pop();
                push(ToValue::makeBool(!v->isTruthy()));
                break;
            }

            // Jumps
            case OpCode::JUMP: {
                uint16_t offset = readShort(*curFrame->chunk, curFrame->ip);
                curFrame->ip += offset;
                break;
            }
            case OpCode::JUMP_IF_FALSE: {
                uint16_t offset = readShort(*curFrame->chunk, curFrame->ip);
                auto val = pop();
                if (!val->isTruthy()) curFrame->ip += offset;
                break;
            }
            case OpCode::JUMP_IF_TRUE: {
                uint16_t offset = readShort(*curFrame->chunk, curFrame->ip);
                auto val = pop();
                if (val->isTruthy()) curFrame->ip += offset;
                break;
            }
            case OpCode::LOOP: {
                uint16_t offset = readShort(*curFrame->chunk, curFrame->ip);
                curFrame->ip -= offset;
                break;
            }

            // Functions
            case OpCode::CALL: {
                uint16_t argc = readShort(*curFrame->chunk, curFrame->ip);
                std::vector<ToValuePtr> args;
                for (int i = argc - 1; i >= 0; i--) {
                    args.insert(args.begin(), pop());
                }
                auto callee = pop();

                if (callee->type == ToValue::Type::BUILTIN) {
                    push(callee->builtinVal(args));
                } else if (callee->type == ToValue::Type::FUNCTION) {
                    auto& func = callee->funcVal;

                    // Compile function body to bytecode (cached)
                    // Compile is done in compileFunction
                    Chunk funcChunk; // unused, just trigger cache
                    compileFunction(func);

                    // Create function scope
                    auto funcEnv = (func->closure ? func->closure : curFrame->env)->createChild();
                    for (size_t i = 0; i < func->params.size() && i < args.size(); i++) {
                        funcEnv->define(func->params[i], args[i]);
                    }

                    // Push new call frame
                    if (frameCount >= FRAMES_MAX) throw ToRuntimeError("Call stack overflow");

                    // Store compiled chunk persistently
                    auto& stored = functionChunks[func->name];

                    frames[frameCount] = {&stored, 0, stackTop, funcEnv};
                    frameCount++;
                    goto restart_frame;
                } else if (callee->type == ToValue::Type::CLASS) {
                    // Class instantiation — use tree-walker
                    auto result = treeWalker->callFunction(callee, args, 0);
                    push(result);
                } else {
                    throw ToRuntimeError("Not callable");
                }
                break;
            }
            case OpCode::RETURN: {
                auto result = pop();
                frameCount--;
                if (frameCount == 0) {
                    push(result);
                    return;
                }
                // Restore previous frame
                push(result);
                goto restart_frame;
            }

            // Data structures
            case OpCode::MAKE_LIST: {
                uint16_t count = readShort(*curFrame->chunk, curFrame->ip);
                std::vector<ToValuePtr> items;
                for (int i = count - 1; i >= 0; i--) {
                    items.insert(items.begin(), pop());
                }
                push(ToValue::makeList(std::move(items)));
                break;
            }
            case OpCode::MAKE_DICT: {
                uint16_t count = readShort(*curFrame->chunk, curFrame->ip);
                std::vector<std::pair<std::string, ToValuePtr>> entries;
                for (int i = count - 1; i >= 0; i--) {
                    auto val = pop();
                    auto key = pop();
                    entries.insert(entries.begin(), {key->strVal, val});
                }
                push(ToValue::makeDict(std::move(entries)));
                break;
            }
            case OpCode::MAKE_RANGE: {
                auto end = pop(); auto start = pop();
                std::vector<ToValuePtr> list;
                for (int64_t i = start->intVal; i < end->intVal; i++) {
                    list.push_back(ToValue::makeInt(i));
                }
                push(ToValue::makeList(std::move(list)));
                break;
            }
            case OpCode::INDEX_GET: {
                auto idx = pop(); auto obj = pop();
                if (obj->type == ToValue::Type::LIST) {
                    int64_t i = idx->intVal;
                    if (i < 0) i += obj->listVal.size();
                    push(obj->listVal[i]);
                } else if (obj->type == ToValue::Type::STRING) {
                    int64_t i = idx->intVal;
                    if (i < 0) i += obj->strVal.size();
                    push(ToValue::makeString(std::string(1, obj->strVal[i])));
                } else if (obj->type == ToValue::Type::DICT) {
                    for (auto& p : obj->dictVal) {
                        if (p.first == idx->strVal) { push(p.second); goto done_index; }
                    }
                    throw ToRuntimeError("Key not found: " + idx->strVal);
                    done_index:;
                } else {
                    throw ToRuntimeError("Cannot index " + obj->typeName());
                }
                break;
            }
            case OpCode::INDEX_SET: {
                auto val = pop(); auto idx = pop(); auto obj = pop();
                if (obj->type == ToValue::Type::LIST) {
                    int64_t i = idx->intVal;
                    if (i < 0) i += obj->listVal.size();
                    obj->listVal[i] = val;
                } else if (obj->type == ToValue::Type::DICT) {
                    for (auto& p : obj->dictVal) {
                        if (p.first == idx->strVal) { p.second = val; goto done_iset; }
                    }
                    obj->dictVal.push_back({idx->strVal, val});
                    done_iset:;
                }
                break;
            }

            // Member access
            case OpCode::GET_MEMBER:
            case OpCode::GET_MEMBER_OPT: {
                uint16_t idx = readShort(*curFrame->chunk, curFrame->ip);
                auto obj = pop();
                bool optional = (op == OpCode::GET_MEMBER_OPT);
                if (optional && obj->type == ToValue::Type::NONE) {
                    push(ToValue::makeNone());
                    break;
                }
                auto& name = curFrame->chunk->names[idx];
                if (obj->type == ToValue::Type::INSTANCE) {
                    auto it = obj->instanceVal->fields.find(name);
                    if (it != obj->instanceVal->fields.end()) { push(it->second); break; }
                } else if (obj->type == ToValue::Type::DICT) {
                    for (auto& p : obj->dictVal) {
                        if (p.first == name) { push(p.second); goto done_member; }
                    }
                } else if (obj->type == ToValue::Type::LIST && name == "length") {
                    push(ToValue::makeInt(obj->listVal.size())); break;
                } else if (obj->type == ToValue::Type::STRING && name == "length") {
                    push(ToValue::makeInt(obj->strVal.size())); break;
                }
                push(ToValue::makeNone());
                done_member:;
                break;
            }
            case OpCode::SET_MEMBER: {
                uint16_t idx = readShort(*curFrame->chunk, curFrame->ip);
                auto val = pop();
                auto obj = pop();
                auto& name = curFrame->chunk->names[idx];
                if (obj->type == ToValue::Type::INSTANCE) {
                    obj->instanceVal->fields[name] = val;
                } else if (obj->type == ToValue::Type::DICT) {
                    for (auto& p : obj->dictVal) {
                        if (p.first == name) { p.second = val; goto done_smember; }
                    }
                    obj->dictVal.push_back({name, val});
                    done_smember:;
                }
                break;
            }

            // String concatenation for interpolation
            case OpCode::CONCAT: {
                auto b = pop(); auto a = pop();
                push(ToValue::makeString(a->toString() + b->toString()));
                break;
            }

            // Iteration
            case OpCode::GET_ITER: {
                // Convert iterable to a list-based iterator
                auto iterable = pop();
                if (iterable->type == ToValue::Type::LIST) {
                    // Push a dict with {items, index}
                    auto iter = ToValue::makeDict({});
                    iter->dictVal.push_back({"__items__", iterable});
                    iter->dictVal.push_back({"__index__", ToValue::makeInt(0)});
                    push(iter);
                } else if (iterable->type == ToValue::Type::STRING) {
                    std::vector<ToValuePtr> chars;
                    for (char c : iterable->strVal) chars.push_back(ToValue::makeString(std::string(1, c)));
                    auto iter = ToValue::makeDict({});
                    iter->dictVal.push_back({"__items__", ToValue::makeList(std::move(chars))});
                    iter->dictVal.push_back({"__index__", ToValue::makeInt(0)});
                    push(iter);
                } else {
                    throw ToRuntimeError("Cannot iterate over " + iterable->typeName());
                }
                break;
            }
            case OpCode::ITER_NEXT: {
                auto iter = pop();
                ToValuePtr items, indexVal;
                for (auto& p : iter->dictVal) {
                    if (p.first == "__items__") items = p.second;
                    if (p.first == "__index__") indexVal = p.second;
                }
                int64_t idx = indexVal->intVal;
                if (idx < (int64_t)items->listVal.size()) {
                    push(items->listVal[idx]);
                    // Increment index
                    for (auto& p : iter->dictVal) {
                        if (p.first == "__index__") { p.second = ToValue::makeInt(idx + 1); break; }
                    }
                    push(ToValue::makeBool(true)); // not done
                } else {
                    push(ToValue::makeBool(false)); // done
                }
                break;
            }

            case OpCode::PRINT: {
                auto val = pop();
                std::cout << val->toString() << std::endl;
                break;
            }

            case OpCode::NONE: push(ToValue::makeNone()); break;
            case OpCode::TRUE_: push(ToValue::makeBool(true)); break;
            case OpCode::FALSE_: push(ToValue::makeBool(false)); break;

            case OpCode::HALT: return;

            default:
                throw ToRuntimeError("Unknown opcode: " + std::to_string((int)op));
        }
    }
    // If inner while exits (end of chunk without HALT/RETURN), implicit return
    if (frameCount > 1) {
        frameCount--;
        push(ToValue::makeNone());
        goto restart_frame;
    }
}
