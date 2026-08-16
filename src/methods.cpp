// ============================================================
// methods.cpp — every built-in method, in one place
// ------------------------------------------------------------
// Two rules keep this small enough to learn:
//
//   1. One name per idea. There is no `push` next to `add`, no
//      `contains` next to `has`. Typing the other word gets you a
//      message naming the one to use, so there is exactly one spelling
//      to read in anyone else's code.
//
//   2. The same verbs work on every container. `add`, `pop`, `peek`,
//      `has`, `remove` mean the same thing everywhere — the container
//      decides *which* value they act on. Choosing a stack over a queue
//      is the whole difference, which is the point.
//
// Both the tree-walking interpreter and the bytecode VM dispatch method
// calls through callBuiltinMethod(), so `xs.pop()` behaves identically
// under `to run` and `to fast`.
// ============================================================
#include "methods.h"
#include "interpreter.h"
#include "error.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <cstdio>

// ============================================================
// The method table — one source of truth
// ------------------------------------------------------------
// Drives `help()`, the "did you mean" in error messages, and the docs.
// ============================================================

namespace {

enum TypeBit : uint32_t {
    T_LIST   = 1u << 0,
    T_TUPLE  = 1u << 1,
    T_SET    = 1u << 2,
    T_DICT   = 1u << 3,
    T_DEQUE  = 1u << 4,
    T_QUEUE  = 1u << 5,
    T_STACK  = 1u << 6,
    T_HEAP   = 1u << 7,
    T_STRING = 1u << 8,
};

constexpr uint32_t T_HOLDERS  = T_LIST | T_TUPLE | T_SET | T_DICT | T_DEQUE |
                                T_QUEUE | T_STACK | T_HEAP;
constexpr uint32_t T_EVERY    = T_HOLDERS | T_STRING;
// Containers you can change.
constexpr uint32_t T_MUTABLE  = T_LIST | T_SET | T_DEQUE | T_QUEUE | T_STACK | T_HEAP;
// Containers with numbered positions.
constexpr uint32_t T_POSITION = T_LIST | T_TUPLE | T_DEQUE | T_QUEUE | T_STACK;
// Everything you can walk one value at a time.
constexpr uint32_t T_WALKABLE = T_EVERY;

struct MethodDoc {
    const char* name;
    const char* call;     // how it is written
    const char* summary;  // one line, plain language
    uint32_t types;
    const char* group;
};

// Order here is the order `help()` prints.
const MethodDoc METHODS[] = {
    // ---- Basics -------------------------------------------------------
    {"length",     ".length",            "how many values are in it",              T_EVERY,    "Basics"},
    {"is_empty",   "is_empty()",         "true when there is nothing in it",       T_EVERY,    "Basics"},
    {"has",        "has(value)",         "true when the value is in there",        T_EVERY,    "Basics"},
    {"copy",       "copy()",             "a separate copy you can change freely",  T_HOLDERS,  "Basics"},
    {"clear",      "clear()",            "throw everything out",                   T_MUTABLE | T_DICT, "Basics"},

    // ---- Adding and taking --------------------------------------------
    {"add",        "add(value)",         "put a value in",                         T_MUTABLE,  "Adding and taking"},
    {"add_all",    "add_all(other)",     "put everything from another one in",     T_MUTABLE,  "Adding and taking"},
    {"pop",        "pop()",              "take the next one out and hand it back", T_MUTABLE,  "Adding and taking"},
    {"peek",       "peek()",             "look at what pop() would take",          T_MUTABLE,  "Adding and taking"},
    {"remove",     "remove(value)",      "take that value out; true if it was there", T_MUTABLE, "Adding and taking"},
    {"insert",     "insert(at, value)",  "put a value in at a position",           T_LIST | T_DEQUE, "Adding and taking"},
    {"remove_at",  "remove_at(position)","take out whatever is at a position",     T_LIST | T_DEQUE, "Adding and taking"},
    {"add_first",  "add_first(value)",   "put a value in at the front",            T_DEQUE,    "Adding and taking"},
    {"pop_first",  "pop_first()",        "take the front one out",                 T_DEQUE,    "Adding and taking"},
    {"peek_first", "peek_first()",       "look at the front one",                  T_DEQUE,    "Adding and taking"},
    {"rotate",     "rotate(n)",          "shift everything around by n places",    T_DEQUE,    "Adding and taking"},

    // ---- Finding -------------------------------------------------------
    {"first",      "first()  first(n)",  "the first value, or the first n of them", T_WALKABLE, "Finding"},
    {"last",       "last()  last(n)",    "the last value, or the last n of them",   T_WALKABLE, "Finding"},
    {"skip",       "skip(n)",            "everything except the first n",           T_WALKABLE, "Finding"},
    {"index_of",   "index_of(value)",    "where the value sits, or -1",             T_WALKABLE, "Finding"},
    {"count",      "count(value)",       "how many times the value appears",        T_WALKABLE, "Finding"},
    {"find",       "find(test)",         "the first value the test says yes to",    T_WALKABLE, "Finding"},
    {"any",        "any(test)",          "true when the test says yes to any",      T_WALKABLE, "Finding"},
    {"all",        "all(test)",          "true when the test says yes to all",      T_WALKABLE, "Finding"},
    {"slice",      "slice(from, to)",    "a piece of it; same as xs[from:to]",      T_WALKABLE, "Finding"},

    // ---- Transforming --------------------------------------------------
    {"map",        "map(change)",        "a list of every value, changed",          T_WALKABLE, "Transforming"},
    {"filter",     "filter(test)",       "only the values the test says yes to",    T_WALKABLE, "Transforming"},
    {"reduce",     "reduce(combine, start)", "fold everything down to one value",   T_WALKABLE, "Transforming"},
    {"each",       "each(do)",           "run something once per value",            T_WALKABLE, "Transforming"},
    {"sum",        "sum()",              "add all the numbers up",                  T_WALKABLE, "Transforming"},
    {"min",        "min()",              "the smallest value",                      T_WALKABLE, "Transforming"},
    {"max",        "max()",              "the largest value",                       T_WALKABLE, "Transforming"},
    {"sort",       "sort()  sort(key)",  "put it in order, in place",               T_LIST | T_DEQUE, "Transforming"},
    {"sorted",     "sorted()  sorted(key)", "a new one, in order",                  T_WALKABLE, "Transforming"},
    {"reverse",    "reverse()",          "flip it around, in place",                T_LIST | T_DEQUE, "Transforming"},
    {"reversed",   "reversed()",         "a new one, flipped around",               T_WALKABLE, "Transforming"},
    {"unique",     "unique()",           "drop repeats, keeping the first of each", T_WALKABLE, "Transforming"},
    {"flatten",    "flatten()",          "pull nested collections up one level",    T_WALKABLE, "Transforming"},
    {"chunk",      "chunk(n)",           "split into pieces of n",                  T_WALKABLE, "Transforming"},
    {"zip",        "zip(other)",         "pair each value with another's",          T_WALKABLE, "Transforming"},
    {"join",       "join(separator)",    "glue the values into one string",         T_WALKABLE, "Transforming"},

    // ---- Sets -----------------------------------------------------------
    {"union",      "union(other)",       "everything in either one",                T_SET, "Sets"},
    {"intersect",  "intersect(other)",   "only what is in both",                    T_SET, "Sets"},
    {"difference", "difference(other)",  "what is in this one but not the other",   T_SET, "Sets"},
    {"symmetric_difference", "symmetric_difference(other)", "what is in one but not both", T_SET, "Sets"},
    {"is_subset",  "is_subset(other)",   "true when the other holds all of these",  T_SET, "Sets"},
    {"is_superset","is_superset(other)", "true when this holds all of the other's", T_SET, "Sets"},
    {"is_disjoint","is_disjoint(other)", "true when they share nothing",            T_SET, "Sets"},

    // ---- Dicts -----------------------------------------------------------
    {"keys",       "keys()",             "a list of the keys",                      T_DICT, "Dicts"},
    {"values",     "values()",           "a list of the values",                    T_DICT, "Dicts"},
    {"entries",    "entries()",          "a list of (key, value) pairs",            T_DICT, "Dicts"},
    {"get",        "get(key, fallback)", "the value, or the fallback when missing", T_DICT, "Dicts"},
    {"set",        "set(key, value)",    "store a value under a key",               T_DICT, "Dicts"},
    {"merge",      "merge(other)",       "a new dict with the other one laid on top", T_DICT, "Dicts"},
    {"map_values", "map_values(change)", "a new dict with every value changed",     T_DICT, "Dicts"},
    {"invert",     "invert()",           "a new dict with keys and values swapped", T_DICT, "Dicts"},

    // ---- Text -------------------------------------------------------------
    {"upper",       "upper()",            "in capitals",                            T_STRING, "Text"},
    {"lower",       "lower()",            "in lower case",                          T_STRING, "Text"},
    {"trim",        "trim()",             "without the spaces at either end",       T_STRING, "Text"},
    {"trim_start",  "trim_start()",       "without the spaces at the start",        T_STRING, "Text"},
    {"trim_end",    "trim_end()",         "without the spaces at the end",          T_STRING, "Text"},
    {"split",       "split(separator)",   "cut into a list of pieces",              T_STRING, "Text"},
    {"lines",       "lines()",            "cut into a list, one per line",          T_STRING, "Text"},
    {"chars",       "chars()",            "a list of single characters",            T_STRING, "Text"},
    {"replace",     "replace(old, new)",  "with every old swapped for new",         T_STRING, "Text"},
    {"starts_with", "starts_with(text)",  "true when it begins with that",          T_STRING, "Text"},
    {"ends_with",   "ends_with(text)",    "true when it ends with that",            T_STRING, "Text"},
    {"repeat",      "repeat(n)",          "itself, n times over",                   T_STRING, "Text"},
    {"pad_start",   "pad_start(width)",   "padded out to a width from the left",    T_STRING, "Text"},
    {"pad_end",     "pad_end(width)",     "padded out to a width from the right",   T_STRING, "Text"},
    {"capitalize",  "capitalize()",       "with the first letter in capitals",      T_STRING, "Text"},
    {"to_int",      "to_int()",           "read it as a whole number",              T_STRING, "Text"},
    {"to_float",    "to_float()",         "read it as a decimal number",            T_STRING, "Text"},

    // ---- Converting ----------------------------------------------------------
    {"to_list",   "to_list()",   "the same values, as a list",   T_EVERY, "Converting"},
    {"to_set",    "to_set()",    "the same values, as a set",    T_EVERY, "Converting"},
    {"to_tuple",  "to_tuple()",  "the same values, as a tuple",  T_EVERY, "Converting"},
    {"to_deque",  "to_deque()",  "the same values, as a deque",  T_EVERY, "Converting"},
    {"to_queue",  "to_queue()",  "the same values, as a queue",  T_EVERY, "Converting"},
    {"to_stack",  "to_stack()",  "the same values, as a stack",  T_EVERY, "Converting"},
    {"to_heap",   "to_heap()",   "the same values, as a heap",   T_EVERY, "Converting"},
};

// Words people reach for out of habit, and the one word `to` uses instead.
// Typing the wrong one is not an error you have to guess your way out of.
struct Redirect { const char* wrong; const char* right; };
const Redirect REDIRECTS[] = {
    {"push",        "add"},
    {"append",      "add"},
    {"enqueue",     "add"},
    {"push_back",   "add"},
    {"push_front",  "add_first"},
    {"dequeue",     "pop"},
    {"pop_back",    "pop"},
    {"pop_front",   "pop_first"},
    {"shift",       "pop_first"},
    {"unshift",     "add_first"},
    {"top",         "peek"},
    {"front",       "peek"},
    {"back",        "peek"},
    {"peek_back",   "peek"},
    {"peek_front",  "peek_first"},
    {"contains",    "has"},
    {"includes",    "has"},
    {"has_key",     "has"},
    {"member",      "has"},
    {"discard",     "remove"},
    {"delete",      "remove"},
    {"remove_value","remove"},
    {"erase",       "remove"},
    {"size",        "length"},
    {"count_all",   "length"},
    {"items",       "entries"},
    {"pairs",       "entries"},
    {"fold",        "reduce"},
    {"inject",      "reduce"},
    {"for_each",    "each"},
    {"foreach",     "each"},
    {"drop",        "skip"},
    {"extend",      "add_all"},
    {"concat",      "add_all"},
    {"update",      "merge"},
    {"sort_desc",   "sort"},
    {"sorted_desc", "sorted"},
    {"put",         "set"},
    {"index",       "index_of"},
    {"find_index",  "index_of"},
    {"substring",   "slice"},
    {"sublist",     "slice"},
};

std::string capitalized(std::string s) {
    if (!s.empty()) s[0] = (char)std::toupper((unsigned char)s[0]);
    return s;
}

uint32_t typeBitOf(const ToValuePtr& v) {
    switch (v->type) {
        case ToValue::Type::LIST:   return T_LIST;
        case ToValue::Type::TUPLE:  return T_TUPLE;
        case ToValue::Type::SET:    return T_SET;
        case ToValue::Type::DICT:   return T_DICT;
        case ToValue::Type::DEQUE:  return T_DEQUE;
        case ToValue::Type::QUEUE:  return T_QUEUE;
        case ToValue::Type::STACK:  return T_STACK;
        case ToValue::Type::HEAP:   return T_HEAP;
        case ToValue::Type::STRING: return T_STRING;
        default: return 0;
    }
}

// How many single-character edits turn one word into the other, capped
// so a wild guess never reads as a suggestion.
int editDistance(const std::string& a, const std::string& b) {
    std::vector<int> prev(b.size() + 1), cur(b.size() + 1);
    for (size_t j = 0; j <= b.size(); j++) prev[j] = (int)j;
    for (size_t i = 1; i <= a.size(); i++) {
        cur[0] = (int)i;
        for (size_t j = 1; j <= b.size(); j++) {
            int cost = a[i - 1] == b[j - 1] ? 0 : 1;
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
        }
        prev = cur;
    }
    return prev[b.size()];
}

} // namespace

