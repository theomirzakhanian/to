// ============================================================
// value.cpp — the canonical value operations
// ------------------------------------------------------------
// Equality, ordering, hashing, printing, and the storage behind
// dicts, sets, deques, queues, stacks and heaps. Everything that
// needs to compare two `to` values goes through this file, so
// `a == b`, `xs.sort()`, `d[k]` and `s.has(x)` can never disagree.
// ============================================================
#include "environment.h"
#include "error.h"
#include <algorithm>
#include <cmath>
#include <cstring>

// ------------------------------------------------------------
// Type ranking — gives values of different types a stable order
// so sorting a mixed list never throws.
// ------------------------------------------------------------
static int typeRank(ToValue::Type t) {
    switch (t) {
        case ToValue::Type::NONE:      return 0;
        case ToValue::Type::BOOL:      return 1;
        case ToValue::Type::INT:       return 2;
        case ToValue::Type::FLOAT:     return 2; // numbers share a rank and compare numerically
        case ToValue::Type::STRING:    return 3;
        case ToValue::Type::LIST:      return 4;
        case ToValue::Type::TUPLE:     return 5;
        case ToValue::Type::SET:       return 6;
        case ToValue::Type::DICT:      return 7;
        case ToValue::Type::DEQUE:     return 8;
        case ToValue::Type::QUEUE:     return 9;
        case ToValue::Type::STACK:     return 10;
        case ToValue::Type::HEAP:      return 11;
        case ToValue::Type::FUNCTION:  return 12;
        case ToValue::Type::BUILTIN:   return 13;
        case ToValue::Type::CLASS:     return 14;
        case ToValue::Type::INSTANCE:  return 15;
        case ToValue::Type::FUTURE:    return 16;
        case ToValue::Type::GENERATOR: return 17;
    }
    return 18;
}

static bool isNumber(const ToValuePtr& v) {
    return v->type == ToValue::Type::INT || v->type == ToValue::Type::FLOAT;
}

static double asDouble(const ToValuePtr& v) {
    return v->type == ToValue::Type::INT ? (double)v->intVal : v->floatVal;
}

std::string valueTypeName(const ToValue& v) {
    switch (v.type) {
        case ToValue::Type::INT:       return "int";
        case ToValue::Type::FLOAT:     return "float";
        case ToValue::Type::STRING:    return "string";
        case ToValue::Type::BOOL:      return "bool";
        case ToValue::Type::NONE:      return "none";
        case ToValue::Type::LIST:      return "list";
        case ToValue::Type::TUPLE:     return "tuple";
        case ToValue::Type::DICT:      return "dict";
        case ToValue::Type::SET:       return "set";
        case ToValue::Type::DEQUE:     return "deque";
        case ToValue::Type::QUEUE:     return "queue";
        case ToValue::Type::STACK:     return "stack";
        case ToValue::Type::HEAP:      return "heap";
        case ToValue::Type::FUNCTION:  return "function";
        case ToValue::Type::BUILTIN:   return "builtin";
        case ToValue::Type::CLASS:     return "class";
        case ToValue::Type::INSTANCE:  return "instance";
        case ToValue::Type::FUTURE:    return "future";
        case ToValue::Type::GENERATOR: return "generator";
    }
    return "unknown";
}

// ============================================================
// Equality
// ============================================================

