#include "codegen.h"
#include "error.h"
#include <fstream>
#include <cstdlib>
#include <filesystem>

CodeGenerator::CodeGenerator(const std::string& filename)
    : filename(filename) {}

void CodeGenerator::emit(const std::string& code) {
    output << code;
}

void CodeGenerator::emitLine(const std::string& code) {
    emitIndent();
    output << code << "\n";
}

void CodeGenerator::emitIndent() {
    for (int i = 0; i < indentLevel; i++) output << "    ";
}

void CodeGenerator::generateRuntime() {
    emit(R"(
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <ctype.h>

// ========================
// To Runtime (compiled)
// ========================

typedef enum { VAL_INT, VAL_FLOAT, VAL_STRING, VAL_BOOL, VAL_NONE, VAL_LIST, VAL_DICT, VAL_INSTANCE } ValType;

typedef struct ToList ToList;
typedef struct ToDictEntry ToDictEntry;
typedef struct ToDict ToDict;

typedef struct ToVal {
    ValType type;
    union {
        long long intVal;
        double floatVal;
        bool boolVal;
    };
    char* strVal;
    ToList* listVal;
    ToDict* dictVal;
} ToVal;

struct ToList {
    ToVal* items;
    int length;
    int capacity;
};

struct ToDictEntry {
    char* key;
    ToVal value;
};

struct ToDict {
    ToDictEntry* entries;
    int length;
    int capacity;
};

// Constructors
ToVal to_int(long long v) { ToVal val; memset(&val, 0, sizeof(val)); val.type = VAL_INT; val.intVal = v; return val; }
ToVal to_float(double v) { ToVal val; memset(&val, 0, sizeof(val)); val.type = VAL_FLOAT; val.floatVal = v; return val; }
ToVal to_bool(bool v) { ToVal val; memset(&val, 0, sizeof(val)); val.type = VAL_BOOL; val.boolVal = v; return val; }
ToVal to_none() { ToVal val; memset(&val, 0, sizeof(val)); val.type = VAL_NONE; return val; }
ToVal to_string(const char* s) { ToVal val; memset(&val, 0, sizeof(val)); val.type = VAL_STRING; val.strVal = strdup(s); return val; }

// List operations
ToList* to_new_list(int cap) {
    ToList* l = (ToList*)malloc(sizeof(ToList));
    l->items = (ToVal*)malloc(sizeof(ToVal) * (cap > 0 ? cap : 4));
    l->length = 0;
    l->capacity = cap > 0 ? cap : 4;
    return l;
}

ToVal to_list_val(ToList* l) { ToVal v; memset(&v, 0, sizeof(v)); v.type = VAL_LIST; v.listVal = l; return v; }

void to_list_add(ToList* l, ToVal val) {
    if (l->length >= l->capacity) {
        l->capacity *= 2;
        l->items = (ToVal*)realloc(l->items, sizeof(ToVal) * l->capacity);
    }
    l->items[l->length++] = val;
}

ToVal to_list_get(ToList* l, int idx) {
    if (idx < 0) idx += l->length;
    if (idx < 0 || idx >= l->length) { fprintf(stderr, "Error: Index out of bounds\n"); exit(1); }
    return l->items[idx];
}

void to_list_set(ToList* l, int idx, ToVal val) {
    if (idx < 0) idx += l->length;
    if (idx < 0 || idx >= l->length) { fprintf(stderr, "Error: Index out of bounds\n"); exit(1); }
    l->items[idx] = val;
}

// Dict operations
ToDict* to_new_dict(int cap) {
    ToDict* d = (ToDict*)malloc(sizeof(ToDict));
    d->entries = (ToDictEntry*)malloc(sizeof(ToDictEntry) * (cap > 0 ? cap : 4));
    d->length = 0;
    d->capacity = cap > 0 ? cap : 4;
    return d;
}

ToVal to_dict_val(ToDict* d) { ToVal v; memset(&v, 0, sizeof(v)); v.type = VAL_DICT; v.dictVal = d; return v; }

void to_dict_set(ToDict* d, const char* key, ToVal val) {
    for (int i = 0; i < d->length; i++) {
        if (strcmp(d->entries[i].key, key) == 0) {
            d->entries[i].value = val;
            return;
        }
    }
    if (d->length >= d->capacity) {
        d->capacity *= 2;
        d->entries = (ToDictEntry*)realloc(d->entries, sizeof(ToDictEntry) * d->capacity);
    }
    d->entries[d->length].key = strdup(key);
    d->entries[d->length].value = val;
    d->length++;
}

ToVal to_dict_get(ToDict* d, const char* key) {
    for (int i = 0; i < d->length; i++) {
        if (strcmp(d->entries[i].key, key) == 0) return d->entries[i].value;
    }
    fprintf(stderr, "Error: Key not found: %s\n", key);
    exit(1);
}

bool to_dict_has(ToDict* d, const char* key) {
    for (int i = 0; i < d->length; i++) {
        if (strcmp(d->entries[i].key, key) == 0) return true;
    }
    return false;
}

// Print
char* to_val_to_str(ToVal v) {
    static char buf[4096];
    switch (v.type) {
        case VAL_INT: snprintf(buf, sizeof(buf), "%lld", v.intVal); break;
        case VAL_FLOAT: snprintf(buf, sizeof(buf), "%g", v.floatVal); break;
        case VAL_STRING: return v.strVal;
        case VAL_BOOL: snprintf(buf, sizeof(buf), "%s", v.boolVal ? "true" : "false"); break;
        case VAL_NONE: snprintf(buf, sizeof(buf), "none"); break;
        case VAL_LIST: {
            int pos = 0;
            pos += snprintf(buf + pos, sizeof(buf) - pos, "[");
            for (int i = 0; i < v.listVal->length; i++) {
                if (i > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, ", ");
                if (v.listVal->items[i].type == VAL_STRING)
                    pos += snprintf(buf + pos, sizeof(buf) - pos, "\"%s\"", to_val_to_str(v.listVal->items[i]));
                else
                    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", to_val_to_str(v.listVal->items[i]));
            }
            snprintf(buf + pos, sizeof(buf) - pos, "]");
            break;
        }
        case VAL_DICT: {
            int pos = 0;
            pos += snprintf(buf + pos, sizeof(buf) - pos, "{");
            for (int i = 0; i < v.dictVal->length; i++) {
                if (i > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, ", ");
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s = %s", v.dictVal->entries[i].key, to_val_to_str(v.dictVal->entries[i].value));
            }
            snprintf(buf + pos, sizeof(buf) - pos, "}");
            break;
        }
        default: snprintf(buf, sizeof(buf), "<object>"); break;
    }
    return buf;
}

void to_print(ToVal val) { printf("%s\n", to_val_to_str(val)); }

// Arithmetic
double to_as_float(ToVal v) { if (v.type == VAL_INT) return (double)v.intVal; return v.floatVal; }
bool to_truthy(ToVal v) {
    switch (v.type) {
        case VAL_BOOL: return v.boolVal;
        case VAL_INT: return v.intVal != 0;
        case VAL_FLOAT: return v.floatVal != 0.0;
        case VAL_STRING: return v.strVal && strlen(v.strVal) > 0;
        case VAL_NONE: return false;
        case VAL_LIST: return v.listVal && v.listVal->length > 0;
        default: return true;
    }
}

ToVal to_add(ToVal a, ToVal b) {
    if (a.type == VAL_STRING || b.type == VAL_STRING) {
        char buf[8192];
        snprintf(buf, sizeof(buf), "%s%s", to_val_to_str(a), to_val_to_str(b));
        return to_string(buf);
    }
    if (a.type == VAL_FLOAT || b.type == VAL_FLOAT)
        return to_float(to_as_float(a) + to_as_float(b));
    return to_int(a.intVal + b.intVal);
}
ToVal to_sub(ToVal a, ToVal b) {
    if (a.type == VAL_FLOAT || b.type == VAL_FLOAT)
        return to_float(to_as_float(a) - to_as_float(b));
    return to_int(a.intVal - b.intVal);
}
ToVal to_mul(ToVal a, ToVal b) {
    if (a.type == VAL_FLOAT || b.type == VAL_FLOAT)
        return to_float(to_as_float(a) * to_as_float(b));
    return to_int(a.intVal * b.intVal);
}
ToVal to_div(ToVal a, ToVal b) {
    double bv = to_as_float(b);
    if (bv == 0) { fprintf(stderr, "Error: Division by zero\n"); exit(1); }
    if (a.type == VAL_INT && b.type == VAL_INT && a.intVal % b.intVal == 0)
        return to_int(a.intVal / b.intVal);
    return to_float(to_as_float(a) / bv);
}
ToVal to_mod(ToVal a, ToVal b) {
    if (b.intVal == 0) { fprintf(stderr, "Error: Modulo by zero\n"); exit(1); }
    return to_int(a.intVal % b.intVal);
}
ToVal to_neg(ToVal a) { if (a.type == VAL_INT) return to_int(-a.intVal); return to_float(-a.floatVal); }

// Comparison
ToVal to_eq(ToVal a, ToVal b) {
    if (a.type != b.type) return to_bool(false);
    switch (a.type) {
        case VAL_INT: return to_bool(a.intVal == b.intVal);
        case VAL_FLOAT: return to_bool(a.floatVal == b.floatVal);
        case VAL_STRING: return to_bool(strcmp(a.strVal, b.strVal) == 0);
        case VAL_BOOL: return to_bool(a.boolVal == b.boolVal);
        case VAL_NONE: return to_bool(true);
        default: return to_bool(false);
    }
}
ToVal to_neq(ToVal a, ToVal b) { return to_bool(!to_eq(a, b).boolVal); }
ToVal to_lt(ToVal a, ToVal b) { return to_bool(to_as_float(a) < to_as_float(b)); }
ToVal to_lte(ToVal a, ToVal b) { return to_bool(to_as_float(a) <= to_as_float(b)); }
ToVal to_gt(ToVal a, ToVal b) { return to_bool(to_as_float(a) > to_as_float(b)); }
ToVal to_gte(ToVal a, ToVal b) { return to_bool(to_as_float(a) >= to_as_float(b)); }

// Input
ToVal to_input(ToVal prompt) {
    if (prompt.type == VAL_STRING) printf("%s", prompt.strVal);
    char line[4096];
    if (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = 0;
        return to_string(line);
    }
    return to_string("");
}

// String methods
ToVal to_str_upper(ToVal s) {
    char* r = strdup(s.strVal);
    for (int i = 0; r[i]; i++) r[i] = toupper(r[i]);
    ToVal v = to_string(r); free(r); return v;
}
ToVal to_str_lower(ToVal s) {
    char* r = strdup(s.strVal);
    for (int i = 0; r[i]; i++) r[i] = tolower(r[i]);
    ToVal v = to_string(r); free(r); return v;
}
int to_str_len(ToVal s) { return s.type == VAL_STRING ? strlen(s.strVal) : 0; }
int to_len(ToVal v) {
    if (v.type == VAL_STRING) return strlen(v.strVal);
    if (v.type == VAL_LIST) return v.listVal->length;
    if (v.type == VAL_DICT) return v.dictVal->length;
    return 0;
}

)");
}

