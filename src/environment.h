#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include <deque>
#include <functional>
#include <variant>
#include <sstream>
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "ast.h"

// Forward declarations
class Environment;
using EnvPtr = std::shared_ptr<Environment>;

// Value types for the interpreter
struct ToValue;
using ToValuePtr = std::shared_ptr<ToValue>;
using BuiltinFn = std::function<ToValuePtr(std::vector<ToValuePtr>)>;

// ============================================================
// Canonical value operations — defined once, in value.cpp
// ------------------------------------------------------------
// Every structural comparison in the language funnels through
// these: ==, sorting, dict keys, set membership, heap ordering.
// ============================================================

// Structural equality. Numbers compare across int/float, containers
// compare element-wise, everything else compares by identity.
bool valueEquals(const ToValuePtr& a, const ToValuePtr& b);

// Total order over every value: -1, 0, or 1. Never throws — values of
// different types order by type rank so sort() always terminates.
int valueCompare(const ToValuePtr& a, const ToValuePtr& b);

// True when the value can be a dict key or a set element.
bool valueHashable(const ToValuePtr& v);

// Hash consistent with valueEquals. Throws for unhashable values.
size_t valueHash(const ToValuePtr& v);

std::string valueTypeName(const ToValue& v);

// "a list", "an int" — so error messages read as English.
std::string withArticle(const std::string& typeName);

// ============================================================
// Dictionary storage
// ------------------------------------------------------------
// Insertion-ordered, hash-indexed. Lookup is O(1) instead of the
// linear scan the language used to do on every dict access.
//
// Entries expose `.first` / `.second` so the hundred-odd existing
// call sites that treat a dict as a vector of string pairs keep
// working; `keyVal` carries the real key object when a key is not
// a plain string (ints, floats, bools, none, tuples).
// ============================================================

struct ToDictEntry {
    std::string first;   // string key, or the display text of a non-string key
    ToValuePtr second;   // the value
    ToValuePtr keyVal;   // the real key when it is not a plain string; null otherwise
};

class ToDict {
public:
    using Entry = ToDictEntry;
    using value_type = ToDictEntry;
    using iterator = std::vector<Entry>::iterator;
    using const_iterator = std::vector<Entry>::const_iterator;

    ToDict() = default;
    ToDict(std::initializer_list<std::pair<std::string, ToValuePtr>> init);
    ToDict(const std::vector<std::pair<std::string, ToValuePtr>>& init);

    iterator begin() { return entries.begin(); }
    iterator end() { return entries.end(); }
    const_iterator begin() const { return entries.begin(); }
    const_iterator end() const { return entries.end(); }

    size_t size() const { return entries.size(); }
    bool empty() const { return entries.empty(); }
    void clear();
    void reserve(size_t n) { entries.reserve(n); }

    Entry& operator[](size_t i) { return entries[i]; }
    const Entry& operator[](size_t i) const { return entries[i]; }
    Entry& back() { return entries.back(); }

    // Insert or overwrite a string-keyed entry.
    void push_back(const std::pair<std::string, ToValuePtr>& kv) { set(kv.first, kv.second); }
    void insertEntry(const Entry& e);

    void set(const std::string& key, ToValuePtr value);
    void setKey(const ToValuePtr& key, ToValuePtr value);

    // Null when absent.
    ToValuePtr get(const std::string& key) const;
    ToValuePtr getKey(const ToValuePtr& key) const;

    bool contains(const std::string& key) const { return indexOf(key) != npos; }
    bool containsKey(const ToValuePtr& key) const { return indexOfKey(key) != npos; }

    bool erase(const std::string& key);
    bool eraseKey(const ToValuePtr& key);

    // The key at position i as a value object.
    ToValuePtr keyAt(size_t i) const;

    static constexpr size_t npos = static_cast<size_t>(-1);
    size_t indexOf(const std::string& key) const;
    size_t indexOfKey(const ToValuePtr& key) const;

private:
    std::vector<Entry> entries;
    std::unordered_map<std::string, size_t> strIndex;          // string keys -> position
    std::unordered_map<size_t, std::vector<size_t>> valIndex;  // hash of non-string key -> positions
    void reindex();
};

// ============================================================
// Backing store for set / deque / queue / stack / heap
// ------------------------------------------------------------
// One heap-allocated payload shared by the five ordered
// collections, so adding them costs ToValue a single pointer.
// ============================================================