bool valueEquals(const ToValuePtr& a, const ToValuePtr& b) {
    if (!a || !b) return a == b;
    if (a.get() == b.get()) return true;

    if (isNumber(a) && isNumber(b)) {
        if (a->type == ToValue::Type::INT && b->type == ToValue::Type::INT)
            return a->intVal == b->intVal;
        return asDouble(a) == asDouble(b);
    }
    if (a->type != b->type) return false;

    switch (a->type) {
        case ToValue::Type::STRING: return a->strVal == b->strVal;
        case ToValue::Type::BOOL:   return a->boolVal == b->boolVal;
        case ToValue::Type::NONE:   return true;

        case ToValue::Type::LIST:
        case ToValue::Type::TUPLE: {
            if (a->listVal.size() != b->listVal.size()) return false;
            for (size_t i = 0; i < a->listVal.size(); i++)
                if (!valueEquals(a->listVal[i], b->listVal[i])) return false;
            return true;
        }
        case ToValue::Type::DICT: {
            if (a->dictVal.size() != b->dictVal.size()) return false;
            for (size_t i = 0; i < a->dictVal.size(); i++) {
                auto key = a->dictVal.keyAt(i);
                auto other = b->dictVal.getKey(key);
                if (!other || !valueEquals(a->dictVal[i].second, other)) return false;
            }
            return true;
        }
        case ToValue::Type::SET: {
            if (!a->collVal || !b->collVal) return a->collVal == b->collVal;
            if (a->collVal->items.size() != b->collVal->items.size()) return false;
            for (auto& item : a->collVal->items)
                if (!b->collVal->has(item)) return false;
            return true;
        }
        case ToValue::Type::DEQUE:
        case ToValue::Type::QUEUE:
        case ToValue::Type::STACK:
        case ToValue::Type::HEAP: {
            if (!a->collVal || !b->collVal) return a->collVal == b->collVal;
            if (a->collVal->items.size() != b->collVal->items.size()) return false;
            for (size_t i = 0; i < a->collVal->items.size(); i++)
                if (!valueEquals(a->collVal->items[i], b->collVal->items[i])) return false;
            return true;
        }
        default:
            return false; // functions, classes, instances, futures: identity only
    }
}

// ============================================================
// Ordering
// ============================================================

int valueCompare(const ToValuePtr& a, const ToValuePtr& b) {
    if (!a || !b) return a == b ? 0 : (a ? 1 : -1);

    int ra = typeRank(a->type), rb = typeRank(b->type);
    if (ra != rb) return ra < rb ? -1 : 1;

    switch (a->type) {
        case ToValue::Type::NONE: return 0;
        case ToValue::Type::BOOL:
            return a->boolVal == b->boolVal ? 0 : (a->boolVal ? 1 : -1);
        case ToValue::Type::INT:
        case ToValue::Type::FLOAT: {
            if (a->type == ToValue::Type::INT && b->type == ToValue::Type::INT)
                return a->intVal == b->intVal ? 0 : (a->intVal < b->intVal ? -1 : 1);
            double x = asDouble(a), y = asDouble(b);
            if (x < y) return -1;
            if (x > y) return 1;
            return 0;
        }
        case ToValue::Type::STRING: {
            int c = a->strVal.compare(b->strVal);
            return c == 0 ? 0 : (c < 0 ? -1 : 1);
        }
        default: break;
    }

    // Ordered containers compare lexicographically.
    auto ea = a->elements();
    auto eb = b->elements();
    if (a->type == ToValue::Type::SET || a->type == ToValue::Type::DICT) {
        // Unordered by nature — compare by size, then by a canonical sorted view,
        // so the order is total and stable rather than insertion-dependent.
        if (ea.size() != eb.size()) return ea.size() < eb.size() ? -1 : 1;
        auto cmp = [](const ToValuePtr& x, const ToValuePtr& y) { return valueCompare(x, y) < 0; };
        std::sort(ea.begin(), ea.end(), cmp);
        std::sort(eb.begin(), eb.end(), cmp);
    }
    size_t n = std::min(ea.size(), eb.size());
    for (size_t i = 0; i < n; i++) {
        int c = valueCompare(ea[i], eb[i]);
        if (c != 0) return c;
    }
    if (ea.size() != eb.size()) return ea.size() < eb.size() ? -1 : 1;
    if (ea.empty() && eb.empty() && a.get() != b.get()) {
        // Distinct empty containers of the same kind: fall back to identity so the
        // order stays strict.
        return a.get() < b.get() ? -1 : 1;
    }
    return 0;
}

// ============================================================
// Hashing
// ============================================================

