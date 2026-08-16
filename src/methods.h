#pragma once
#include "environment.h"
#include <string>
#include <vector>

class Interpreter;

// ============================================================
// Built-in behaviour, defined once for both engines.
// ------------------------------------------------------------
// The tree-walker (`to run`) and the bytecode VM (`to fast`) both
// call into these, so a method can never mean two different things
// depending on how the program was launched.
// ============================================================

// `recv.name(args)` for every built-in type. Throws when there is no
// such method. `interp` is used to invoke callbacks (map, filter, sort ...)
// and may be null only when the receiver's methods take no callbacks.
ToValuePtr callBuiltinMethod(const ToValuePtr& recv, const std::string& name,
                             std::vector<ToValuePtr>& args, Interpreter* interp, int line);

// `recv.name` with no call — `.length` and friends. Null when the
// receiver has no such property.
ToValuePtr getBuiltinProperty(const ToValuePtr& recv, const std::string& name);

// `obj[index]`
ToValuePtr indexGet(const ToValuePtr& obj, const ToValuePtr& index, int line);
// `obj[index] = value`
void indexSet(const ToValuePtr& obj, const ToValuePtr& index, ToValuePtr value, int line);
// `obj[start:end:step]` — any bound may be null for "as far as it goes".
ToValuePtr sliceValue(const ToValuePtr& obj, const ToValuePtr& start,
                      const ToValuePtr& end, const ToValuePtr& step, int line);

// The binary operators, as a tag rather than a string, so neither engine
// pays for string comparison on every arithmetic instruction.
enum class BinOp : int8_t {
    Add, Sub, Mul, Div, Mod,
    Eq, Neq, Lt, Lte, Gt, Gte,
    Unknown
};

BinOp binOpFor(const std::string& op);
const char* binOpName(BinOp op);

// `left <op> right` for every operator except the short-circuiting
// `and` / `or`, which need their operands evaluated lazily. Handles
// operator overloading on class instances when `interp` is available.
ToValuePtr applyBinaryOp(BinOp op, const ToValuePtr& left,
                         const ToValuePtr& right, Interpreter* interp, int line);
ToValuePtr applyBinaryOp(const std::string& op, const ToValuePtr& left,
                         const ToValuePtr& right, Interpreter* interp, int line);

// `-value` and `not value`.
ToValuePtr applyUnaryOp(const std::string& op, const ToValuePtr& operand, int line);

// Global constructors: set(), tuple(), deque(), queue(), stack(), heap(), max_heap(), sorted(), ...
void registerCollectionBuiltins(EnvPtr env);