struct ToCollection {
    std::deque<ToValuePtr> items;                          // elements, in collection order
    std::unordered_map<size_t, std::vector<size_t>> index; // set only: hash -> positions
    bool maxHeap = false;                                  // heap only: pop order

    // -- set helpers ------------------------------------------------
    bool has(const ToValuePtr& v) const;
    bool insert(const ToValuePtr& v);  // true when newly added
    bool discard(const ToValuePtr& v); // true when something was removed
    void reindex();

    // -- heap helpers -----------------------------------------------
    bool heapPrecedes(const ToValuePtr& a, const ToValuePtr& b) const;
    void heapPush(ToValuePtr v);
    ToValuePtr heapPop();
    void heapify();
};
using ToCollectionPtr = std::shared_ptr<ToCollection>;

struct ToFunction {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> paramTypes; // optional type hints
    std::string returnTypeHint;
    std::vector<ASTNodePtr> body;
    EnvPtr closure;
    bool isGenerator = false; // set if body contains 'yield'

    // Bytecode compiled from this body, filled in lazily by the VM. Held here
    // rather than in a side table keyed by address, because functions are
    // freed and a later function can land on the same address.
    std::shared_ptr<void> compiledChunk;
};

struct ToClass {
    std::string name;
    std::shared_ptr<ToClass> parent; // inheritance
    std::vector<MethodDef> methods;
    EnvPtr closure;
};

struct ToInstance {
    std::shared_ptr<ToClass> klass;
    std::unordered_map<std::string, ToValuePtr> fields;
};

// Generator — backed by a coroutine-style thread that pauses on yield.
//
// The handshake state lives in its own object because the producer thread
// must be able to reach it without owning the generator: if the thread held
// a reference to the ToGenerator, the generator could never be destroyed
// from the outside, and the last release would land on the producer thread
// itself — which then tries to join itself and aborts the process.
struct ToGeneratorState {
    std::mutex mtx;
    std::condition_variable cv;
    ToValuePtr currentValue;              // the yielded value
    std::atomic<bool> hasValue{false};    // producer has yielded a value
    std::atomic<bool> consumerReady{false}; // consumer asked for the next value
    std::atomic<bool> done{false};        // producer finished, or was abandoned
    std::string error;                    // error message if the body threw
};
using ToGeneratorStatePtr = std::shared_ptr<ToGeneratorState>;

struct ToGenerator {
    ToGeneratorStatePtr state = std::make_shared<ToGeneratorState>();
    std::shared_ptr<std::thread> thread;
    ~ToGenerator();
};

struct ToValue {
    enum class Type {
        INT, FLOAT, STRING, BOOL, NONE,
        LIST, TUPLE, DICT, SET, DEQUE, QUEUE, STACK, HEAP,
        FUNCTION, BUILTIN, CLASS, INSTANCE, FUTURE, GENERATOR
    };
    Type type;

    int64_t intVal = 0;
    double floatVal = 0.0;
    std::string strVal;
    bool boolVal = false;
    std::vector<ToValuePtr> listVal;   // LIST and TUPLE
    ToDict dictVal;                    // DICT
    ToCollectionPtr collVal;           // SET, DEQUE, QUEUE, STACK, HEAP
    std::shared_ptr<ToFunction> funcVal;
    BuiltinFn builtinVal;
    std::shared_ptr<ToClass> classVal;
    std::shared_ptr<ToInstance> instanceVal;
    std::shared_ptr<std::future<ToValuePtr>> futureVal;
    std::shared_ptr<ToGenerator> generatorVal;

    // Factory methods.
    // makeInt / makeBool / makeNone hand back shared immutable values for
    // the common cases, so hot loops stop allocating. Nothing in the
    // runtime mutates a scalar in place, which is what makes that safe.
    static ToValuePtr makeInt(int64_t v);
    static ToValuePtr makeBool(bool v);
    static ToValuePtr makeNone();