bool valueHashable(const ToValuePtr& v) {
    if (!v) return false;
    switch (v->type) {
        case ToValue::Type::INT:
        case ToValue::Type::FLOAT:
        case ToValue::Type::STRING:
        case ToValue::Type::BOOL:
        case ToValue::Type::NONE:
            return true;
        case ToValue::Type::TUPLE: {
            for (auto& e : v->listVal)
                if (!valueHashable(e)) return false;
            return true;
        }
        default:
            return false;
    }
}

static inline size_t hashMix(size_t seed, size_t h) {
    return seed ^ (h + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

size_t valueHash(const ToValuePtr& v) {
    if (!v) return 0;
    switch (v->type) {
        case ToValue::Type::NONE:   return 0x9e3779b9;
        case ToValue::Type::BOOL:   return v->boolVal ? 0x85ebca6b : 0xc2b2ae35;
        case ToValue::Type::INT:    return std::hash<int64_t>{}(v->intVal);
        case ToValue::Type::FLOAT: {
            // 2.0 and 2 are equal, so they must hash alike.
            double d = v->floatVal;
            if (std::isfinite(d) && d == std::floor(d) &&
                d >= (double)INT64_MIN && d <= (double)INT64_MAX)
                return std::hash<int64_t>{}((int64_t)d);
            return std::hash<double>{}(d);
        }
        case ToValue::Type::STRING: return std::hash<std::string>{}(v->strVal);
        case ToValue::Type::TUPLE: {
            size_t h = 0xcbf29ce484222325ULL;
            for (auto& e : v->listVal) h = hashMix(h, valueHash(e));
            return h;
        }
        default:
            throw ToRuntimeError("Cannot use " + valueTypeName(*v) +
                                 " as a dict key or set element — it is not hashable");
    }
}

// ============================================================
// Interned scalars
// ------------------------------------------------------------
// none, true, false and small integers are handed out as shared
// immutable values. Nothing in the runtime writes to a scalar in
// place, so sharing them is invisible to programs and removes a
// large fraction of all allocations from arithmetic-heavy loops.
// ============================================================

namespace {

constexpr int64_t SMALL_INT_MIN = -1024;
constexpr int64_t SMALL_INT_MAX = 1024;

ToValuePtr makeRawInt(int64_t v) {
    auto val = std::make_shared<ToValue>();
    val->type = ToValue::Type::INT;
    val->intVal = v;
    return val;
}

struct InternPool {
    ToValuePtr none;
    ToValuePtr yes;
    ToValuePtr no;
    std::vector<ToValuePtr> ints;

    InternPool() {
        none = std::make_shared<ToValue>();
        none->type = ToValue::Type::NONE;
        yes = std::make_shared<ToValue>();
        yes->type = ToValue::Type::BOOL;
        yes->boolVal = true;
        no = std::make_shared<ToValue>();
        no->type = ToValue::Type::BOOL;
        no->boolVal = false;
        ints.reserve(SMALL_INT_MAX - SMALL_INT_MIN + 1);
        for (int64_t i = SMALL_INT_MIN; i <= SMALL_INT_MAX; i++) ints.push_back(makeRawInt(i));
    }
};

const InternPool& pool() {
    static const InternPool p;
    return p;
}

} // namespace

ToValuePtr ToValue::makeNone() { return pool().none; }

ToValuePtr ToValue::makeBool(bool v) { return v ? pool().yes : pool().no; }

ToValuePtr ToValue::makeInt(int64_t v) {
    if (v >= SMALL_INT_MIN && v <= SMALL_INT_MAX) return pool().ints[(size_t)(v - SMALL_INT_MIN)];
    return makeRawInt(v);
}

// ============================================================
// Collection factories
// ============================================================

ToValuePtr ToValue::makeSet(std::vector<ToValuePtr> items) {
    auto coll = std::make_shared<ToCollection>();
    for (auto& item : items) coll->insert(item);
    return makeCollection(Type::SET, std::move(coll));
}

static ToCollectionPtr collFrom(std::vector<ToValuePtr>& items) {
    auto coll = std::make_shared<ToCollection>();
    for (auto& item : items) coll->items.push_back(std::move(item));
    return coll;
}

ToValuePtr ToValue::makeDeque(std::vector<ToValuePtr> items) {
    return makeCollection(Type::DEQUE, collFrom(items));
}
ToValuePtr ToValue::makeQueue(std::vector<ToValuePtr> items) {
    return makeCollection(Type::QUEUE, collFrom(items));
}
ToValuePtr ToValue::makeStack(std::vector<ToValuePtr> items) {
    return makeCollection(Type::STACK, collFrom(items));
}
ToValuePtr ToValue::makeHeap(std::vector<ToValuePtr> items, bool maxHeap) {
    auto coll = collFrom(items);
    coll->maxHeap = maxHeap;
    coll->heapify();
    return makeCollection(Type::HEAP, std::move(coll));
}

// ============================================================
// ToDict
// ============================================================

ToDict::ToDict(std::initializer_list<std::pair<std::string, ToValuePtr>> init) {
    entries.reserve(init.size());
    for (auto& kv : init) set(kv.first, kv.second);
}

ToDict::ToDict(const std::vector<std::pair<std::string, ToValuePtr>>& init) {
    entries.reserve(init.size());
    for (auto& kv : init) set(kv.first, kv.second);
}

void ToDict::clear() {
    entries.clear();
    strIndex.clear();
    valIndex.clear();
}

size_t ToDict::indexOf(const std::string& key) const {
    auto it = strIndex.find(key);
    return it == strIndex.end() ? npos : it->second;
}

size_t ToDict::indexOfKey(const ToValuePtr& key) const {
    if (!key) return npos;
    if (key->type == ToValue::Type::STRING) return indexOf(key->strVal);
    auto it = valIndex.find(valueHash(key));
    if (it == valIndex.end()) return npos;
    for (size_t pos : it->second) {
        if (pos < entries.size() && entries[pos].keyVal && valueEquals(entries[pos].keyVal, key))
            return pos;
    }
    return npos;
}

void ToDict::set(const std::string& key, ToValuePtr value) {
    auto it = strIndex.find(key);
    if (it != strIndex.end()) {
        entries[it->second].second = std::move(value);
        return;
    }
    strIndex.emplace(key, entries.size());
    entries.push_back(ToDictEntry{key, std::move(value), nullptr});
}

void ToDict::insertEntry(const Entry& e) {
    if (e.keyVal && e.keyVal->type != ToValue::Type::STRING) setKey(e.keyVal, e.second);
    else set(e.first, e.second);
}

void ToDict::setKey(const ToValuePtr& key, ToValuePtr value) {
    if (!key) throw ToRuntimeError("Dict key cannot be null");
    if (key->type == ToValue::Type::STRING) { set(key->strVal, std::move(value)); return; }

    size_t pos = indexOfKey(key);
    if (pos != npos) { entries[pos].second = std::move(value); return; }

    size_t h = valueHash(key); // throws for unhashable keys before anything is mutated
    valIndex[h].push_back(entries.size());
    entries.push_back(ToDictEntry{key->toString(), std::move(value), key});
}

ToValuePtr ToDict::get(const std::string& key) const {
    size_t pos = indexOf(key);
    return pos == npos ? nullptr : entries[pos].second;
}

ToValuePtr ToDict::getKey(const ToValuePtr& key) const {
    size_t pos = indexOfKey(key);
    return pos == npos ? nullptr : entries[pos].second;
}

ToValuePtr ToDict::keyAt(size_t i) const {
    if (i >= entries.size()) return ToValue::makeNone();
    if (entries[i].keyVal) return entries[i].keyVal;
    return ToValue::makeString(entries[i].first);
}

void ToDict::reindex() {
    strIndex.clear();
    valIndex.clear();
    for (size_t i = 0; i < entries.size(); i++) {
        if (entries[i].keyVal && entries[i].keyVal->type != ToValue::Type::STRING)
            valIndex[valueHash(entries[i].keyVal)].push_back(i);
        else
            strIndex.emplace(entries[i].first, i);
    }
}

bool ToDict::erase(const std::string& key) {
    size_t pos = indexOf(key);
    if (pos == npos) return false;
    entries.erase(entries.begin() + pos);
    reindex();
    return true;
}

bool ToDict::eraseKey(const ToValuePtr& key) {
    if (key && key->type == ToValue::Type::STRING) return erase(key->strVal);
    size_t pos = indexOfKey(key);
    if (pos == npos) return false;
    entries.erase(entries.begin() + pos);
    reindex();
    return true;
}

// ============================================================
// ToCollection — set operations
// ============================================================

void ToCollection::reindex() {
    index.clear();
    for (size_t i = 0; i < items.size(); i++) index[valueHash(items[i])].push_back(i);
}

bool ToCollection::has(const ToValuePtr& v) const {
    if (!valueHashable(v)) return false;
    auto it = index.find(valueHash(v));
    if (it == index.end()) return false;
    for (size_t pos : it->second)
        if (pos < items.size() && valueEquals(items[pos], v)) return true;
    return false;
}

bool ToCollection::insert(const ToValuePtr& v) {
    size_t h = valueHash(v); // rejects unhashable elements
    auto& bucket = index[h];
    for (size_t pos : bucket)
        if (pos < items.size() && valueEquals(items[pos], v)) return false;
    bucket.push_back(items.size());
    items.push_back(v);
    return true;
}

bool ToCollection::discard(const ToValuePtr& v) {
    if (!valueHashable(v)) return false;
    auto it = index.find(valueHash(v));
    if (it == index.end()) return false;
    for (size_t pos : it->second) {
        if (pos < items.size() && valueEquals(items[pos], v)) {
            items.erase(items.begin() + pos);
            reindex();
            return true;
        }
    }
    return false;
}

// ============================================================
// ToCollection — binary heap
// ============================================================

bool ToCollection::heapPrecedes(const ToValuePtr& a, const ToValuePtr& b) const {
    int c = valueCompare(a, b);
    return maxHeap ? c > 0 : c < 0;
}

void ToCollection::heapPush(ToValuePtr v) {
    items.push_back(std::move(v));
    size_t i = items.size() - 1;
    while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (!heapPrecedes(items[i], items[parent])) break;
        std::swap(items[i], items[parent]);
        i = parent;
    }
}

ToValuePtr ToCollection::heapPop() {
    if (items.empty()) return nullptr;
    ToValuePtr top = items.front();
    items.front() = items.back();
    items.pop_back();
    size_t n = items.size(), i = 0;
    while (true) {
        size_t l = 2 * i + 1, r = 2 * i + 2, best = i;
        if (l < n && heapPrecedes(items[l], items[best])) best = l;
        if (r < n && heapPrecedes(items[r], items[best])) best = r;
        if (best == i) break;
        std::swap(items[i], items[best]);
        i = best;
    }
    return top;
}

void ToCollection::heapify() {
    if (items.size() < 2) return;
    std::deque<ToValuePtr> source;
    source.swap(items);
    for (auto& v : source) heapPush(std::move(v));
}

// ============================================================
// ToValue — printing, truthiness, iteration
// ============================================================

std::vector<ToValuePtr> ToValue::elements() const {
    switch (type) {
        case Type::LIST:
        case Type::TUPLE:
            return listVal;
        case Type::STRING: {
            std::vector<ToValuePtr> out;
            out.reserve(strVal.size());
            for (char c : strVal) out.push_back(ToValue::makeString(std::string(1, c)));
            return out;
        }
        case Type::DICT: {
            std::vector<ToValuePtr> out;
            out.reserve(dictVal.size());
            for (size_t i = 0; i < dictVal.size(); i++) out.push_back(dictVal.keyAt(i));
            return out;
        }
        case Type::HEAP: {
            // Iterating a heap yields its elements in pop order, which is the
            // only order a heap actually promises.
            if (!collVal) return {};
            ToCollection copy = *collVal;
            std::vector<ToValuePtr> out;
            out.reserve(copy.items.size());
            while (!copy.items.empty()) out.push_back(copy.heapPop());
            return out;
        }
        case Type::SET:
        case Type::DEQUE:
        case Type::QUEUE:
        case Type::STACK: {
            if (!collVal) return {};
            return std::vector<ToValuePtr>(collVal->items.begin(), collVal->items.end());
        }
        default:
            return {};
    }
}

int64_t ToValue::length() const {
    switch (type) {
        case Type::STRING: return (int64_t)strVal.size();
        case Type::LIST:
        case Type::TUPLE:  return (int64_t)listVal.size();
        case Type::DICT:   return (int64_t)dictVal.size();
        case Type::SET:
        case Type::DEQUE:
        case Type::QUEUE:
        case Type::STACK:
        case Type::HEAP:   return collVal ? (int64_t)collVal->items.size() : 0;
        default: return -1;
    }
}

bool ToValue::isTruthy() const {
    switch (type) {
        case Type::BOOL:   return boolVal;
        case Type::NONE:   return false;
        case Type::INT:    return intVal != 0;
        case Type::FLOAT:  return floatVal != 0.0;
        case Type::STRING: return !strVal.empty();
        case Type::LIST:
        case Type::TUPLE:  return !listVal.empty();
        case Type::DICT:   return !dictVal.empty();
        case Type::SET:
        case Type::DEQUE:
        case Type::QUEUE:
        case Type::STACK:
        case Type::HEAP:   return collVal && !collVal->items.empty();
        default: return true;
    }
}

static std::string joinReprs(const std::vector<ToValuePtr>& items) {
    std::string out;
    for (size_t i = 0; i < items.size(); i++) {
        if (i > 0) out += ", ";
        out += items[i] ? items[i]->repr() : "none";
    }
    return out;
}

std::string ToValue::repr() const {
    if (type == Type::STRING) return "\"" + strVal + "\"";
    return toString();
}

std::string ToValue::toString() const {
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
        case Type::LIST: return "[" + joinReprs(listVal) + "]";
        case Type::TUPLE: {
            if (listVal.size() == 1) return "(" + listVal[0]->repr() + ",)";
            return "(" + joinReprs(listVal) + ")";
        }
        case Type::DICT: {
            std::string result = "{";
            for (size_t i = 0; i < dictVal.size(); i++) {
                if (i > 0) result += ", ";
                auto key = dictVal.keyAt(i);
                // String keys print bare, the way they are written in a literal.
                result += dictVal[i].keyVal ? key->repr() : dictVal[i].first;
                result += " = ";
                result += dictVal[i].second ? dictVal[i].second->repr() : "none";
            }
            return result + "}";
        }
        case Type::SET: {
            auto items = elements();
            if (items.empty()) return "set()";
            return "{" + joinReprs(items) + "}";
        }
        case Type::DEQUE: return "deque[" + joinReprs(elements()) + "]";
        case Type::QUEUE: return "queue[" + joinReprs(elements()) + "]";
        case Type::STACK: return "stack[" + joinReprs(elements()) + "]";
        case Type::HEAP:  return std::string(collVal && collVal->maxHeap ? "max_heap[" : "heap[") +
                                 joinReprs(elements()) + "]";
        case Type::FUNCTION: return "<function " + funcVal->name + ">";
        case Type::BUILTIN: return "<builtin function>";
        case Type::CLASS: return "<class " + classVal->name + ">";
        case Type::INSTANCE: return "<instance of " + instanceVal->klass->name + ">";
        case Type::FUTURE: return "<future>";
        case Type::GENERATOR: return "<generator>";
    }
    return "<unknown>";
}