// The message a wrong member name produces. It always ends with something
// the reader can act on.
void reportUnknownMember(const ToValuePtr& recv, const std::string& name, int line) {
    const std::string kind = recv->typeName();
    const std::string aKind = withArticle(kind);
    uint32_t bit = typeBitOf(recv);

    for (auto& r : REDIRECTS) {
        if (name != r.wrong) continue;
        // Only suggest the replacement if this type actually has it.
        for (auto& m : METHODS) {
            if (name == r.wrong && std::string(m.name) == r.right && (m.types & bit)) {
                throw ToRuntimeError(capitalized(aKind) + " has no '" + name +
                                     "' — in to it is called '" + r.right + "'. Try " +
                                     kind + "." + m.call, line);
            }
        }
    }

    // A method that exists, but not on this type.
    for (auto& m : METHODS) {
        if (name == m.name && !(m.types & bit)) {
            if (recv->type == ToValue::Type::TUPLE)
                throw ToRuntimeError("A tuple cannot be changed, so it has no '" + name +
                                     "'. Call to_list() first if you need to change it.", line);
            throw ToRuntimeError("'" + name + "' works on other collections, but not on " +
                                 aKind + ". Call to_list() first if you need it.", line);
        }
    }

    // A near miss — probably a typo.
    std::string best;
    int bestDistance = 3;
    for (auto& m : METHODS) {
        if (!(m.types & bit)) continue;
        int d = editDistance(name, m.name);
        if (d < bestDistance) { bestDistance = d; best = m.name; }
    }
    if (!best.empty())
        throw ToRuntimeError(capitalized(aKind) + " has no '" + name + "' — did you mean '" +
                             best + "'?  Run help(value) to see everything " + aKind +
                             " can do.", line);

    throw ToRuntimeError(capitalized(aKind) + " has no '" + name +
                         "'. Run help(value) to see everything " + aKind + " can do.", line);
}

namespace {

// ------------------------------------------------------------
// Small helpers
// ------------------------------------------------------------

[[noreturn]] void argError(const std::string& what, const std::string& expected, int line) {
    throw ToRuntimeError(what + " takes " + expected, line);
}

void expectArgs(const std::vector<ToValuePtr>& args, size_t n,
                const std::string& what, const std::string& expected, int line) {
    if (args.size() != n) argError(what, expected, line);
}

int64_t asIndex(const ToValuePtr& v, const std::string& what, int line) {
    if (!v || v->type != ToValue::Type::INT)
        throw ToRuntimeError(what + " expects a whole number", line);
    return v->intVal;
}

// Resolve a possibly-negative index against a length. Returns -1 when out of range.
int64_t normalizeIndex(int64_t i, int64_t len) {
    if (i < 0) i += len;
    if (i < 0 || i >= len) return -1;
    return i;
}

[[noreturn]] void emptyError(const ToValuePtr& recv, const std::string& verb, int line) {
    throw ToRuntimeError("Cannot " + verb + " from an empty " + recv->typeName() +
                         " — check is_empty() first", line);
}

ToValuePtr invoke(Interpreter* interp, const ToValuePtr& fn,
                  std::vector<ToValuePtr> args, const std::string& what, int line) {
    if (!fn || (fn->type != ToValue::Type::FUNCTION && fn->type != ToValue::Type::BUILTIN &&
                fn->type != ToValue::Type::CLASS))
        throw ToRuntimeError(what + " expects a function, like (x): x * 2", line);
    if (fn->type == ToValue::Type::BUILTIN) return fn->builtinVal(std::move(args));
    if (!interp)
        throw ToRuntimeError(what + " cannot call a function here", line);
    return interp->callFunction(fn, args, line);
}

ToCollectionPtr coll(const ToValuePtr& v) {
    if (!v->collVal) v->collVal = std::make_shared<ToCollection>();
    return v->collVal;
}

// A comparator built from an optional key function.
struct KeyedLess {
    Interpreter* interp;
    ToValuePtr keyFn;
    int line;