    static ToValuePtr makeFloat(double v) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::FLOAT;
        val->floatVal = v;
        return val;
    }
    static ToValuePtr makeString(const std::string& v) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::STRING;
        val->strVal = v;
        return val;
    }
    static ToValuePtr makeString(std::string&& v) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::STRING;
        val->strVal = std::move(v);
        return val;
    }
    static ToValuePtr makeList(std::vector<ToValuePtr> v) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::LIST;
        val->listVal = std::move(v);
        return val;
    }
    static ToValuePtr makeTuple(std::vector<ToValuePtr> v) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::TUPLE;
        val->listVal = std::move(v);
        return val;
    }
    static ToValuePtr makeDict(ToDict v) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::DICT;
        val->dictVal = std::move(v);
        return val;
    }
    static ToValuePtr makeCollection(Type t, ToCollectionPtr c) {
        auto val = std::make_shared<ToValue>();
        val->type = t;
        val->collVal = c ? std::move(c) : std::make_shared<ToCollection>();
        return val;
    }
    static ToValuePtr makeSet(std::vector<ToValuePtr> items = {});
    static ToValuePtr makeDeque(std::vector<ToValuePtr> items = {});
    static ToValuePtr makeQueue(std::vector<ToValuePtr> items = {});
    static ToValuePtr makeStack(std::vector<ToValuePtr> items = {});
    static ToValuePtr makeHeap(std::vector<ToValuePtr> items = {}, bool maxHeap = false);

    static ToValuePtr makeFunction(std::shared_ptr<ToFunction> f) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::FUNCTION;
        val->funcVal = std::move(f);
        return val;
    }
    static ToValuePtr makeBuiltin(BuiltinFn f) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::BUILTIN;
        val->builtinVal = std::move(f);
        return val;
    }
    static ToValuePtr makeClass(std::shared_ptr<ToClass> c) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::CLASS;
        val->classVal = std::move(c);
        return val;
    }
    static ToValuePtr makeInstance(std::shared_ptr<ToInstance> i) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::INSTANCE;
        val->instanceVal = std::move(i);
        return val;
    }
    static ToValuePtr makeFuture(std::shared_ptr<std::future<ToValuePtr>> f) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::FUTURE;
        val->futureVal = std::move(f);
        return val;
    }
    static ToValuePtr makeGenerator(std::shared_ptr<ToGenerator> g) {
        auto val = std::make_shared<ToValue>();
        val->type = Type::GENERATOR;
        val->generatorVal = std::move(g);
        return val;
    }

    // How the value prints.
    std::string toString() const;
    // How the value prints *inside* a collection — strings gain quotes.
    std::string repr() const;

    bool isTruthy() const;
    std::string typeName() const { return valueTypeName(*this); }

    // True for LIST and TUPLE, which share the listVal payload.
    bool isSequence() const { return type == Type::LIST || type == Type::TUPLE; }
    // True for the five collections behind collVal.
    bool isCollection() const {
        return type == Type::SET || type == Type::DEQUE || type == Type::QUEUE ||
               type == Type::STACK || type == Type::HEAP;
    }
    // Every element of any ordered value, in iteration order.
    std::vector<ToValuePtr> elements() const;
    // Number of elements, or -1 when the value has no length.
    int64_t length() const;
};

class Environment : public std::enable_shared_from_this<Environment> {
public:
    explicit Environment(EnvPtr parent = nullptr) : parent(parent) {}

    ToValuePtr get(const std::string& name) const {
        auto it = values.find(name);
        if (it != values.end()) return it->second;
        if (parent) return parent->get(name);
        return nullptr;
    }

    bool has(const std::string& name) const {
        if (values.count(name)) return true;
        if (parent) return parent->has(name);
        return false;
    }

    void set(const std::string& name, ToValuePtr value) {
        // Check if constant
        if (constants.count(name)) {
            throw std::runtime_error("Cannot reassign constant '" + name + "'");
        }
        // If variable exists in a parent scope, update it there
        if (values.find(name) == values.end() && parent && parent->has(name)) {
            parent->set(name, value);
            return;
        }
        values[name] = std::move(value);
    }

    void define(const std::string& name, ToValuePtr value) {
        values[name] = std::move(value);
    }

    void defineConst(const std::string& name, ToValuePtr value) {
        values[name] = std::move(value);
        constants.insert(name);
    }

    bool isConst(const std::string& name) const {
        if (constants.count(name)) return true;
        if (parent) return parent->isConst(name);
        return false;
    }

    EnvPtr createChild() {
        return std::make_shared<Environment>(shared_from_this());
    }

private:
    EnvPtr parent;
    std::unordered_map<std::string, ToValuePtr> values;
    std::unordered_set<std::string> constants;
};