std::string CodeGenerator::generate(ASTNodePtr program) {
    output.str("");
    output.clear();

    generateRuntime();

    // Forward declarations for all functions
    emit("// Forward declarations\n");

    std::vector<ASTNodePtr> functions;
    std::vector<ASTNodePtr> mainStmts;

    for (auto& stmt : program->statements) {
        if (stmt->type == NodeType::FunctionDef) {
            functions.push_back(stmt);
        } else {
            mainStmts.push_back(stmt);
        }
    }

    for (auto& func : functions) {
        emit("ToVal fn_" + func->name + "(");
        for (size_t i = 0; i < func->params.size(); i++) {
            if (i > 0) emit(", ");
            emit("ToVal " + func->params[i]);
        }
        emit(");\n");
    }
    emit("\n");

    for (auto& func : functions) {
        generateFunctionDef(func);
        emit("\n");
    }

    emit("int main(int argc, char** argv) {\n");
    indentLevel = 1;
    for (auto& stmt : mainStmts) {
        generateStatement(stmt);
    }
    emitLine("return 0;");
    indentLevel = 0;
    emit("}\n");

    return output.str();
}

bool CodeGenerator::compile(const std::string& cSource, const std::string& outputPath) {
    std::string tmpFile = outputPath + ".c";
    {
        std::ofstream f(tmpFile);
        f << cSource;
    }

    std::string cmd = "cc -O2 -o " + outputPath + " " + tmpFile + " -lm 2>&1";
    int result = system(cmd.c_str());

    std::filesystem::remove(tmpFile);

    return result == 0;
}

