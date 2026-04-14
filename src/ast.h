#pragma once
#include <string>
#include <vector>
#include <memory>
#include <variant>

// Forward declarations
struct ASTNode;
using ASTNodePtr = std::shared_ptr<ASTNode>;

// All AST node types
enum class NodeType {
    // Literals
    IntegerLiteral, FloatLiteral, StringLiteral, BoolLiteral, NoneLiteral,
    ListLiteral, DictLiteral, RangeLiteral,

    // Expressions
    Identifier, BinaryExpr, UnaryExpr, CallExpr, IndexExpr,
    MemberAccess, StringInterpolation, LambdaExpr, AsyncExpr, AwaitExpr, PipeExpr,

    // Statements
    Assignment, ConstDecl, PrintStmt, ReturnStmt,
    BreakStmt, ContinueStmt, ExpressionStmt, AssertStmt,
    DestructureList, DestructureDict,

    // Blocks
    IfBlock, GivenBlock, WhileLoop, ThroughLoop,
    FunctionDef, ClassDef, TryCatchFinally,
    ImportStmt, ShapeDef, EnumDef,

    // Program
    Program,
};

// Dictionary entry for dict literals
struct DictEntry {
    std::string key;
    ASTNodePtr value;
};

// Given branch
struct GivenBranch {
    ASTNodePtr condition; // null for else branch
    std::vector<ASTNodePtr> body;
};

// If branch (for or clauses)
struct IfBranch {
    ASTNodePtr condition;
    std::vector<ASTNodePtr> body;
};

// Catch clause
struct CatchClause {
    std::string errorName;
    std::vector<ASTNodePtr> body;
};

// Method definition inside a class
struct MethodDef {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> paramTypes; // optional type hints
    std::string returnTypeHint;
    std::vector<ASTNodePtr> body;
    int line;
};

// AST Node — a single struct with a type tag and fields
// Uses a flat structure for simplicity
struct ASTNode {
    NodeType type;
    int line = 0;
    int column = 0;

    // Literal values
    int64_t intValue = 0;
    double floatValue = 0.0;
    std::string stringValue;
    bool boolValue = false;

    // For identifiers, function names, class names, variable names
    std::string name;

    // For binary/unary expressions
    std::string op;
    ASTNodePtr left;
    ASTNodePtr right;
    ASTNodePtr operand; // unary

    // For function calls
    ASTNodePtr callee;
    std::vector<ASTNodePtr> arguments;

    // For member access: object.member
    ASTNodePtr object;
    std::string member;

    // For index access: list[index]
    ASTNodePtr indexExpr;

    // For assignment
    ASTNodePtr target;  // the thing being assigned to
    ASTNodePtr value;   // the value being assigned

    // For list/dict literals
    std::vector<ASTNodePtr> elements;
    std::vector<DictEntry> entries;

    // For range literal (start..end)
    ASTNodePtr rangeStart;
    ASTNodePtr rangeEnd;

    // For string interpolation
    // elements vector is reused — alternating string parts and expression parts
    // (tagged by stringValue == "__interp_str__" or "__interp_expr__")

    // For if/or/else
    ASTNodePtr condition;
    std::vector<ASTNodePtr> body;
    std::vector<IfBranch> orBranches;
    std::vector<ASTNodePtr> elseBody;

    // For given
    ASTNodePtr givenExpr;
    std::vector<GivenBranch> givenBranches;

    // For through loop
    ASTNodePtr iterable;
    std::string loopVar;

    // For function definition
    std::vector<std::string> params;
    std::vector<std::string> paramTypes; // optional type hints (empty string = no hint)
    std::string returnTypeHint; // optional return type hint

    // For class definition
    std::string parentClass; // inheritance: build Dog from Animal
    std::vector<MethodDef> methods;

    // For try/catch/finally
    std::vector<ASTNodePtr> tryBody;
    CatchClause catchClause;
    std::vector<ASTNodePtr> finallyBody;

    // For imports
    std::vector<std::string> importNames;
    std::string modulePath;

    // For destructuring
    std::vector<std::string> destructNames; // variable names to bind
    std::string restName; // ...rest variable name (empty if none)

    // For enum (options)
    std::vector<std::string> enumValues;

    // For shape
    std::vector<std::pair<std::string, std::vector<std::string>>> shapeMethodSigs; // name -> param types
    std::string fitsShape; // class fits Shape

    // For program
    std::vector<ASTNodePtr> statements;

    // For compound assignment (+=, etc.)
    std::string assignOp;

    // Helper constructors
    static ASTNodePtr makeInt(int64_t val, int line) {
        auto n = std::make_shared<ASTNode>();
        n->type = NodeType::IntegerLiteral;
        n->intValue = val;
        n->line = line;
        return n;
    }

    static ASTNodePtr makeFloat(double val, int line) {
        auto n = std::make_shared<ASTNode>();
        n->type = NodeType::FloatLiteral;
        n->floatValue = val;
        n->line = line;
        return n;
    }

    static ASTNodePtr makeString(const std::string& val, int line) {
        auto n = std::make_shared<ASTNode>();
        n->type = NodeType::StringLiteral;
        n->stringValue = val;
        n->line = line;
        return n;
    }

    static ASTNodePtr makeBool(bool val, int line) {
        auto n = std::make_shared<ASTNode>();
        n->type = NodeType::BoolLiteral;
        n->boolValue = val;
        n->line = line;
        return n;
    }

    static ASTNodePtr makeNone(int line) {
        auto n = std::make_shared<ASTNode>();
        n->type = NodeType::NoneLiteral;
        n->line = line;
        return n;
    }

    static ASTNodePtr makeIdentifier(const std::string& name, int line) {
        auto n = std::make_shared<ASTNode>();
        n->type = NodeType::Identifier;
        n->name = name;
        n->line = line;
        return n;
    }

    static ASTNodePtr makeProgram(std::vector<ASTNodePtr> stmts) {
        auto n = std::make_shared<ASTNode>();
        n->type = NodeType::Program;
        n->statements = std::move(stmts);
        return n;
    }
};
