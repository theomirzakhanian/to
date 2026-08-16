// ============================================================
// methods.cpp — every built-in method, in one place
// ------------------------------------------------------------
// Both the tree-walking interpreter and the bytecode VM dispatch
// method calls through callBuiltinMethod(), so `xs.sort()` behaves
// identically under `to run` and `to fast`.
// ============================================================
#include "methods.h"
#include "interpreter.h"
#include "error.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>

// ------------------------------------------------------------
// Small helpers
// ------------------------------------------------------------

namespace {

[[noreturn]] void argError(const std::string& what, const std::string& expected, int line) {
    throw ToRuntimeError(what + " takes " + expected, line);
}

void expectArgs(const std::vector<ToValuePtr>& args, size_t n,
                const std::string& what, const std::string& expected, int line) {
    if (args.size() != n) argError(what, expected, line);
}

int64_t asIndex(const ToValuePtr& v, const std::string& what, int line) {
    if (!v || v->type != ToValue::Type::INT)
        throw ToRuntimeError(what + " expects an integer index", line);
    return v->intVal;
}

// Resolve a possibly-negative index against a length. Returns -1 when out of range.
int64_t normalizeIndex(int64_t i, int64_t len) {
    if (i < 0) i += len;
    if (i < 0 || i >= len) return -1;
    return i;
}

ToValuePtr invoke(Interpreter* interp, const ToValuePtr& fn,
                  std::vector<ToValuePtr> args, const std::string& what, int line) {
    if (!fn || (fn->type != ToValue::Type::FUNCTION && fn->type != ToValue::Type::BUILTIN &&
                fn->type != ToValue::Type::CLASS))
        throw ToRuntimeError(what + " expects a function", line);
    if (fn->type == ToValue::Type::BUILTIN) return fn->builtinVal(std::move(args));
    if (!interp)
        throw ToRuntimeError(what + " cannot call a function here", line);
    return interp->callFunction(fn, args, line);
}

ToCollectionPtr coll(const ToValuePtr& v) {
    if (!v->collVal) v->collVal = std::make_shared<ToCollection>();
    return v->collVal;
}

std::string lengthlessName(const ToValuePtr& v) { return v->typeName(); }

// A comparator built from an optional key function.
struct KeyedLess {
    Interpreter* interp;
    ToValuePtr keyFn;
    int line;
    bool descending = false;