void CodeGenerator::generateStatement(ASTNodePtr node) {
    switch (node->type) {
        case NodeType::PrintStmt: generatePrint(node); break;
        case NodeType::Assignment: generateAssignment(node); break;
        case NodeType::ConstDecl: {
            emitLine("const ToVal " + node->name + " = " + exprToC(node->value) + ";");
            declaredVars.insert(node->name);
            break;
        }
        case NodeType::IfBlock: generateIf(node); break;
        case NodeType::WhileLoop: generateWhile(node); break;
        case NodeType::ThroughLoop: generateThrough(node); break;
        case NodeType::ReturnStmt: {
            if (node->value) emitLine("return " + exprToC(node->value) + ";");
            else emitLine("return to_none();");
            break;
        }
        case NodeType::BreakStmt: emitLine("break;"); break;
        case NodeType::ContinueStmt: emitLine("continue;"); break;
        case NodeType::ExpressionStmt: {
            emitLine(exprToC(node->value) + ";");
            break;
        }
        case NodeType::AssertStmt: {
            emitLine("if (!to_truthy(" + exprToC(node->value) + ")) {");
            indentLevel++;
            emitLine("fprintf(stderr, \"Assertion failed at line " + std::to_string(node->line) + "\\n\");");
            emitLine("exit(1);");
            indentLevel--;
            emitLine("}");
            break;
        }
        default:
            emitLine("// unsupported statement");
    }
}