    bool operator()(const ToValuePtr& a, const ToValuePtr& b) const {
        if (!keyFn) return valueCompare(a, b) < 0;
        return valueCompare(invoke(interp, keyFn, {a}, "sort key", line),
                            invoke(interp, keyFn, {b}, "sort key", line)) < 0;
    }
};

// ------------------------------------------------------------
// Methods every collection answers to
// ------------------------------------------------------------

ToValuePtr walkableMethod(const ToValuePtr& recv, const std::string& name,
                          std::vector<ToValuePtr>& args, Interpreter* interp, int line) {
    const std::string what = recv->typeName() + "." + name + "()";

    if (name == "length")   return ToValue::makeInt(recv->length());
    if (name == "is_empty") return ToValue::makeBool(recv->length() == 0);

    if (name == "to_list")  return ToValue::makeList(recv->elements());
    if (name == "to_tuple") return ToValue::makeTuple(recv->elements());
    if (name == "to_set")   return ToValue::makeSet(recv->elements());
    if (name == "to_deque") return ToValue::makeDeque(recv->elements());
    if (name == "to_queue") return ToValue::makeQueue(recv->elements());
    if (name == "to_stack") return ToValue::makeStack(recv->elements());
    if (name == "to_heap")  return ToValue::makeHeap(recv->elements());

    if (name == "has") {
        expectArgs(args, 1, what, "exactly 1 value", line);
        if (recv->type == ToValue::Type::SET) return ToValue::makeBool(coll(recv)->has(args[0]));
        if (recv->type == ToValue::Type::DICT) {
            if (!valueHashable(args[0])) return ToValue::makeBool(false);
            return ToValue::makeBool(recv->dictVal.containsKey(args[0]));
        }
        if (recv->type == ToValue::Type::STRING) {
            if (args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("string.has() looks for text inside text", line);
            return ToValue::makeBool(recv->strVal.find(args[0]->strVal) != std::string::npos);
        }
        for (auto& item : recv->elements())
            if (valueEquals(item, args[0])) return ToValue::makeBool(true);
        return ToValue::makeBool(false);
    }
    if (name == "index_of") {
        expectArgs(args, 1, what, "exactly 1 value", line);
        if (recv->type == ToValue::Type::STRING) {
            if (args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("string.index_of() looks for text inside text", line);
            size_t pos = recv->strVal.find(args[0]->strVal);
            return ToValue::makeInt(pos == std::string::npos ? -1 : (int64_t)pos);
        }
        auto items = recv->elements();
        for (size_t i = 0; i < items.size(); i++)
            if (valueEquals(items[i], args[0])) return ToValue::makeInt((int64_t)i);
        return ToValue::makeInt(-1);
    }
    if (name == "count") {
        expectArgs(args, 1, what, "exactly 1 value", line);
        if (recv->type == ToValue::Type::STRING) {
            if (args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("string.count() counts text inside text", line);
            const std::string& needle = args[0]->strVal;
            if (needle.empty()) return ToValue::makeInt(0);
            int64_t n = 0;
            size_t pos = 0;
            while ((pos = recv->strVal.find(needle, pos)) != std::string::npos) {
                n++;
                pos += needle.size();
            }
            return ToValue::makeInt(n);
        }
        int64_t n = 0;
        for (auto& item : recv->elements())
            if (valueEquals(item, args[0])) n++;
        return ToValue::makeInt(n);
    }

    if (name == "first" || name == "last") {
        bool wantFirst = name == "first";
        if (recv->type == ToValue::Type::STRING) {
            int64_t n = args.empty() ? 1 : asIndex(args[0], what, line);
            n = std::max<int64_t>(0, std::min<int64_t>(n, (int64_t)recv->strVal.size()));
            std::string piece = wantFirst ? recv->strVal.substr(0, n)
                                          : recv->strVal.substr(recv->strVal.size() - n);
            return ToValue::makeString(std::move(piece));
        }
        auto items = recv->elements();
        if (args.empty()) {
            if (items.empty()) return ToValue::makeNone();
            return wantFirst ? items.front() : items.back();
        }
        int64_t n = asIndex(args[0], what, line);
        n = std::max<int64_t>(0, std::min<int64_t>(n, (int64_t)items.size()));
        std::vector<ToValuePtr> out;
        if (wantFirst) out.assign(items.begin(), items.begin() + n);
        else out.assign(items.end() - n, items.end());
        return ToValue::makeList(std::move(out));
    }
    if (name == "skip") {
        expectArgs(args, 1, what, "exactly 1 whole number", line);
        int64_t n = asIndex(args[0], what, line);
        if (recv->type == ToValue::Type::STRING) {
            n = std::max<int64_t>(0, std::min<int64_t>(n, (int64_t)recv->strVal.size()));
            return ToValue::makeString(recv->strVal.substr(n));
        }
        auto items = recv->elements();
        n = std::max<int64_t>(0, std::min<int64_t>(n, (int64_t)items.size()));
        return ToValue::makeList(std::vector<ToValuePtr>(items.begin() + n, items.end()));
    }
    if (name == "slice") {
        if (args.empty() || args.size() > 3) argError(what, "1 to 3 whole numbers", line);
        return sliceValue(recv, args[0], args.size() > 1 ? args[1] : nullptr,
                          args.size() > 2 ? args[2] : nullptr, line);
    }

    if (name == "join") {
        std::string sep;
        if (!args.empty() && args[0]->type == ToValue::Type::STRING) sep = args[0]->strVal;
        std::string result;
        auto items = recv->elements();
        for (size_t i = 0; i < items.size(); i++) {
            if (i > 0) result += sep;
            result += items[i]->toString();
        }
        return ToValue::makeString(std::move(result));
    }

    if (name == "map") {
        expectArgs(args, 1, what, "exactly 1 function", line);
        auto items = recv->elements();
        std::vector<ToValuePtr> out;
        out.reserve(items.size());
        for (auto& item : items) out.push_back(invoke(interp, args[0], {item}, what, line));
        return ToValue::makeList(std::move(out));
    }
    if (name == "filter") {
        expectArgs(args, 1, what, "exactly 1 function", line);
        if (recv->type == ToValue::Type::DICT) {
            ToDict out;
            for (size_t i = 0; i < recv->dictVal.size(); i++) {
                auto key = recv->dictVal.keyAt(i);
                if (invoke(interp, args[0], {key, recv->dictVal[i].second}, what, line)->isTruthy())
                    out.setKey(key, recv->dictVal[i].second);
            }
            return ToValue::makeDict(std::move(out));
        }
        std::vector<ToValuePtr> out;
        for (auto& item : recv->elements())
            if (invoke(interp, args[0], {item}, what, line)->isTruthy()) out.push_back(item);
        return ToValue::makeList(std::move(out));
    }
    if (name == "reduce") {
        if (args.empty() || args.size() > 2) argError(what, "a function and a starting value", line);
        auto items = recv->elements();
        size_t start = 0;
        ToValuePtr acc;
        if (args.size() == 2) {
            acc = args[1];
        } else {
            if (items.empty())
                throw ToRuntimeError(what + " on an empty " + recv->typeName() +
                                     " needs a starting value: reduce(combine, start)", line);
            acc = items[0];
            start = 1;
        }
        for (size_t i = start; i < items.size(); i++)
            acc = invoke(interp, args[0], {acc, items[i]}, what, line);
        return acc;
    }
    if (name == "each") {
        expectArgs(args, 1, what, "exactly 1 function", line);
        if (recv->type == ToValue::Type::DICT) {
            for (size_t i = 0; i < recv->dictVal.size(); i++)
                invoke(interp, args[0], {recv->dictVal.keyAt(i), recv->dictVal[i].second}, what, line);
            return ToValue::makeNone();
        }
        for (auto& item : recv->elements()) invoke(interp, args[0], {item}, what, line);
        return ToValue::makeNone();
    }
    if (name == "find") {
        expectArgs(args, 1, what, "exactly 1 function", line);
        for (auto& item : recv->elements())
            if (invoke(interp, args[0], {item}, what, line)->isTruthy()) return item;
        return ToValue::makeNone();
    }
    if (name == "any") {
        expectArgs(args, 1, what, "exactly 1 function", line);
        for (auto& item : recv->elements())
            if (invoke(interp, args[0], {item}, what, line)->isTruthy()) return ToValue::makeBool(true);
        return ToValue::makeBool(false);
    }
    if (name == "all") {
        expectArgs(args, 1, what, "exactly 1 function", line);
        for (auto& item : recv->elements())
            if (!invoke(interp, args[0], {item}, what, line)->isTruthy()) return ToValue::makeBool(false);
        return ToValue::makeBool(true);
    }

    if (name == "sum") {
        int64_t isum = 0;
        double fsum = 0;
        bool useFloat = false;
        for (auto& item : recv->elements()) {
            if (item->type == ToValue::Type::INT) { isum += item->intVal; fsum += (double)item->intVal; }
            else if (item->type == ToValue::Type::FLOAT) { useFloat = true; fsum += item->floatVal; }
            else throw ToRuntimeError(what + " needs numbers, but found a " + item->typeName(), line);
        }
        return useFloat ? ToValue::makeFloat(fsum) : ToValue::makeInt(isum);
    }
    if (name == "min" || name == "max") {
        auto items = recv->elements();
        if (items.empty())
            throw ToRuntimeError(what + " needs at least one value", line);
        KeyedLess less{interp, args.empty() ? nullptr : args[0], line};
        auto it = name == "min" ? std::min_element(items.begin(), items.end(), less)
                                : std::max_element(items.begin(), items.end(), less);
        return *it;
    }

    if (name == "sorted") {
        auto items = recv->elements();
        std::stable_sort(items.begin(), items.end(),
                         KeyedLess{interp, args.empty() ? nullptr : args[0], line});
        return ToValue::makeList(std::move(items));
    }
    if (name == "reversed") {
        if (recv->type == ToValue::Type::STRING)
            return ToValue::makeString(std::string(recv->strVal.rbegin(), recv->strVal.rend()));
        auto items = recv->elements();
        std::reverse(items.begin(), items.end());
        return ToValue::makeList(std::move(items));
    }
    if (name == "unique") {
        std::vector<ToValuePtr> out;
        ToCollection seen;
        for (auto& item : recv->elements()) {
            if (!valueHashable(item)) {
                bool dup = false;
                for (auto& kept : out) if (valueEquals(kept, item)) { dup = true; break; }
                if (!dup) out.push_back(item);
            } else if (seen.insert(item)) {
                out.push_back(item);
            }
        }
        return ToValue::makeList(std::move(out));
    }
    if (name == "flatten") {
        std::vector<ToValuePtr> out;
        for (auto& item : recv->elements()) {
            if (item->length() >= 0 && item->type != ToValue::Type::STRING) {
                for (auto& inner : item->elements()) out.push_back(inner);
            } else {
                out.push_back(item);
            }
        }
        return ToValue::makeList(std::move(out));
    }
    if (name == "chunk") {
        expectArgs(args, 1, what, "exactly 1 whole number", line);
        int64_t n = asIndex(args[0], what, line);
        if (n <= 0) throw ToRuntimeError(what + " needs a chunk size of 1 or more", line);
        auto items = recv->elements();
        std::vector<ToValuePtr> out;
        for (size_t i = 0; i < items.size(); i += (size_t)n) {
            std::vector<ToValuePtr> part(items.begin() + i,
                                         items.begin() + std::min(items.size(), i + (size_t)n));
            out.push_back(ToValue::makeList(std::move(part)));
        }
        return ToValue::makeList(std::move(out));
    }
    if (name == "zip") {
        expectArgs(args, 1, what, "exactly 1 collection", line);
        auto a = recv->elements();
        auto b = args[0]->elements();
        std::vector<ToValuePtr> out;
        size_t n = std::min(a.size(), b.size());
        for (size_t i = 0; i < n; i++) out.push_back(ToValue::makeTuple({a[i], b[i]}));
        return ToValue::makeList(std::move(out));
    }

    if (name == "copy") {
        switch (recv->type) {
            case ToValue::Type::LIST:  return ToValue::makeList(recv->listVal);
            case ToValue::Type::TUPLE: return ToValue::makeTuple(recv->listVal);
            case ToValue::Type::DICT:  return ToValue::makeDict(recv->dictVal);
            case ToValue::Type::SET:   return ToValue::makeSet(recv->elements());
            case ToValue::Type::DEQUE: return ToValue::makeDeque(recv->elements());
            case ToValue::Type::QUEUE: return ToValue::makeQueue(recv->elements());
            case ToValue::Type::STACK: return ToValue::makeStack(recv->elements());
            case ToValue::Type::HEAP: {
                auto c = std::make_shared<ToCollection>(*coll(recv));
                return ToValue::makeCollection(ToValue::Type::HEAP, c);
            }
            default: break;
        }
    }
    return nullptr;
}

// ------------------------------------------------------------
// add / pop / peek / remove — the container decides which value
// ------------------------------------------------------------

// What pop() and peek() act on, per container:
//   list, stack, deque  the most recently added  (the back)
//   queue               the one waiting longest  (the front)
//   heap                the smallest             (largest, for max_heap)
//   set                 the one added first
bool popsFromFront(const ToValuePtr& recv) {
    return recv->type == ToValue::Type::QUEUE || recv->type == ToValue::Type::SET;
}

ToValuePtr mutableMethod(const ToValuePtr& recv, const std::string& name,
                         std::vector<ToValuePtr>& args, Interpreter* interp, int line) {
    const std::string what = recv->typeName() + "." + name + "()";
    bool isList = recv->type == ToValue::Type::LIST;
    bool isHeap = recv->type == ToValue::Type::HEAP;
    bool isSet = recv->type == ToValue::Type::SET;

    if (name == "add" || name == "add_all") {
        bool all = name == "add_all";
        if (args.empty()) argError(what, all ? "1 collection" : "at least 1 value", line);
        std::vector<ToValuePtr> incoming;
        if (all) {
            for (auto& a : args) {
                if (a->length() < 0) throw ToRuntimeError(what + " expects a collection", line);
                for (auto& v : a->elements()) incoming.push_back(v);
            }
        } else {
            incoming = args;
        }
        bool changed = false;
        for (auto& v : incoming) {
            if (isList) recv->listVal.push_back(v);
            else if (isSet) changed = coll(recv)->insert(v) || changed;
            else if (isHeap) coll(recv)->heapPush(v);
            else coll(recv)->items.push_back(v);
        }
        return isSet ? ToValue::makeBool(changed) : ToValue::makeNone();
    }

    if (name == "pop") {
        if (recv->length() == 0) emptyError(recv, "pop", line);
        if (isList) {
            auto v = recv->listVal.back();
            recv->listVal.pop_back();
            return v;
        }
        if (isHeap) return coll(recv)->heapPop();
        if (isSet) {
            auto v = coll(recv)->items.front();
            coll(recv)->discard(v);
            return v;
        }
        auto c = coll(recv);
        if (popsFromFront(recv)) {
            auto v = c->items.front();
            c->items.pop_front();
            return v;
        }
        auto v = c->items.back();
        c->items.pop_back();
        return v;
    }

    if (name == "peek") {
        if (recv->length() == 0) return ToValue::makeNone();
        if (isList) return recv->listVal.back();
        auto c = coll(recv);
        // A heap keeps its smallest at the front.
        return (isHeap || popsFromFront(recv)) ? c->items.front() : c->items.back();
    }

    if (name == "remove") {
        expectArgs(args, 1, what, "exactly 1 value", line);
        if (isSet) return ToValue::makeBool(coll(recv)->discard(args[0]));
        if (isList) {
            for (size_t i = 0; i < recv->listVal.size(); i++) {
                if (valueEquals(recv->listVal[i], args[0])) {
                    recv->listVal.erase(recv->listVal.begin() + i);
                    return ToValue::makeBool(true);
                }
            }
            return ToValue::makeBool(false);
        }
        auto c = coll(recv);
        for (size_t i = 0; i < c->items.size(); i++) {
            if (valueEquals(c->items[i], args[0])) {
                c->items.erase(c->items.begin() + i);
                if (isHeap) c->heapify();
                return ToValue::makeBool(true);
            }
        }
        return ToValue::makeBool(false);
    }

    if (name == "clear") {
        if (isList) recv->listVal.clear();
        else {
            coll(recv)->items.clear();
            coll(recv)->index.clear();
        }
        return ToValue::makeNone();
    }

    // ---- deque's other end -------------------------------------------
    if (recv->type == ToValue::Type::DEQUE) {
        if (name == "add_first") {
            if (args.empty()) argError(what, "at least 1 value", line);
            for (auto& a : args) coll(recv)->items.push_front(a);
            return ToValue::makeNone();
        }
        if (name == "pop_first") {
            if (recv->length() == 0) emptyError(recv, "pop", line);
            auto v = coll(recv)->items.front();
            coll(recv)->items.pop_front();
            return v;
        }
        if (name == "peek_first")
            return recv->length() == 0 ? ToValue::makeNone() : coll(recv)->items.front();
        if (name == "rotate") {
            int64_t n = args.empty() ? 1 : asIndex(args[0], what, line);
            auto c = coll(recv);
            if (!c->items.empty()) {
                int64_t sz = (int64_t)c->items.size();
                n = ((n % sz) + sz) % sz;
                std::rotate(c->items.begin(), c->items.end() - n, c->items.end());
            }
            return ToValue::makeNone();
        }
    }

    // ---- positions: lists and deques ----------------------------------
    if (isList || recv->type == ToValue::Type::DEQUE) {
        int64_t len = recv->length();
        if (name == "insert") {
            expectArgs(args, 2, what, "a position and a value", line);
            int64_t i = asIndex(args[0], what, line);
            if (i < 0) i += len;
            i = std::max<int64_t>(0, std::min<int64_t>(i, len));
            if (isList) recv->listVal.insert(recv->listVal.begin() + i, args[1]);
            else coll(recv)->items.insert(coll(recv)->items.begin() + i, args[1]);
            return ToValue::makeNone();
        }
        if (name == "remove_at") {
            expectArgs(args, 1, what, "exactly 1 position", line);
            int64_t i = normalizeIndex(asIndex(args[0], what, line), len);
            if (i < 0)
                throw ToRuntimeError(what + ": there is no position " + args[0]->toString() +
                                     " in a " + recv->typeName() + " of " +
                                     std::to_string(len), line);
            ToValuePtr v;
            if (isList) {
                v = recv->listVal[i];
                recv->listVal.erase(recv->listVal.begin() + i);
            } else {
                v = coll(recv)->items[i];
                coll(recv)->items.erase(coll(recv)->items.begin() + i);
            }
            return v;
        }
        if (name == "sort") {
            KeyedLess less{interp, args.empty() ? nullptr : args[0], line};
            if (isList) std::stable_sort(recv->listVal.begin(), recv->listVal.end(), less);
            else std::stable_sort(coll(recv)->items.begin(), coll(recv)->items.end(), less);
            return ToValue::makeNone();
        }
        if (name == "reverse") {
            if (isList) std::reverse(recv->listVal.begin(), recv->listVal.end());
            else std::reverse(coll(recv)->items.begin(), coll(recv)->items.end());
            return ToValue::makeNone();
        }
    }

    return nullptr;
}

// ------------------------------------------------------------
// set
// ------------------------------------------------------------

ToValuePtr setMethod(const ToValuePtr& recv, const std::string& name,
                     std::vector<ToValuePtr>& args, int line) {
    const std::string what = "set." + name + "()";
    if (name != "union" && name != "intersect" && name != "difference" &&
        name != "symmetric_difference" && name != "is_subset" &&
        name != "is_superset" && name != "is_disjoint")
        return nullptr;

    expectArgs(args, 1, what, "exactly 1 collection", line);
    auto c = coll(recv);
    auto other = ToValue::makeSet(args[0]->elements());
    auto oc = coll(other);

    if (name == "is_subset") {
        for (auto& v : c->items) if (!oc->has(v)) return ToValue::makeBool(false);
        return ToValue::makeBool(true);
    }
    if (name == "is_superset") {
        for (auto& v : oc->items) if (!c->has(v)) return ToValue::makeBool(false);
        return ToValue::makeBool(true);
    }
    if (name == "is_disjoint") {
        for (auto& v : c->items) if (oc->has(v)) return ToValue::makeBool(false);
        return ToValue::makeBool(true);
    }

    std::vector<ToValuePtr> out;
    if (name == "union") {
        for (auto& v : c->items) out.push_back(v);
        for (auto& v : oc->items) out.push_back(v);
    } else if (name == "intersect") {
        for (auto& v : c->items) if (oc->has(v)) out.push_back(v);
    } else if (name == "difference") {
        for (auto& v : c->items) if (!oc->has(v)) out.push_back(v);
    } else {
        for (auto& v : c->items) if (!oc->has(v)) out.push_back(v);
        for (auto& v : oc->items) if (!c->has(v)) out.push_back(v);
    }
    return ToValue::makeSet(std::move(out));
}

// ------------------------------------------------------------
// dict
// ------------------------------------------------------------

ToValuePtr dictMethod(const ToValuePtr& recv, const std::string& name,
                      std::vector<ToValuePtr>& args, Interpreter* interp, int line) {
    auto& d = recv->dictVal;
    const std::string what = "dict." + name + "()";

    if (name == "keys") {
        std::vector<ToValuePtr> out;
        out.reserve(d.size());
        for (size_t i = 0; i < d.size(); i++) out.push_back(d.keyAt(i));
        return ToValue::makeList(std::move(out));
    }
    if (name == "values") {
        std::vector<ToValuePtr> out;
        out.reserve(d.size());
        for (auto& e : d) out.push_back(e.second);
        return ToValue::makeList(std::move(out));
    }
    if (name == "entries") {
        std::vector<ToValuePtr> out;
        out.reserve(d.size());
        for (size_t i = 0; i < d.size(); i++)
            out.push_back(ToValue::makeTuple({d.keyAt(i), d[i].second}));
        return ToValue::makeList(std::move(out));
    }
    if (name == "get") {
        if (args.empty() || args.size() > 2) argError(what, "a key and a fallback value", line);
        if (!valueHashable(args[0])) return args.size() > 1 ? args[1] : ToValue::makeNone();
        auto found = d.getKey(args[0]);
        if (found) return found;
        return args.size() > 1 ? args[1] : ToValue::makeNone();
    }
    if (name == "set") {
        expectArgs(args, 2, what, "a key and a value", line);
        d.setKey(args[0], args[1]);
        return ToValue::makeNone();
    }
    if (name == "remove") {
        expectArgs(args, 1, what, "exactly 1 key", line);
        if (!valueHashable(args[0])) return ToValue::makeBool(false);
        return ToValue::makeBool(d.eraseKey(args[0]));
    }
    if (name == "clear") {
        d.clear();
        return ToValue::makeNone();
    }
    if (name == "merge") {
        expectArgs(args, 1, what, "exactly 1 dict", line);
        if (args[0]->type != ToValue::Type::DICT)
            throw ToRuntimeError(what + " expects a dict", line);
        ToDict out = d;
        for (size_t i = 0; i < args[0]->dictVal.size(); i++)
            out.setKey(args[0]->dictVal.keyAt(i), args[0]->dictVal[i].second);
        return ToValue::makeDict(std::move(out));
    }
    if (name == "map_values") {
        expectArgs(args, 1, what, "exactly 1 function", line);
        ToDict out;
        for (size_t i = 0; i < d.size(); i++)
            out.setKey(d.keyAt(i), invoke(interp, args[0], {d[i].second}, what, line));
        return ToValue::makeDict(std::move(out));
    }
    if (name == "invert") {
        ToDict out;
        for (size_t i = 0; i < d.size(); i++) out.setKey(d[i].second, d.keyAt(i));
        return ToValue::makeDict(std::move(out));
    }
    return nullptr;
}

// ------------------------------------------------------------
// string — every method hands back new text; strings never change
// ------------------------------------------------------------

ToValuePtr stringMethod(const ToValuePtr& recv, const std::string& name,
                        std::vector<ToValuePtr>& args, int line) {
    const std::string& s = recv->strVal;
    const std::string what = "string." + name + "()";

    auto needsText = [&](size_t n) {
        expectArgs(args, n, what, n == 1 ? "exactly 1 piece of text" : "2 pieces of text", line);
        for (size_t i = 0; i < n; i++)
            if (args[i]->type != ToValue::Type::STRING)
                throw ToRuntimeError(what + " works on text", line);
    };

    if (name == "upper" || name == "lower") {
        bool up = name == "upper";
        std::string r = s;
        for (auto& c : r) c = (char)(up ? std::toupper((unsigned char)c) : std::tolower((unsigned char)c));
        return ToValue::makeString(std::move(r));
    }
    if (name == "trim" || name == "trim_start" || name == "trim_end") {
        std::string r = s;
        if (name != "trim_end") {
            size_t b = r.find_first_not_of(" \t\n\r");
            r.erase(0, b == std::string::npos ? r.size() : b);
        }
        if (name != "trim_start") {
            size_t e = r.find_last_not_of(" \t\n\r");
            r.erase(e == std::string::npos ? 0 : e + 1);
        }
        return ToValue::makeString(std::move(r));
    }
    if (name == "split") {
        std::string sep = " ";
        if (!args.empty() && args[0]->type == ToValue::Type::STRING) sep = args[0]->strVal;
        std::vector<ToValuePtr> parts;
        if (sep.empty()) {
            for (char c : s) parts.push_back(ToValue::makeString(std::string(1, c)));
            return ToValue::makeList(std::move(parts));
        }
        size_t start = 0, pos;
        while ((pos = s.find(sep, start)) != std::string::npos) {
            parts.push_back(ToValue::makeString(s.substr(start, pos - start)));
            start = pos + sep.size();
        }
        parts.push_back(ToValue::makeString(s.substr(start)));
        return ToValue::makeList(std::move(parts));
    }
    if (name == "lines") {
        std::vector<ToValuePtr> parts;
        size_t start = 0, pos;
        while ((pos = s.find('\n', start)) != std::string::npos) {
            parts.push_back(ToValue::makeString(s.substr(start, pos - start)));
            start = pos + 1;
        }
        parts.push_back(ToValue::makeString(s.substr(start)));
        return ToValue::makeList(std::move(parts));
    }
    if (name == "chars") {
        std::vector<ToValuePtr> parts;
        parts.reserve(s.size());
        for (char c : s) parts.push_back(ToValue::makeString(std::string(1, c)));
        return ToValue::makeList(std::move(parts));
    }
    if (name == "replace") {
        needsText(2);
        std::string result = s, from = args[0]->strVal, to = args[1]->strVal;
        if (from.empty()) return ToValue::makeString(std::move(result));
        size_t pos = 0;
        while ((pos = result.find(from, pos)) != std::string::npos) {
            result.replace(pos, from.length(), to);
            pos += to.length();
        }
        return ToValue::makeString(std::move(result));
    }
    if (name == "starts_with") {
        needsText(1);
        return ToValue::makeBool(s.rfind(args[0]->strVal, 0) == 0);
    }
    if (name == "ends_with") {
        needsText(1);
        const std::string& suffix = args[0]->strVal;
        if (suffix.size() > s.size()) return ToValue::makeBool(false);
        return ToValue::makeBool(s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0);
    }
    if (name == "repeat") {
        expectArgs(args, 1, what, "exactly 1 whole number", line);
        int64_t n = asIndex(args[0], what, line);
        std::string r;
        for (int64_t i = 0; i < n; i++) r += s;
        return ToValue::makeString(std::move(r));
    }
    if (name == "pad_start" || name == "pad_end") {
        if (args.empty() || args.size() > 2) argError(what, "a width and optional padding text", line);
        int64_t width = asIndex(args[0], what, line);
        std::string pad = args.size() > 1 && args[1]->type == ToValue::Type::STRING
                          ? args[1]->strVal : " ";
        if (pad.empty()) pad = " ";
        std::string filler;
        while ((int64_t)(s.size() + filler.size()) < width) filler += pad;
        if ((int64_t)(s.size() + filler.size()) > width)
            filler.resize(width > (int64_t)s.size() ? width - s.size() : 0);
        return ToValue::makeString(name == "pad_start" ? filler + s : s + filler);
    }
    if (name == "capitalize") {
        std::string r = s;
        if (!r.empty()) r[0] = (char)std::toupper((unsigned char)r[0]);
        return ToValue::makeString(std::move(r));
    }
    if (name == "to_int") {
        try {
            size_t used = 0;
            int64_t v = std::stoll(s, &used);
            if (used == s.size()) return ToValue::makeInt(v);
        } catch (...) {}
        throw ToRuntimeError("Cannot read \"" + s + "\" as a whole number", line);
    }
    if (name == "to_float") {
        try {
            size_t used = 0;
            double v = std::stod(s, &used);
            if (used == s.size()) return ToValue::makeFloat(v);
        } catch (...) {}
        throw ToRuntimeError("Cannot read \"" + s + "\" as a decimal number", line);
    }
    return nullptr;
}

} // namespace

// ============================================================
// Public entry points
// ============================================================

ToValuePtr getBuiltinProperty(const ToValuePtr& recv, const std::string& name) {
    if (!recv) return nullptr;
    if (name == "length") {
        int64_t n = recv->length();
        if (n >= 0) return ToValue::makeInt(n);
    }
    if (recv->type == ToValue::Type::DICT) {
        auto found = recv->dictVal.get(name);
        if (found) return found;
    }
    if (recv->type == ToValue::Type::INSTANCE) {
        auto it = recv->instanceVal->fields.find(name);
        if (it != recv->instanceVal->fields.end()) return it->second;
    }
    return nullptr;
}

ToValuePtr callBuiltinMethod(const ToValuePtr& recv, const std::string& name,
                             std::vector<ToValuePtr>& args, Interpreter* interp, int line) {
    ToValuePtr result;

    if (recv->type == ToValue::Type::DICT) {
        result = dictMethod(recv, name, args, interp, line);
        if (result) return result;
    }
    if (recv->type == ToValue::Type::SET) {
        result = setMethod(recv, name, args, line);
        if (result) return result;
    }
    if (recv->type == ToValue::Type::STRING) {
        result = stringMethod(recv, name, args, line);
        if (result) return result;
    }
    if (recv->type == ToValue::Type::LIST || recv->isCollection()) {
        result = mutableMethod(recv, name, args, interp, line);
        if (result) return result;
    }
    if (recv->length() >= 0) {
        result = walkableMethod(recv, name, args, interp, line);
        if (result) return result;
    }

    reportUnknownMember(recv, name, line);
}

// ------------------------------------------------------------
// help()
// ------------------------------------------------------------

namespace {

const char* KIND_BLURBS[][2] = {
    {"list",   "values in a row, added and taken from the end"},
    {"tuple",  "a fixed row of values; cannot change, so it can be a dict key"},
    {"set",    "unique values, in the order they were first added"},
    {"dict",   "values stored under keys, in the order the keys arrived"},
    {"deque",  "a row you can add to and take from at either end"},
    {"queue",  "first one in is the first one out"},
    {"stack",  "last one in is the first one out"},
    {"heap",   "the smallest value always comes out first"},
    {"string", "text; every method hands back new text"},
};

std::string blurbFor(const std::string& kind) {
    for (auto& b : KIND_BLURBS)
        if (kind == b[0]) return b[1];
    return "";
}

} // namespace

std::string helpText(const ToValuePtr& value) {
    std::string out;

    if (!value || typeBitOf(value) == 0) {
        out += "\nto — collections\n\n";
        out += "  [1, 2, 3]        list    values in a row\n";
        out += "  (1, 2, 3)        tuple   a fixed row; can be a dict key\n";
        out += "  {1, 2, 3}        set     unique values\n";
        out += "  {name = \"Theo\"}  dict    values stored under keys\n";
        out += "  queue()          queue   first in, first out\n";
        out += "  stack()          stack   last in, first out\n";
        out += "  deque()          deque   add and take at either end\n";
        out += "  heap()           heap    smallest comes out first\n\n";
        out += "The same words work on all of them:\n\n";
        out += "  add(value)     put one in\n";
        out += "  pop()          take the next one out\n";
        out += "  peek()         look at the next one without taking it\n";
        out += "  has(value)     is it in there\n";
        out += "  remove(value)  take that one out\n";
        out += "  .length        how many\n\n";
        out += "Which one comes out of pop() is the only difference:\n";
        out += "a stack gives the newest, a queue the oldest, a heap the smallest.\n\n";
        out += "Run help(x) on any value to see everything it can do.\n";
        return out;
    }

    const std::string kind = value->typeName();
    uint32_t bit = typeBitOf(value);
    out += "\n" + kind + " — " + blurbFor(kind) + "\n";

    std::string currentGroup;
    for (auto& m : METHODS) {
        if (!(m.types & bit)) continue;
        if (currentGroup != m.group) {
            currentGroup = m.group;
            out += "\n  " + currentGroup + "\n";
        }
        std::string call = m.call;
        out += "    " + call;
        for (size_t i = call.size(); i < 24; i++) out += ' ';
        out += m.summary;
        out += "\n";
    }
    return out;
}

// ------------------------------------------------------------
// Indexing
// ------------------------------------------------------------

ToValuePtr indexGet(const ToValuePtr& obj, const ToValuePtr& index, int line) {
    switch (obj->type) {
        case ToValue::Type::LIST:
        case ToValue::Type::TUPLE: {
            int64_t len = (int64_t)obj->listVal.size();
            int64_t i = normalizeIndex(asIndex(index, obj->typeName(), line), len);
            if (i < 0)
                throw ToRuntimeError("There is no position " + index->toString() + " in a " +
                                     obj->typeName() + " of " + std::to_string(len) +
                                     " — positions run 0 to " + std::to_string(len - 1), line);
            return obj->listVal[i];
        }
        case ToValue::Type::STRING: {
            int64_t len = (int64_t)obj->strVal.size();
            int64_t i = normalizeIndex(asIndex(index, "string", line), len);
            if (i < 0)
                throw ToRuntimeError("There is no position " + index->toString() +
                                     " in text of length " + std::to_string(len), line);
            return ToValue::makeString(std::string(1, obj->strVal[i]));
        }
        case ToValue::Type::DICT: {
            if (!valueHashable(index))
                throw ToRuntimeError("A " + index->typeName() + " cannot be a dict key, because it "
                                     "can change. Use a tuple instead, like (1, 2).", line);
            auto found = obj->dictVal.getKey(index);
            if (!found)
                throw ToRuntimeError("No key " + index->repr() +
                                     " in this dict — use get(key, fallback) when it might be missing",
                                     line);
            return found;
        }
        case ToValue::Type::DEQUE:
        case ToValue::Type::QUEUE:
        case ToValue::Type::STACK: {
            auto& items = obj->collVal->items;
            int64_t i = normalizeIndex(asIndex(index, obj->typeName(), line), (int64_t)items.size());
            if (i < 0)
                throw ToRuntimeError("There is no position " + index->toString() + " in a " +
                                     obj->typeName() + " of " + std::to_string(items.size()), line);
            return items[i];
        }
        case ToValue::Type::SET:
            throw ToRuntimeError("A set has no positions — use has(value), or to_list() first", line);
        case ToValue::Type::HEAP:
            throw ToRuntimeError("A heap has no positions — use peek(), or to_list() first", line);
        default:
            throw ToRuntimeError("Cannot look inside a " + obj->typeName() + " with [ ]", line);
    }
}

void indexSet(const ToValuePtr& obj, const ToValuePtr& index, ToValuePtr value, int line) {
    switch (obj->type) {
        case ToValue::Type::LIST: {
            int64_t len = (int64_t)obj->listVal.size();
            int64_t i = normalizeIndex(asIndex(index, "list", line), len);
            if (i < 0)
                throw ToRuntimeError("There is no position " + index->toString() + " in a list of " +
                                     std::to_string(len) + " — use add() to grow it", line);
            obj->listVal[i] = std::move(value);
            return;
        }
        case ToValue::Type::DICT:
            if (!valueHashable(index))
                throw ToRuntimeError("A " + index->typeName() + " cannot be a dict key, because it "
                                     "can change. Use a tuple instead, like (1, 2).", line);
            obj->dictVal.setKey(index, std::move(value));
            return;
        case ToValue::Type::DEQUE:
        case ToValue::Type::QUEUE:
        case ToValue::Type::STACK: {
            auto& items = obj->collVal->items;
            int64_t i = normalizeIndex(asIndex(index, obj->typeName(), line), (int64_t)items.size());
            if (i < 0)
                throw ToRuntimeError("There is no position " + index->toString() + " in a " +
                                     obj->typeName(), line);
            items[i] = std::move(value);
            return;
        }
        case ToValue::Type::TUPLE:
            throw ToRuntimeError("A tuple cannot be changed — call to_list() first", line);
        case ToValue::Type::SET:
            throw ToRuntimeError("A set has no positions — use add(value) instead", line);
        case ToValue::Type::HEAP:
            throw ToRuntimeError("A heap decides its own order — use add(value) instead", line);
        case ToValue::Type::STRING:
            throw ToRuntimeError("Text cannot be changed in place — build a new string instead", line);
        default:
            throw ToRuntimeError("Cannot assign into a " + obj->typeName() + " with [ ]", line);
    }
}

ToValuePtr sliceValue(const ToValuePtr& obj, const ToValuePtr& start,
                      const ToValuePtr& end, const ToValuePtr& step, int line) {
    int64_t len = obj->length();
    if (len < 0 || obj->type == ToValue::Type::DICT)
        throw ToRuntimeError("Cannot take a slice of a " + obj->typeName(), line);

    int64_t stride = 1;
    if (step && step->type != ToValue::Type::NONE) {
        stride = asIndex(step, "slice step", line);
        if (stride == 0) throw ToRuntimeError("A slice step cannot be 0", line);
    }

    auto bound = [&](const ToValuePtr& v, int64_t fallback) {
        if (!v || v->type == ToValue::Type::NONE) return fallback;
        int64_t i = asIndex(v, "slice bound", line);
        if (i < 0) i += len;
        return i;
    };

    int64_t from = bound(start, stride > 0 ? 0 : len - 1);
    int64_t to = bound(end, stride > 0 ? len : -1);
    if (stride > 0) {
        from = std::max<int64_t>(from, 0);
        to = std::min<int64_t>(to, len);
    } else {
        from = std::min<int64_t>(from, len - 1);
        to = std::max<int64_t>(to, -1);
    }

    if (obj->type == ToValue::Type::STRING) {
        std::string out;
        for (int64_t i = from; stride > 0 ? i < to : i > to; i += stride)
            if (i >= 0 && i < len) out += obj->strVal[i];
        return ToValue::makeString(std::move(out));
    }

    auto items = obj->elements();
    std::vector<ToValuePtr> out;
    for (int64_t i = from; stride > 0 ? i < to : i > to; i += stride)
        if (i >= 0 && i < (int64_t)items.size()) out.push_back(items[i]);

    switch (obj->type) {
        case ToValue::Type::TUPLE: return ToValue::makeTuple(std::move(out));
        case ToValue::Type::DEQUE: return ToValue::makeDeque(std::move(out));
        case ToValue::Type::QUEUE: return ToValue::makeQueue(std::move(out));
        case ToValue::Type::STACK: return ToValue::makeStack(std::move(out));
        default: return ToValue::makeList(std::move(out));
    }
}

// ------------------------------------------------------------
// Operators
// ------------------------------------------------------------

ToValuePtr applyUnaryOp(const std::string& op, const ToValuePtr& operand, int line) {
    if (op == "-") {
        if (operand->type == ToValue::Type::INT) return ToValue::makeInt(-operand->intVal);
        if (operand->type == ToValue::Type::FLOAT) return ToValue::makeFloat(-operand->floatVal);
        throw ToRuntimeError("Cannot negate a " + operand->typeName(), line);
    }
    if (op == "not") return ToValue::makeBool(!operand->isTruthy());
    throw ToRuntimeError("Unknown unary operator: " + op, line);
}

BinOp binOpFor(const std::string& op) {
    if (op.size() == 1) {
        switch (op[0]) {
            case '+': return BinOp::Add;
            case '-': return BinOp::Sub;
            case '*': return BinOp::Mul;
            case '/': return BinOp::Div;
            case '%': return BinOp::Mod;
            case '<': return BinOp::Lt;
            case '>': return BinOp::Gt;
            default: return BinOp::Unknown;
        }
    }
    if (op == "==") return BinOp::Eq;
    if (op == "!=") return BinOp::Neq;
    if (op == "<=") return BinOp::Lte;
    if (op == ">=") return BinOp::Gte;
    return BinOp::Unknown;
}

const char* binOpName(BinOp op) {
    static const char* names[] = {"+", "-", "*", "/", "%", "==", "!=", "<", "<=", ">", ">=", "?"};
    return names[(int)op];
}

ToValuePtr applyBinaryOp(const std::string& op, const ToValuePtr& left,
                         const ToValuePtr& right, Interpreter* interp, int line) {
    BinOp tag = binOpFor(op);
    if (tag == BinOp::Unknown)
        throw ToRuntimeError("Unknown operator '" + op + "'", line);
    return applyBinaryOp(tag, left, right, interp, line);
}

ToValuePtr applyBinaryOp(BinOp op, const ToValuePtr& left,
                         const ToValuePtr& right, Interpreter* interp, int line) {
    // Numbers first — this is the hot path for every arithmetic loop.
    bool leftNum = left->type == ToValue::Type::INT || left->type == ToValue::Type::FLOAT;
    bool rightNum = right->type == ToValue::Type::INT || right->type == ToValue::Type::FLOAT;
    if (leftNum && rightNum) {
        if (left->type == ToValue::Type::INT && right->type == ToValue::Type::INT) {
            int64_t a = left->intVal, b = right->intVal;
            switch (op) {
                case BinOp::Add: return ToValue::makeInt(a + b);
                case BinOp::Sub: return ToValue::makeInt(a - b);
                case BinOp::Mul: return ToValue::makeInt(a * b);
                case BinOp::Div:
                    if (b == 0) throw ToRuntimeError("Cannot divide by zero", line);
                    if (a % b == 0) return ToValue::makeInt(a / b);
                    return ToValue::makeFloat((double)a / (double)b);
                case BinOp::Mod:
                    if (b == 0) throw ToRuntimeError("Cannot take the remainder of a division by zero", line);
                    return ToValue::makeInt(a % b);
                case BinOp::Eq:  return ToValue::makeBool(a == b);
                case BinOp::Neq: return ToValue::makeBool(a != b);
                case BinOp::Lt:  return ToValue::makeBool(a < b);
                case BinOp::Lte: return ToValue::makeBool(a <= b);
                case BinOp::Gt:  return ToValue::makeBool(a > b);
                case BinOp::Gte: return ToValue::makeBool(a >= b);
                default: break;
            }
        }
        double lv = left->type == ToValue::Type::INT ? (double)left->intVal : left->floatVal;
        double rv = right->type == ToValue::Type::INT ? (double)right->intVal : right->floatVal;
        switch (op) {
            case BinOp::Add: return ToValue::makeFloat(lv + rv);
            case BinOp::Sub: return ToValue::makeFloat(lv - rv);
            case BinOp::Mul: return ToValue::makeFloat(lv * rv);
            case BinOp::Div:
                if (rv == 0) throw ToRuntimeError("Cannot divide by zero", line);
                return ToValue::makeFloat(lv / rv);
            case BinOp::Mod:
                if (rv == 0) throw ToRuntimeError("Cannot take the remainder of a division by zero", line);
                return ToValue::makeFloat(std::fmod(lv, rv));
            case BinOp::Eq:  return ToValue::makeBool(lv == rv);
            case BinOp::Neq: return ToValue::makeBool(lv != rv);
            case BinOp::Lt:  return ToValue::makeBool(lv < rv);
            case BinOp::Lte: return ToValue::makeBool(lv <= rv);
            case BinOp::Gt:  return ToValue::makeBool(lv > rv);
            case BinOp::Gte: return ToValue::makeBool(lv >= rv);
            default: break;
        }
    }

    // Operator overloading: a class that defines plus/minus/equals/... answers first.
    if (left->type == ToValue::Type::INSTANCE && interp) {
        std::string methodName;
        bool invert = false;
        switch (op) {
            case BinOp::Add: methodName = "plus"; break;
            case BinOp::Sub: methodName = "minus"; break;
            case BinOp::Mul: methodName = "times"; break;
            case BinOp::Div: methodName = "divide"; break;
            case BinOp::Mod: methodName = "mod"; break;
            case BinOp::Eq:  methodName = "equals"; break;
            case BinOp::Neq: methodName = "equals"; invert = true; break;
            case BinOp::Lt:  methodName = "less_than"; break;
            case BinOp::Lte: methodName = "less_equal"; break;
            case BinOp::Gt:  methodName = "greater_than"; break;
            case BinOp::Gte: methodName = "greater_equal"; break;
            default: break;
        }
        if (!methodName.empty()) {
            bool found = false;
            auto result = interp->callInstanceMethod(left, methodName, {right}, line, &found);
            if (found) {
                if (invert && result->type == ToValue::Type::BOOL)
                    return ToValue::makeBool(!result->boolVal);
                return result;
            }
        }
    }

    // String building
    if (op == BinOp::Add &&
        (left->type == ToValue::Type::STRING || right->type == ToValue::Type::STRING))
        return ToValue::makeString(left->toString() + right->toString());

    // Equality is structural: two lists with equal values are equal, and so
    // are two sets with the same members.
    if (op == BinOp::Eq)  return ToValue::makeBool(valueEquals(left, right));
    if (op == BinOp::Neq) return ToValue::makeBool(!valueEquals(left, right));

    // Ordering, for values that are actually comparable to each other.
    if (left->type == right->type &&
        (left->type == ToValue::Type::STRING || left->length() >= 0)) {
        int c = valueCompare(left, right);
        if (op == BinOp::Lt)  return ToValue::makeBool(c < 0);
        if (op == BinOp::Lte) return ToValue::makeBool(c <= 0);
        if (op == BinOp::Gt)  return ToValue::makeBool(c > 0);
        if (op == BinOp::Gte) return ToValue::makeBool(c >= 0);
    }

    // Joining and removing across collections.
    if (op == BinOp::Add || op == BinOp::Sub) {
        if (left->type == ToValue::Type::SET && right->type == ToValue::Type::SET) {
            std::vector<ToValuePtr> out;
            if (op == BinOp::Add) {
                out = left->elements();
                for (auto& v : right->elements()) out.push_back(v);
            } else {
                for (auto& v : left->elements())
                    if (!coll(right)->has(v)) out.push_back(v);
            }
            return ToValue::makeSet(std::move(out));
        }
        if (op == BinOp::Add && left->type == right->type && left->type == ToValue::Type::DICT) {
            ToDict out = left->dictVal;
            for (size_t i = 0; i < right->dictVal.size(); i++)
                out.setKey(right->dictVal.keyAt(i), right->dictVal[i].second);
            return ToValue::makeDict(std::move(out));
        }
        if (op == BinOp::Add && left->type == right->type && left->length() >= 0) {
            auto out = left->elements();
            for (auto& v : right->elements()) out.push_back(v);
            switch (left->type) {
                case ToValue::Type::TUPLE: return ToValue::makeTuple(std::move(out));
                case ToValue::Type::DEQUE: return ToValue::makeDeque(std::move(out));
                case ToValue::Type::QUEUE: return ToValue::makeQueue(std::move(out));
                case ToValue::Type::STACK: return ToValue::makeStack(std::move(out));
                default: return ToValue::makeList(std::move(out));
            }
        }
    }

    // Repetition: [0] * 3
    if (op == BinOp::Mul && right->type == ToValue::Type::INT && left->isSequence()) {
        std::vector<ToValuePtr> out;
        for (int64_t n = 0; n < right->intVal; n++)
            for (auto& v : left->listVal) out.push_back(v);
        return left->type == ToValue::Type::TUPLE ? ToValue::makeTuple(std::move(out))
                                                  : ToValue::makeList(std::move(out));
    }

    throw ToRuntimeError(std::string("Cannot use '") + binOpName(op) + "' between " +
        withArticle(left->typeName()) + " and " + withArticle(right->typeName()), line);
}

// ------------------------------------------------------------
// Constructors, exposed as globals
// ------------------------------------------------------------

void registerCollectionBuiltins(EnvPtr env) {
    // Every constructor takes nothing, or one collection to copy from.
    auto seed = [](const std::vector<ToValuePtr>& args) {
        if (args.empty()) return std::vector<ToValuePtr>{};
        if (args.size() == 1 && args[0]->length() >= 0) return args[0]->elements();
        return args;
    };

    env->define("set", ToValue::makeBuiltin([seed](std::vector<ToValuePtr> args) {
        return ToValue::makeSet(seed(args));
    }));
    env->define("tuple", ToValue::makeBuiltin([seed](std::vector<ToValuePtr> args) {
        return ToValue::makeTuple(seed(args));
    }));
    env->define("deque", ToValue::makeBuiltin([seed](std::vector<ToValuePtr> args) {
        return ToValue::makeDeque(seed(args));
    }));
    env->define("queue", ToValue::makeBuiltin([seed](std::vector<ToValuePtr> args) {
        return ToValue::makeQueue(seed(args));
    }));
    env->define("stack", ToValue::makeBuiltin([seed](std::vector<ToValuePtr> args) {
        return ToValue::makeStack(seed(args));
    }));
    env->define("heap", ToValue::makeBuiltin([seed](std::vector<ToValuePtr> args) {
        return ToValue::makeHeap(seed(args), false);
    }));
    env->define("max_heap", ToValue::makeBuiltin([seed](std::vector<ToValuePtr> args) {
        return ToValue::makeHeap(seed(args), true);
    }));
    env->define("list", ToValue::makeBuiltin([seed](std::vector<ToValuePtr> args) {
        return ToValue::makeList(seed(args));
    }));
    env->define("dict", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) {
        ToDict d;
        if (args.size() == 1 && args[0]->type == ToValue::Type::DICT) {
            d = args[0]->dictVal;
        } else if (args.size() == 1 && args[0]->length() >= 0) {
            for (auto& pair : args[0]->elements()) {
                auto kv = pair->elements();
                if (kv.size() != 2)
                    throw ToRuntimeError("dict() builds from (key, value) pairs");
                d.setKey(kv[0], kv[1]);
            }
        } else if (!args.empty()) {
            throw ToRuntimeError("dict() takes a dict, or a list of (key, value) pairs");
        }
        return ToValue::makeDict(std::move(d));
    }));

    // help() — the whole point of which is that you never have to leave
    // the editor to remember a method name.
    env->define("help", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) {
        printf("%s", helpText(args.empty() ? nullptr : args[0]).c_str());
        return ToValue::makeNone();
    }));
}