    bool operator()(const ToValuePtr& a, const ToValuePtr& b) const {
        ToValuePtr ka = a, kb = b;
        if (keyFn) {
            ka = invoke(interp, keyFn, {a}, "sort key", line);
            kb = invoke(interp, keyFn, {b}, "sort key", line);
        }
        int c = valueCompare(ka, kb);
        return descending ? c > 0 : c < 0;
    }
};

std::vector<ToValuePtr> sortedCopy(const std::vector<ToValuePtr>& items,
                                   Interpreter* interp, const std::vector<ToValuePtr>& args,
                                   bool descending, int line) {
    std::vector<ToValuePtr> out = items;
    KeyedLess less{interp, args.empty() ? nullptr : args[0], line, descending};
    std::stable_sort(out.begin(), out.end(), less);
    return out;
}

// ------------------------------------------------------------
// Methods shared by every ordered value
// ------------------------------------------------------------

// Handles the methods that read a value without caring what kind of
// collection it is. Returns null when `name` is not one of them.
ToValuePtr commonSequenceMethod(const ToValuePtr& recv, const std::string& name,
                                std::vector<ToValuePtr>& args, Interpreter* interp, int line) {
    const std::string what = recv->typeName() + "." + name + "()";

    if (name == "length" || name == "size" || name == "count_all") {
        return ToValue::makeInt(recv->length());
    }
    if (name == "is_empty") {
        return ToValue::makeBool(recv->length() == 0);
    }
    if (name == "to_list") {
        return ToValue::makeList(recv->elements());
    }
    if (name == "to_tuple") {
        return ToValue::makeTuple(recv->elements());
    }
    if (name == "to_set") {
        return ToValue::makeSet(recv->elements());
    }
    if (name == "to_deque") {
        return ToValue::makeDeque(recv->elements());
    }
    if (name == "to_queue") {
        return ToValue::makeQueue(recv->elements());
    }
    if (name == "to_stack") {
        return ToValue::makeStack(recv->elements());
    }
    if (name == "to_heap") {
        return ToValue::makeHeap(recv->elements());
    }
    if (name == "contains" || name == "has") {
        expectArgs(args, 1, what, "exactly 1 argument", line);
        if (recv->type == ToValue::Type::SET)
            return ToValue::makeBool(coll(recv)->has(args[0]));
        for (auto& item : recv->elements())
            if (valueEquals(item, args[0])) return ToValue::makeBool(true);
        return ToValue::makeBool(false);
    }
    if (name == "index_of") {
        expectArgs(args, 1, what, "exactly 1 argument", line);
        auto items = recv->elements();
        for (size_t i = 0; i < items.size(); i++)
            if (valueEquals(items[i], args[0])) return ToValue::makeInt((int64_t)i);
        return ToValue::makeInt(-1);
    }
    if (name == "count") {
        expectArgs(args, 1, what, "exactly 1 argument", line);
        int64_t n = 0;
        for (auto& item : recv->elements())
            if (valueEquals(item, args[0])) n++;
        return ToValue::makeInt(n);
    }
    if (name == "first") {
        auto items = recv->elements();
        return items.empty() ? ToValue::makeNone() : items.front();
    }
    if (name == "last") {
        auto items = recv->elements();
        return items.empty() ? ToValue::makeNone() : items.back();
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
        expectArgs(args, 1, what, "exactly 1 function argument", line);
        std::vector<ToValuePtr> out;
        auto items = recv->elements();
        out.reserve(items.size());
        for (auto& item : items) out.push_back(invoke(interp, args[0], {item}, what, line));
        return ToValue::makeList(std::move(out));
    }
    if (name == "filter") {
        expectArgs(args, 1, what, "exactly 1 function argument", line);
        std::vector<ToValuePtr> out;
        for (auto& item : recv->elements())
            if (invoke(interp, args[0], {item}, what, line)->isTruthy()) out.push_back(item);
        return ToValue::makeList(std::move(out));
    }
    if (name == "reduce" || name == "fold") {
        if (args.empty() || args.size() > 2) argError(what, "a function and an optional initial value", line);
        auto items = recv->elements();
        size_t start = 0;
        ToValuePtr acc;
        if (args.size() == 2) {
            acc = args[1];
        } else {
            if (items.empty()) throw ToRuntimeError(what + " on an empty " + recv->typeName() +
                                                    " needs an initial value", line);
            acc = items[0];
            start = 1;
        }
        for (size_t i = start; i < items.size(); i++)
            acc = invoke(interp, args[0], {acc, items[i]}, what, line);
        return acc;
    }
    if (name == "each" || name == "for_each") {
        expectArgs(args, 1, what, "exactly 1 function argument", line);
        for (auto& item : recv->elements()) invoke(interp, args[0], {item}, what, line);
        return ToValue::makeNone();
    }
    if (name == "find") {
        expectArgs(args, 1, what, "exactly 1 function argument", line);
        for (auto& item : recv->elements())
            if (invoke(interp, args[0], {item}, what, line)->isTruthy()) return item;
        return ToValue::makeNone();
    }
    if (name == "any") {
        expectArgs(args, 1, what, "exactly 1 function argument", line);
        for (auto& item : recv->elements())
            if (invoke(interp, args[0], {item}, what, line)->isTruthy()) return ToValue::makeBool(true);
        return ToValue::makeBool(false);
    }
    if (name == "all") {
        expectArgs(args, 1, what, "exactly 1 function argument", line);
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
            else throw ToRuntimeError(what + " needs numbers, found " + item->typeName(), line);
        }
        return useFloat ? ToValue::makeFloat(fsum) : ToValue::makeInt(isum);
    }
    if (name == "min" || name == "max") {
        auto items = recv->elements();
        if (items.empty()) throw ToRuntimeError(what + " on an empty " + recv->typeName(), line);
        KeyedLess less{interp, args.empty() ? nullptr : args[0], line, false};
        auto it = name == "min" ? std::min_element(items.begin(), items.end(), less)
                                : std::max_element(items.begin(), items.end(), less);
        return *it;
    }
    if (name == "sorted") {
        return ToValue::makeList(sortedCopy(recv->elements(), interp, args, false, line));
    }
    if (name == "sorted_desc") {
        return ToValue::makeList(sortedCopy(recv->elements(), interp, args, true, line));
    }
    if (name == "reversed") {
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
    if (name == "take" || name == "drop") {
        expectArgs(args, 1, what, "exactly 1 integer argument", line);
        int64_t n = asIndex(args[0], what, line);
        auto items = recv->elements();
        if (n < 0) n = 0;
        if (n > (int64_t)items.size()) n = (int64_t)items.size();
        std::vector<ToValuePtr> out;
        if (name == "take") out.assign(items.begin(), items.begin() + n);
        else out.assign(items.begin() + n, items.end());
        return ToValue::makeList(std::move(out));
    }
    if (name == "chunk") {
        expectArgs(args, 1, what, "exactly 1 integer argument", line);
        int64_t n = asIndex(args[0], what, line);
        if (n <= 0) throw ToRuntimeError(what + " needs a positive chunk size", line);
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
        expectArgs(args, 1, what, "exactly 1 argument", line);
        auto a = recv->elements();
        auto b = args[0]->elements();
        std::vector<ToValuePtr> out;
        size_t n = std::min(a.size(), b.size());
        for (size_t i = 0; i < n; i++) out.push_back(ToValue::makeTuple({a[i], b[i]}));
        return ToValue::makeList(std::move(out));
    }
    if (name == "slice") {
        if (args.empty() || args.size() > 3) argError(what, "1 to 3 integer arguments", line);
        return sliceValue(recv, args.size() > 0 ? args[0] : nullptr,
                          args.size() > 1 ? args[1] : nullptr,
                          args.size() > 2 ? args[2] : nullptr, line);
    }
    return nullptr;
}

// ------------------------------------------------------------
// list
// ------------------------------------------------------------

ToValuePtr listMethod(const ToValuePtr& recv, const std::string& name,
                      std::vector<ToValuePtr>& args, Interpreter* interp, int line) {
    auto& items = recv->listVal;
    const std::string what = "list." + name + "()";

    if (name == "add" || name == "push" || name == "append") {
        if (args.empty()) argError(what, "at least 1 argument", line);
        for (auto& a : args) items.push_back(a);
        return ToValue::makeNone();
    }
    if (name == "insert") {
        expectArgs(args, 2, what, "an index and a value", line);
        int64_t i = asIndex(args[0], what, line);
        if (i < 0) i += (int64_t)items.size();
        if (i < 0) i = 0;
        if (i > (int64_t)items.size()) i = (int64_t)items.size();
        items.insert(items.begin() + i, args[1]);
        return ToValue::makeNone();
    }
    if (name == "remove") {
        // Historic behaviour: remove by index, not by value.
        expectArgs(args, 1, what, "exactly 1 argument", line);
        int64_t i = normalizeIndex(asIndex(args[0], what, line), (int64_t)items.size());
        if (i < 0) throw ToRuntimeError("list.remove() index out of bounds", line);
        items.erase(items.begin() + i);
        return ToValue::makeNone();
    }
    if (name == "remove_value") {
        expectArgs(args, 1, what, "exactly 1 argument", line);
        for (size_t i = 0; i < items.size(); i++) {
            if (valueEquals(items[i], args[0])) {
                items.erase(items.begin() + i);
                return ToValue::makeBool(true);
            }
        }
        return ToValue::makeBool(false);
    }
    if (name == "pop") {
        if (items.empty()) throw ToRuntimeError("pop from empty list", line);
        if (args.empty()) {
            auto val = items.back();
            items.pop_back();
            return val;
        }
        int64_t i = normalizeIndex(asIndex(args[0], what, line), (int64_t)items.size());
        if (i < 0) throw ToRuntimeError("list.pop() index out of bounds", line);
        auto val = items[i];
        items.erase(items.begin() + i);
        return val;
    }
    if (name == "reverse") {
        std::reverse(items.begin(), items.end());
        return ToValue::makeNone();
    }
    if (name == "sort") {
        KeyedLess less{interp, args.empty() ? nullptr : args[0], line, false};
        std::stable_sort(items.begin(), items.end(), less);
        return ToValue::makeNone();
    }
    if (name == "sort_desc") {
        KeyedLess less{interp, args.empty() ? nullptr : args[0], line, true};
        std::stable_sort(items.begin(), items.end(), less);
        return ToValue::makeNone();
    }
    if (name == "extend" || name == "concat") {
        expectArgs(args, 1, what, "exactly 1 argument", line);
        auto other = args[0]->elements();
        if (name == "extend") {
            items.insert(items.end(), other.begin(), other.end());
            return ToValue::makeNone();
        }
        std::vector<ToValuePtr> out = items;
        out.insert(out.end(), other.begin(), other.end());
        return ToValue::makeList(std::move(out));
    }
    if (name == "clear") {
        items.clear();
        return ToValue::makeNone();
    }
    if (name == "copy") {
        return ToValue::makeList(items);
    }
    if (name == "fill") {
        expectArgs(args, 1, what, "exactly 1 argument", line);
        for (auto& item : items) item = args[0];
        return ToValue::makeNone();
    }
    return nullptr;
}

// ------------------------------------------------------------
// tuple — immutable, so only the read-only methods apply
// ------------------------------------------------------------

ToValuePtr tupleMethod(const ToValuePtr& recv, const std::string& name,
                       std::vector<ToValuePtr>& args, Interpreter*, int line) {
    if (name == "copy") return ToValue::makeTuple(recv->listVal);
    static const char* mutators[] = {"add", "push", "append", "insert", "remove", "pop",
                                     "sort", "clear", "extend", "reverse", "fill"};
    for (auto m : mutators) {
        if (name == m)
            throw ToRuntimeError("A tuple cannot be changed — use to_list() first "
                                 "(tuple." + name + "() does not exist)", line);
    }
    (void)args;
    return nullptr;
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
    if (name == "entries" || name == "items") {
        std::vector<ToValuePtr> out;
        out.reserve(d.size());
        for (size_t i = 0; i < d.size(); i++)
            out.push_back(ToValue::makeTuple({d.keyAt(i), d[i].second}));
        return ToValue::makeList(std::move(out));
    }
    if (name == "has" || name == "contains" || name == "has_key") {
        expectArgs(args, 1, what, "exactly 1 argument", line);
        if (!valueHashable(args[0])) return ToValue::makeBool(false);
        return ToValue::makeBool(d.containsKey(args[0]));
    }
    if (name == "get") {
        if (args.empty() || args.size() > 2) argError(what, "a key and an optional default", line);
        if (!valueHashable(args[0]))
            return args.size() > 1 ? args[1] : ToValue::makeNone();
        auto found = d.getKey(args[0]);
        if (found) return found;
        return args.size() > 1 ? args[1] : ToValue::makeNone();
    }
    if (name == "set") {
        expectArgs(args, 2, what, "a key and a value", line);
        d.setKey(args[0], args[1]);
        return ToValue::makeNone();
    }
    if (name == "remove" || name == "delete") {
        expectArgs(args, 1, what, "exactly 1 argument", line);
        if (!valueHashable(args[0])) return ToValue::makeBool(false);
        return ToValue::makeBool(d.eraseKey(args[0]));
    }
    if (name == "merge" || name == "update") {
        expectArgs(args, 1, what, "exactly 1 dict argument", line);
        if (args[0]->type != ToValue::Type::DICT)
            throw ToRuntimeError(what + " expects a dict", line);
        if (name == "update") {
            for (size_t i = 0; i < args[0]->dictVal.size(); i++)
                d.setKey(args[0]->dictVal.keyAt(i), args[0]->dictVal[i].second);
            return ToValue::makeNone();
        }
        ToDict out = d;
        for (size_t i = 0; i < args[0]->dictVal.size(); i++)
            out.setKey(args[0]->dictVal.keyAt(i), args[0]->dictVal[i].second);
        return ToValue::makeDict(std::move(out));
    }
    if (name == "clear") {
        d.clear();
        return ToValue::makeNone();
    }
    if (name == "copy") {
        return ToValue::makeDict(d);
    }
    if (name == "length" || name == "size") return ToValue::makeInt((int64_t)d.size());
    if (name == "is_empty") return ToValue::makeBool(d.empty());
    if (name == "each" || name == "for_each") {
        expectArgs(args, 1, what, "exactly 1 function argument", line);
        for (size_t i = 0; i < d.size(); i++)
            invoke(interp, args[0], {d.keyAt(i), d[i].second}, what, line);
        return ToValue::makeNone();
    }
    if (name == "map_values") {
        expectArgs(args, 1, what, "exactly 1 function argument", line);
        ToDict out;
        for (size_t i = 0; i < d.size(); i++)
            out.setKey(d.keyAt(i), invoke(interp, args[0], {d[i].second}, what, line));
        return ToValue::makeDict(std::move(out));
    }
    if (name == "filter") {
        expectArgs(args, 1, what, "exactly 1 function argument", line);
        ToDict out;
        for (size_t i = 0; i < d.size(); i++) {
            auto key = d.keyAt(i);
            if (invoke(interp, args[0], {key, d[i].second}, what, line)->isTruthy())
                out.setKey(key, d[i].second);
        }
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
// set
// ------------------------------------------------------------

ToValuePtr setMethod(const ToValuePtr& recv, const std::string& name,
                     std::vector<ToValuePtr>& args, Interpreter*, int line) {
    auto c = coll(recv);
    const std::string what = "set." + name + "()";

    if (name == "add") {
        if (args.empty()) argError(what, "at least 1 argument", line);
        bool added = false;
        for (auto& a : args) added = c->insert(a) || added;
        return ToValue::makeBool(added);
    }
    if (name == "remove" || name == "discard" || name == "delete") {
        expectArgs(args, 1, what, "exactly 1 argument", line);
        return ToValue::makeBool(c->discard(args[0]));
    }
    if (name == "pop") {
        if (c->items.empty()) throw ToRuntimeError("pop from empty set", line);
        auto val = c->items.front();
        c->discard(val);
        return val;
    }
    if (name == "clear") {
        c->items.clear();
        c->index.clear();
        return ToValue::makeNone();
    }
    if (name == "copy") {
        return ToValue::makeSet(recv->elements());
    }
    if (name == "union" || name == "intersect" || name == "difference" ||
        name == "symmetric_difference") {
        expectArgs(args, 1, what, "exactly 1 argument", line);
        auto other = ToValue::makeSet(args[0]->elements());
        auto oc = coll(other);
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
    if (name == "is_subset" || name == "is_superset" || name == "is_disjoint") {
        expectArgs(args, 1, what, "exactly 1 argument", line);
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
        for (auto& v : c->items) if (oc->has(v)) return ToValue::makeBool(false);
        return ToValue::makeBool(true);
    }
    return nullptr;
}

// ------------------------------------------------------------
// deque / queue / stack / heap
// ------------------------------------------------------------

ToValuePtr orderedCollectionMethod(const ToValuePtr& recv, const std::string& name,
                                   std::vector<ToValuePtr>& args, Interpreter*, int line) {
    auto c = coll(recv);
    auto type = recv->type;
    const std::string what = recv->typeName() + "." + name + "()";

    if (name == "clear") {
        c->items.clear();
        return ToValue::makeNone();
    }
    if (name == "copy") {
        std::vector<ToValuePtr> items(c->items.begin(), c->items.end());
        switch (type) {
            case ToValue::Type::DEQUE: return ToValue::makeDeque(std::move(items));
            case ToValue::Type::QUEUE: return ToValue::makeQueue(std::move(items));
            case ToValue::Type::STACK: return ToValue::makeStack(std::move(items));
            default: {
                auto copy = std::make_shared<ToCollection>(*c);
                return ToValue::makeCollection(ToValue::Type::HEAP, copy);
            }
        }
    }

    if (type == ToValue::Type::HEAP) {
        if (name == "push" || name == "add") {
            if (args.empty()) argError(what, "at least 1 argument", line);
            for (auto& a : args) c->heapPush(a);
            return ToValue::makeNone();
        }
        if (name == "pop") {
            if (c->items.empty()) throw ToRuntimeError("pop from empty heap", line);
            return c->heapPop();
        }
        if (name == "peek" || name == "top") {
            return c->items.empty() ? ToValue::makeNone() : c->items.front();
        }
        if (name == "is_max") return ToValue::makeBool(c->maxHeap);
        return nullptr;
    }

    if (type == ToValue::Type::STACK) {
        if (name == "push" || name == "add") {
            if (args.empty()) argError(what, "at least 1 argument", line);
            for (auto& a : args) c->items.push_back(a);
            return ToValue::makeNone();
        }
        if (name == "pop") {
            if (c->items.empty()) throw ToRuntimeError("pop from empty stack", line);
            auto v = c->items.back();
            c->items.pop_back();
            return v;
        }
        if (name == "peek" || name == "top") {
            return c->items.empty() ? ToValue::makeNone() : c->items.back();
        }
        return nullptr;
    }

    if (type == ToValue::Type::QUEUE) {
        if (name == "push" || name == "add" || name == "enqueue") {
            if (args.empty()) argError(what, "at least 1 argument", line);
            for (auto& a : args) c->items.push_back(a);
            return ToValue::makeNone();
        }
        if (name == "pop" || name == "dequeue") {
            if (c->items.empty()) throw ToRuntimeError("pop from empty queue", line);
            auto v = c->items.front();
            c->items.pop_front();
            return v;
        }
        if (name == "peek" || name == "front") {
            return c->items.empty() ? ToValue::makeNone() : c->items.front();
        }
        return nullptr;
    }

    // deque
    if (name == "push" || name == "add" || name == "push_back") {
        if (args.empty()) argError(what, "at least 1 argument", line);
        for (auto& a : args) c->items.push_back(a);
        return ToValue::makeNone();
    }
    if (name == "push_front") {
        if (args.empty()) argError(what, "at least 1 argument", line);
        for (auto& a : args) c->items.push_front(a);
        return ToValue::makeNone();
    }
    if (name == "pop" || name == "pop_back") {
        if (c->items.empty()) throw ToRuntimeError("pop from empty deque", line);
        auto v = c->items.back();
        c->items.pop_back();
        return v;
    }
    if (name == "pop_front") {
        if (c->items.empty()) throw ToRuntimeError("pop from empty deque", line);
        auto v = c->items.front();
        c->items.pop_front();
        return v;
    }
    if (name == "peek" || name == "peek_back") {
        return c->items.empty() ? ToValue::makeNone() : c->items.back();
    }
    if (name == "peek_front") {
        return c->items.empty() ? ToValue::makeNone() : c->items.front();
    }
    if (name == "reverse") {
        std::reverse(c->items.begin(), c->items.end());
        return ToValue::makeNone();
    }
    if (name == "rotate") {
        int64_t n = args.empty() ? 1 : asIndex(args[0], what, line);
        if (!c->items.empty()) {
            int64_t sz = (int64_t)c->items.size();
            n = ((n % sz) + sz) % sz;
            std::rotate(c->items.begin(), c->items.end() - n, c->items.end());
        }
        return ToValue::makeNone();
    }
    return nullptr;
}

// ------------------------------------------------------------
// string
// ------------------------------------------------------------

ToValuePtr stringMethod(const ToValuePtr& recv, const std::string& name,
                        std::vector<ToValuePtr>& args, Interpreter* interp, int line) {
    const std::string& s = recv->strVal;
    const std::string what = "string." + name + "()";

    if (name == "upper") {
        std::string r = s;
        for (auto& c : r) c = (char)std::toupper((unsigned char)c);
        return ToValue::makeString(std::move(r));
    }
    if (name == "lower") {
        std::string r = s;
        for (auto& c : r) c = (char)std::tolower((unsigned char)c);
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
    if (name == "contains") {
        expectArgs(args, 1, what, "exactly 1 string argument", line);
        if (args[0]->type != ToValue::Type::STRING) argError(what, "exactly 1 string argument", line);
        return ToValue::makeBool(s.find(args[0]->strVal) != std::string::npos);
    }
    if (name == "replace") {
        expectArgs(args, 2, what, "2 string arguments", line);
        if (args[0]->type != ToValue::Type::STRING || args[1]->type != ToValue::Type::STRING)
            argError(what, "2 string arguments", line);
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
        expectArgs(args, 1, what, "1 string argument", line);
        if (args[0]->type != ToValue::Type::STRING) argError(what, "1 string argument", line);
        return ToValue::makeBool(s.rfind(args[0]->strVal, 0) == 0);
    }
    if (name == "ends_with") {
        expectArgs(args, 1, what, "1 string argument", line);
        if (args[0]->type != ToValue::Type::STRING) argError(what, "1 string argument", line);
        const std::string& suffix = args[0]->strVal;
        if (suffix.size() > s.size()) return ToValue::makeBool(false);
        return ToValue::makeBool(s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0);
    }
    if (name == "index_of") {
        expectArgs(args, 1, what, "1 string argument", line);
        if (args[0]->type != ToValue::Type::STRING) argError(what, "1 string argument", line);
        size_t pos = s.find(args[0]->strVal);
        return ToValue::makeInt(pos == std::string::npos ? -1 : (int64_t)pos);
    }
    if (name == "count") {
        expectArgs(args, 1, what, "1 string argument", line);
        if (args[0]->type != ToValue::Type::STRING) argError(what, "1 string argument", line);
        const std::string& needle = args[0]->strVal;
        if (needle.empty()) return ToValue::makeInt(0);
        int64_t n = 0;
        size_t pos = 0;
        while ((pos = s.find(needle, pos)) != std::string::npos) { n++; pos += needle.size(); }
        return ToValue::makeInt(n);
    }
    if (name == "repeat") {
        expectArgs(args, 1, what, "1 integer argument", line);
        int64_t n = asIndex(args[0], what, line);
        std::string r;
        for (int64_t i = 0; i < n; i++) r += s;
        return ToValue::makeString(std::move(r));
    }
    if (name == "pad_start" || name == "pad_end") {
        if (args.empty() || args.size() > 2) argError(what, "a width and an optional pad string", line);
        int64_t width = asIndex(args[0], what, line);
        std::string pad = args.size() > 1 && args[1]->type == ToValue::Type::STRING ? args[1]->strVal : " ";
        if (pad.empty()) pad = " ";
        std::string r = s;
        std::string filler;
        while ((int64_t)(r.size() + filler.size()) < width) filler += pad;
        if ((int64_t)(r.size() + filler.size()) > width)
            filler.resize(width > (int64_t)r.size() ? width - r.size() : 0);
        return ToValue::makeString(name == "pad_start" ? filler + r : r + filler);
    }
    if (name == "reverse" || name == "reversed") {
        std::string r(s.rbegin(), s.rend());
        return ToValue::makeString(std::move(r));
    }
    if (name == "capitalize") {
        std::string r = s;
        if (!r.empty()) r[0] = (char)std::toupper((unsigned char)r[0]);
        return ToValue::makeString(std::move(r));
    }
    if (name == "to_int") {
        try {
            return ToValue::makeInt(std::stoll(s));
        } catch (...) {
            throw ToRuntimeError("Cannot read \"" + s + "\" as an int", line);
        }
    }
    if (name == "to_float") {
        try {
            return ToValue::makeFloat(std::stod(s));
        } catch (...) {
            throw ToRuntimeError("Cannot read \"" + s + "\" as a float", line);
        }
    }
    if (name == "is_empty") return ToValue::makeBool(s.empty());
    if (name == "length" || name == "size") return ToValue::makeInt((int64_t)s.size());
    (void)interp;
    return nullptr;
}

} // namespace

// ============================================================
// Public entry points
// ============================================================

ToValuePtr getBuiltinProperty(const ToValuePtr& recv, const std::string& name) {
    if (!recv) return nullptr;
    if (name == "length" || name == "size") {
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
    switch (recv->type) {
        case ToValue::Type::LIST:   result = listMethod(recv, name, args, interp, line); break;
        case ToValue::Type::TUPLE:  result = tupleMethod(recv, name, args, interp, line); break;
        case ToValue::Type::DICT:   result = dictMethod(recv, name, args, interp, line); break;
        case ToValue::Type::SET:    result = setMethod(recv, name, args, interp, line); break;
        case ToValue::Type::DEQUE:
        case ToValue::Type::QUEUE:
        case ToValue::Type::STACK:
        case ToValue::Type::HEAP:   result = orderedCollectionMethod(recv, name, args, interp, line); break;
        case ToValue::Type::STRING: result = stringMethod(recv, name, args, interp, line); break;
        default: break;
    }
    if (result) return result;

    // Everything with a length shares the read-only sequence methods.
    if (recv->length() >= 0) {
        result = commonSequenceMethod(recv, name, args, interp, line);
        if (result) return result;
    }

    throw ToRuntimeError(recv->typeName() + " has no method '" + name + "'", line);
}

// ------------------------------------------------------------
// Indexing
// ------------------------------------------------------------

ToValuePtr indexGet(const ToValuePtr& obj, const ToValuePtr& index, int line) {
    switch (obj->type) {
        case ToValue::Type::LIST:
        case ToValue::Type::TUPLE: {
            int64_t i = normalizeIndex(asIndex(index, obj->typeName(), line), (int64_t)obj->listVal.size());
            if (i < 0) throw ToRuntimeError("Index out of bounds: " + index->toString(), line);
            return obj->listVal[i];
        }
        case ToValue::Type::STRING: {
            int64_t i = normalizeIndex(asIndex(index, "string", line), (int64_t)obj->strVal.size());
            if (i < 0) throw ToRuntimeError("Index out of bounds: " + index->toString(), line);
            return ToValue::makeString(std::string(1, obj->strVal[i]));
        }
        case ToValue::Type::DICT: {
            if (!valueHashable(index))
                throw ToRuntimeError("Cannot use " + index->typeName() + " as a dict key", line);
            auto found = obj->dictVal.getKey(index);
            if (!found) throw ToRuntimeError("Key not found: " + index->repr(), line);
            return found;
        }
        case ToValue::Type::DEQUE:
        case ToValue::Type::QUEUE:
        case ToValue::Type::STACK: {
            auto& items = obj->collVal->items;
            int64_t i = normalizeIndex(asIndex(index, obj->typeName(), line), (int64_t)items.size());
            if (i < 0) throw ToRuntimeError("Index out of bounds: " + index->toString(), line);
            return items[i];
        }
        case ToValue::Type::SET:
            throw ToRuntimeError("A set has no positions to index — use has() or to_list()", line);
        case ToValue::Type::HEAP:
            throw ToRuntimeError("A heap has no positions to index — use peek() or to_list()", line);
        default:
            throw ToRuntimeError("Cannot index " + obj->typeName(), line);
    }
}

void indexSet(const ToValuePtr& obj, const ToValuePtr& index, ToValuePtr value, int line) {
    switch (obj->type) {
        case ToValue::Type::LIST: {
            int64_t i = normalizeIndex(asIndex(index, "list", line), (int64_t)obj->listVal.size());
            if (i < 0) throw ToRuntimeError("Index out of bounds", line);
            obj->listVal[i] = std::move(value);
            return;
        }
        case ToValue::Type::DICT:
            if (!valueHashable(index))
                throw ToRuntimeError("Cannot use " + index->typeName() + " as a dict key", line);
            obj->dictVal.setKey(index, std::move(value));
            return;
        case ToValue::Type::DEQUE:
        case ToValue::Type::QUEUE:
        case ToValue::Type::STACK: {
            auto& items = obj->collVal->items;
            int64_t i = normalizeIndex(asIndex(index, obj->typeName(), line), (int64_t)items.size());
            if (i < 0) throw ToRuntimeError("Index out of bounds", line);
            items[i] = std::move(value);
            return;
        }
        case ToValue::Type::TUPLE:
            throw ToRuntimeError("A tuple cannot be changed — use to_list() first", line);
        case ToValue::Type::STRING:
            throw ToRuntimeError("A string cannot be changed in place", line);
        default:
            throw ToRuntimeError("Cannot index " + obj->typeName(), line);
    }
}

ToValuePtr sliceValue(const ToValuePtr& obj, const ToValuePtr& start,
                      const ToValuePtr& end, const ToValuePtr& step, int line) {
    int64_t len = obj->length();
    if (len < 0 || obj->type == ToValue::Type::DICT)
        throw ToRuntimeError("Cannot slice " + lengthlessName(obj), line);

    int64_t stride = 1;
    if (step && step->type != ToValue::Type::NONE) {
        stride = asIndex(step, "slice step", line);
        if (stride == 0) throw ToRuntimeError("Slice step cannot be zero", line);
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
        throw ToRuntimeError("Cannot negate " + operand->typeName(), line);
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
                    if (b == 0) throw ToRuntimeError("Division by zero", line);
                    if (a % b == 0) return ToValue::makeInt(a / b);
                    return ToValue::makeFloat((double)a / (double)b);
                case BinOp::Mod:
                    if (b == 0) throw ToRuntimeError("Modulo by zero", line);
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
                if (rv == 0) throw ToRuntimeError("Division by zero", line);
                return ToValue::makeFloat(lv / rv);
            case BinOp::Mod:
                if (rv == 0) throw ToRuntimeError("Modulo by zero", line);
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

    // Operator overloading: a class that defines plus/minus/equals/... gets
    // to answer first.
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

    // String concatenation
    if (op == BinOp::Add &&
        (left->type == ToValue::Type::STRING || right->type == ToValue::Type::STRING))
        return ToValue::makeString(left->toString() + right->toString());

    // Equality is structural for every type: two lists with equal elements
    // are equal, and so are two sets with the same members.
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

    throw ToRuntimeError(std::string("Unsupported operation '") + binOpName(op) + "' between " +
        left->typeName() + " and " + right->typeName(), line);
}

// ------------------------------------------------------------
// Constructors, exposed as globals
// ------------------------------------------------------------

void registerCollectionBuiltins(EnvPtr env) {
    auto seed = [](const std::vector<ToValuePtr>& args, const char* what) {
        if (args.empty()) return std::vector<ToValuePtr>{};
        if (args.size() == 1 && args[0]->length() >= 0) return args[0]->elements();
        (void)what;
        return args;
    };

    env->define("set", ToValue::makeBuiltin([seed](std::vector<ToValuePtr> args) {
        return ToValue::makeSet(seed(args, "set()"));
    }));
    env->define("tuple", ToValue::makeBuiltin([seed](std::vector<ToValuePtr> args) {
        return ToValue::makeTuple(seed(args, "tuple()"));
    }));
    env->define("deque", ToValue::makeBuiltin([seed](std::vector<ToValuePtr> args) {
        return ToValue::makeDeque(seed(args, "deque()"));
    }));
    env->define("queue", ToValue::makeBuiltin([seed](std::vector<ToValuePtr> args) {
        return ToValue::makeQueue(seed(args, "queue()"));
    }));
    env->define("stack", ToValue::makeBuiltin([seed](std::vector<ToValuePtr> args) {
        return ToValue::makeStack(seed(args, "stack()"));
    }));
    env->define("heap", ToValue::makeBuiltin([seed](std::vector<ToValuePtr> args) {
        return ToValue::makeHeap(seed(args, "heap()"), false);
    }));
    env->define("max_heap", ToValue::makeBuiltin([seed](std::vector<ToValuePtr> args) {
        return ToValue::makeHeap(seed(args, "max_heap()"), true);
    }));
    env->define("list", ToValue::makeBuiltin([seed](std::vector<ToValuePtr> args) {
        return ToValue::makeList(seed(args, "list()"));
    }));
    env->define("dict", ToValue::makeBuiltin([](std::vector<ToValuePtr> args) {
        ToDict d;
        if (args.size() == 1 && args[0]->type == ToValue::Type::DICT) d = args[0]->dictVal;
        else if (args.size() == 1 && args[0]->length() >= 0) {
            // dict([(k, v), ...])
            for (auto& pair : args[0]->elements()) {
                auto kv = pair->elements();
                if (kv.size() != 2)
                    throw ToRuntimeError("dict() expects a list of two-element pairs");
                d.setKey(kv[0], kv[1]);
            }
        } else if (!args.empty()) {
            throw ToRuntimeError("dict() takes a dict or a list of pairs");
        }
        return ToValue::makeDict(std::move(d));
    }));
}