void CodeGenerator::generatePrint(ASTNodePtr node) {
    emitLine("to_print(" + exprToC(node->value) + ");");
}

void CodeGenerator::generateAssignment(ASTNodePtr node) {
    if (node->target->type == NodeType::Identifier) {
        std::string varName = node->target->name;
        if (node->assignOp == "=") {
            if (declaredVars.count(varName)) {
                emitLine(varName + " = " + exprToC(node->value) + ";");
            } else {
                declaredVars.insert(varName);
                emitLine("ToVal " + varName + " = " + exprToC(node->value) + ";");
            }
        } else if (node->assignOp == "+=") {
            emitLine(varName + " = to_add(" + varName + ", " + exprToC(node->value) + ");");
        } else if (node->assignOp == "-=") {
            emitLine(varName + " = to_sub(" + varName + ", " + exprToC(node->value) + ");");
        } else if (node->assignOp == "*=") {
            emitLine(varName + " = to_mul(" + varName + ", " + exprToC(node->value) + ");");
        } else if (node->assignOp == "/=") {
            emitLine(varName + " = to_div(" + varName + ", " + exprToC(node->value) + ");");
        }
    } else if (node->target->type == NodeType::MemberAccess) {
        // obj.member = val → dict set
        emitLine("to_dict_set(" + exprToC(node->target->object) + ".dictVal, \"" +
            node->target->member + "\", " + exprToC(node->value) + ");");
    } else if (node->target->type == NodeType::IndexExpr) {
        emitLine("{");
        indentLevel++;
        emitLine("ToVal __obj = " + exprToC(node->target->object) + ";");
        emitLine("ToVal __idx = " + exprToC(node->target->indexExpr) + ";");
        emitLine("ToVal __val = " + exprToC(node->value) + ";");
        emitLine("if (__obj.type == VAL_LIST) to_list_set(__obj.listVal, (int)__idx.intVal, __val);");
        emitLine("else if (__obj.type == VAL_DICT) to_dict_set(__obj.dictVal, __idx.strVal, __val);");
        indentLevel--;
        emitLine("}");
    }
}

void CodeGenerator::generateIf(ASTNodePtr node) {
    emitLine("if (to_truthy(" + exprToC(node->condition) + ")) {");
    indentLevel++;
    for (auto& s : node->body) generateStatement(s);
    indentLevel--;

    for (auto& branch : node->orBranches) {
        emitLine("} else if (to_truthy(" + exprToC(branch.condition) + ")) {");
        indentLevel++;
        for (auto& s : branch.body) generateStatement(s);
        indentLevel--;
    }

    if (!node->elseBody.empty()) {
        emitLine("} else {");
        indentLevel++;
        for (auto& s : node->elseBody) generateStatement(s);
        indentLevel--;
    }
    emitLine("}");
}

void CodeGenerator::generateWhile(ASTNodePtr node) {
    emitLine("while (to_truthy(" + exprToC(node->condition) + ")) {");
    indentLevel++;
    for (auto& s : node->body) generateStatement(s);
    indentLevel--;
    emitLine("}");
}

void CodeGenerator::generateThrough(ASTNodePtr node) {
    std::string iterVar = "__iter_" + std::to_string(node->line);
    std::string idxVar = "__i_" + std::to_string(node->line);

    emitLine("{");
    indentLevel++;
    emitLine("ToVal " + iterVar + " = " + exprToC(node->iterable) + ";");
    emitLine("int " + idxVar + "_len = to_len(" + iterVar + ");");
    emitLine("for (int " + idxVar + " = 0; " + idxVar + " < " + idxVar + "_len; " + idxVar + "++) {");
    indentLevel++;

    declaredVars.insert(node->loopVar);
    emitLine("ToVal " + node->loopVar + ";");
    emitLine("if (" + iterVar + ".type == VAL_LIST) " + node->loopVar + " = to_list_get(" + iterVar + ".listVal, " + idxVar + ");");
    emitLine("else if (" + iterVar + ".type == VAL_STRING) { char __c[2] = {" + iterVar + ".strVal[" + idxVar + "], 0}; " + node->loopVar + " = to_string(__c); }");

    for (auto& s : node->body) generateStatement(s);

    indentLevel--;
    emitLine("}");
    indentLevel--;
    emitLine("}");
}

void CodeGenerator::generateFunctionDef(ASTNodePtr node) {
    auto savedVars = declaredVars;
    emit("ToVal fn_" + node->name + "(");
    for (size_t i = 0; i < node->params.size(); i++) {
        if (i > 0) emit(", ");
        emit("ToVal " + node->params[i]);
        declaredVars.insert(node->params[i]);
    }
    emit(") {\n");
    indentLevel++;
    for (auto& stmt : node->body) {
        generateStatement(stmt);
    }
    emitLine("return to_none();");
    indentLevel--;
    emit("}\n");
    declaredVars = savedVars;
}

std::string CodeGenerator::exprToC(ASTNodePtr node) {
    switch (node->type) {
        case NodeType::IntegerLiteral:
            return "to_int(" + std::to_string(node->intValue) + "LL)";
        case NodeType::FloatLiteral:
            return "to_float(" + std::to_string(node->floatValue) + ")";
        case NodeType::StringLiteral: {
            std::string escaped;
            for (char c : node->stringValue) {
                if (c == '"') escaped += "\\\"";
                else if (c == '\\') escaped += "\\\\";
                else if (c == '\n') escaped += "\\n";
                else if (c == '\t') escaped += "\\t";
                else escaped += c;
            }
            return "to_string(\"" + escaped + "\")";
        }
        case NodeType::BoolLiteral:
            return "to_bool(" + std::string(node->boolValue ? "true" : "false") + ")";
        case NodeType::NoneLiteral:
            return "to_none()";
        case NodeType::Identifier: {
            // Map built-in function names
            if (node->name == "len") return "((ToVal(*)(ToVal))0) /* use to_len */";
            return node->name;
        }
        case NodeType::BinaryExpr: {
            if (node->op == "..") {
                // Range → create list
                std::string l = exprToC(node->left);
                std::string r = exprToC(node->right);
                return "({ ToList* __rl = to_new_list(0); "
                       "for (long long __ri = (" + l + ").intVal; __ri < (" + r + ").intVal; __ri++) "
                       "to_list_add(__rl, to_int(__ri)); to_list_val(__rl); })";
            }
            std::string l = exprToC(node->left);
            std::string r = exprToC(node->right);
            if (node->op == "+") return "to_add(" + l + ", " + r + ")";
            if (node->op == "-") return "to_sub(" + l + ", " + r + ")";
            if (node->op == "*") return "to_mul(" + l + ", " + r + ")";
            if (node->op == "/") return "to_div(" + l + ", " + r + ")";
            if (node->op == "%") return "to_mod(" + l + ", " + r + ")";
            if (node->op == "==") return "to_eq(" + l + ", " + r + ")";
            if (node->op == "!=") return "to_neq(" + l + ", " + r + ")";
            if (node->op == "<") return "to_lt(" + l + ", " + r + ")";
            if (node->op == "<=") return "to_lte(" + l + ", " + r + ")";
            if (node->op == ">") return "to_gt(" + l + ", " + r + ")";
            if (node->op == ">=") return "to_gte(" + l + ", " + r + ")";
            if (node->op == "and") return "to_bool(to_truthy(" + l + ") && to_truthy(" + r + "))";
            if (node->op == "or") return "to_bool(to_truthy(" + l + ") || to_truthy(" + r + "))";
            return "to_none()";
        }
        case NodeType::UnaryExpr: {
            std::string op = exprToC(node->operand);
            if (node->op == "-") return "to_neg(" + op + ")";
            if (node->op == "not") return "to_bool(!to_truthy(" + op + "))";
            return "to_none()";
        }
        case NodeType::CallExpr: {
            if (node->callee->type == NodeType::Identifier) {
                std::string name = node->callee->name;
                std::string args;
                for (size_t i = 0; i < node->arguments.size(); i++) {
                    if (i > 0) args += ", ";
                    args += exprToC(node->arguments[i]);
                }
                // Map builtins
                if (name == "print") return "(to_print(" + args + "), to_none())";
                if (name == "input") return "to_input(" + args + ")";
                if (name == "len") return "to_int(to_len(" + args + "))";
                if (name == "str") return "to_string(to_val_to_str(" + args + "))";
                if (name == "int") return "to_int((long long)to_as_float(" + args + "))";
                if (name == "float") return "to_float(to_as_float(" + args + "))";
                if (name == "abs") return "({ ToVal __a = " + args + "; __a.type == VAL_INT ? to_int(__a.intVal < 0 ? -__a.intVal : __a.intVal) : to_float(fabs(__a.floatVal)); })";
                if (name == "type") return "({ ToVal __t = " + args + "; to_string(__t.type == VAL_INT ? \"int\" : __t.type == VAL_FLOAT ? \"float\" : __t.type == VAL_STRING ? \"string\" : __t.type == VAL_BOOL ? \"bool\" : __t.type == VAL_NONE ? \"none\" : __t.type == VAL_LIST ? \"list\" : \"dict\"); })";
                return "fn_" + name + "(" + args + ")";
            }
            if (node->callee->type == NodeType::MemberAccess) {
                auto obj = exprToC(node->callee->object);
                std::string method = node->callee->member;
                std::string args;
                for (size_t i = 0; i < node->arguments.size(); i++) {
                    if (i > 0) args += ", ";
                    args += exprToC(node->arguments[i]);
                }
                // List methods
                if (method == "add") return "(to_list_add((" + obj + ").listVal, " + args + "), to_none())";
                if (method == "length") return "to_int(to_len(" + obj + "))";
                if (method == "has") return "to_bool(to_dict_has((" + obj + ").dictVal, (" + args + ").strVal))";
                // String methods
                if (method == "upper") return "to_str_upper(" + obj + ")";
                if (method == "lower") return "to_str_lower(" + obj + ")";
                // Dict method calls
                return "to_dict_get((" + obj + ").dictVal, \"" + method + "\")";
            }
            return "to_none()";
        }
        case NodeType::MemberAccess: {
            std::string obj = exprToC(node->object);
            if (node->member == "length") {
                return "to_int(to_len(" + obj + "))";
            }
            return "to_dict_get((" + obj + ").dictVal, \"" + node->member + "\")";
        }
        case NodeType::IndexExpr: {
            std::string obj = exprToC(node->object);
            std::string idx = exprToC(node->indexExpr);
            return "({ ToVal __o = " + obj + "; ToVal __i = " + idx + "; "
                   "__o.type == VAL_LIST ? to_list_get(__o.listVal, (int)__i.intVal) : "
                   "__o.type == VAL_DICT ? to_dict_get(__o.dictVal, __i.strVal) : "
                   "__o.type == VAL_STRING ? ({ char __c[2] = {__o.strVal[(int)__i.intVal], 0}; to_string(__c); }) : "
                   "to_none(); })";
        }
        case NodeType::ListLiteral: {
            std::string result = "({ ToList* __l = to_new_list(" + std::to_string(node->elements.size()) + "); ";
            for (auto& elem : node->elements) {
                result += "to_list_add(__l, " + exprToC(elem) + "); ";
            }
            result += "to_list_val(__l); })";
            return result;
        }
        case NodeType::DictLiteral: {
            std::string result = "({ ToDict* __d = to_new_dict(" + std::to_string(node->entries.size()) + "); ";
            for (auto& entry : node->entries) {
                result += "to_dict_set(__d, \"" + entry.key + "\", " + exprToC(entry.value) + "); ";
            }
            result += "to_dict_val(__d); })";
            return result;
        }
        case NodeType::StringInterpolation: {
            std::string result;
            for (size_t i = 0; i < node->elements.size(); i++) {
                std::string part;
                if (node->elements[i]->type == NodeType::StringLiteral) {
                    part = exprToC(node->elements[i]);
                } else {
                    part = "to_string(to_val_to_str(" + exprToC(node->elements[i]) + "))";
                }
                if (i == 0) result = part;
                else result = "to_add(" + result + ", " + part + ")";
            }
            return result;
        }
        default:
            return "to_none()";
    }
}

void CodeGenerator::generateBlock(const std::vector<ASTNodePtr>& stmts) {
    for (auto& stmt : stmts) {
        generateStatement(stmt);
    }
}
