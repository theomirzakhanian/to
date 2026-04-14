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

// To runtime support
typedef enum { VAL_INT, VAL_FLOAT, VAL_STRING, VAL_BOOL, VAL_NONE } ValType;

typedef struct ToVal {
    ValType type;
    union {
        long long intVal;
        double floatVal;
        bool boolVal;
    };
    char* strVal;
} ToVal;

ToVal to_int(long long v) { ToVal val; val.type = VAL_INT; val.intVal = v; val.strVal = NULL; return val; }
ToVal to_float(double v) { ToVal val; val.type = VAL_FLOAT; val.floatVal = v; val.strVal = NULL; return val; }
ToVal to_bool(bool v) { ToVal val; val.type = VAL_BOOL; val.boolVal = v; val.strVal = NULL; return val; }
ToVal to_none() { ToVal val; val.type = VAL_NONE; val.intVal = 0; val.strVal = NULL; return val; }
ToVal to_string(const char* s) {
    ToVal val;
    val.type = VAL_STRING;
    val.strVal = strdup(s);
    return val;
}

void to_print(ToVal val) {
    switch (val.type) {
        case VAL_INT: printf("%lld\n", val.intVal); break;
        case VAL_FLOAT: printf("%g\n", val.floatVal); break;
        case VAL_STRING: printf("%s\n", val.strVal); break;
        case VAL_BOOL: printf("%s\n", val.boolVal ? "true" : "false"); break;
        case VAL_NONE: printf("none\n"); break;
    }
}

double to_as_float(ToVal v) {
    if (v.type == VAL_INT) return (double)v.intVal;
    return v.floatVal;
}

bool to_truthy(ToVal v) {
    switch (v.type) {
        case VAL_BOOL: return v.boolVal;
        case VAL_INT: return v.intVal != 0;
        case VAL_FLOAT: return v.floatVal != 0.0;
        case VAL_STRING: return v.strVal && strlen(v.strVal) > 0;
        case VAL_NONE: return false;
    }
    return false;
}

ToVal to_add(ToVal a, ToVal b) {
    if (a.type == VAL_STRING || b.type == VAL_STRING) {
        char buf[4096];
        char abuf[2048], bbuf[2048];
        if (a.type == VAL_STRING) snprintf(abuf, sizeof(abuf), "%s", a.strVal);
        else if (a.type == VAL_INT) snprintf(abuf, sizeof(abuf), "%lld", a.intVal);
        else snprintf(abuf, sizeof(abuf), "%g", a.floatVal);
        if (b.type == VAL_STRING) snprintf(bbuf, sizeof(bbuf), "%s", b.strVal);
        else if (b.type == VAL_INT) snprintf(bbuf, sizeof(bbuf), "%lld", b.intVal);
        else snprintf(bbuf, sizeof(bbuf), "%g", b.floatVal);
        snprintf(buf, sizeof(buf), "%s%s", abuf, bbuf);
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

ToVal to_eq(ToVal a, ToVal b) {
    if (a.type != b.type) return to_bool(false);
    switch (a.type) {
        case VAL_INT: return to_bool(a.intVal == b.intVal);
        case VAL_FLOAT: return to_bool(a.floatVal == b.floatVal);
        case VAL_STRING: return to_bool(strcmp(a.strVal, b.strVal) == 0);
        case VAL_BOOL: return to_bool(a.boolVal == b.boolVal);
        case VAL_NONE: return to_bool(true);
    }
    return to_bool(false);
}

ToVal to_neq(ToVal a, ToVal b) { return to_bool(!to_eq(a, b).boolVal); }
ToVal to_lt(ToVal a, ToVal b) { return to_bool(to_as_float(a) < to_as_float(b)); }
ToVal to_lte(ToVal a, ToVal b) { return to_bool(to_as_float(a) <= to_as_float(b)); }
ToVal to_gt(ToVal a, ToVal b) { return to_bool(to_as_float(a) > to_as_float(b)); }
ToVal to_gte(ToVal a, ToVal b) { return to_bool(to_as_float(a) >= to_as_float(b)); }
ToVal to_neg(ToVal a) {
    if (a.type == VAL_INT) return to_int(-a.intVal);
    return to_float(-a.floatVal);
}

// String formatting helper for interpolation
char* to_val_to_str(ToVal v) {
    static char buf[4096];
    switch (v.type) {
        case VAL_INT: snprintf(buf, sizeof(buf), "%lld", v.intVal); break;
        case VAL_FLOAT: snprintf(buf, sizeof(buf), "%g", v.floatVal); break;
        case VAL_STRING: return v.strVal;
        case VAL_BOOL: snprintf(buf, sizeof(buf), "%s", v.boolVal ? "true" : "false"); break;
        case VAL_NONE: snprintf(buf, sizeof(buf), "none"); break;
    }
    return buf;
}

)");
}

std::string CodeGenerator::generate(ASTNodePtr program) {
    output.str("");
    output.clear();

    generateRuntime();

    // Forward declarations for all functions
    emit("// Forward declarations\n");

    // Collect top-level function definitions
    std::vector<ASTNodePtr> functions;
    std::vector<ASTNodePtr> mainStmts;

    for (auto& stmt : program->statements) {
        if (stmt->type == NodeType::FunctionDef) {
            functions.push_back(stmt);
        } else {
            mainStmts.push_back(stmt);
        }
    }

    // Emit function forward declarations
    for (auto& func : functions) {
        emit("ToVal fn_" + func->name + "(");
        for (size_t i = 0; i < func->params.size(); i++) {
            if (i > 0) emit(", ");
            emit("ToVal " + func->params[i]);
        }
        emit(");\n");
    }
    emit("\n");

    // Emit function definitions
    for (auto& func : functions) {
        generateFunctionDef(func);
        emit("\n");
    }

    // Emit main
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
    // Write C source to temp file
    std::string tmpFile = outputPath + ".c";
    {
        std::ofstream f(tmpFile);
        f << cSource;
    }

    // Compile with system compiler
    std::string cmd = "cc -O2 -o " + outputPath + " " + tmpFile + " -lm 2>&1";
    int result = system(cmd.c_str());

    // Clean up temp file
    std::filesystem::remove(tmpFile);

    return result == 0;
}

void CodeGenerator::generateStatement(ASTNodePtr node) {
    switch (node->type) {
        case NodeType::PrintStmt: generatePrint(node); break;
        case NodeType::Assignment: generateAssignment(node); break;
        case NodeType::ConstDecl: {
            emitLine("const ToVal " + node->name + " = " + exprToC(node->value) + ";");
            break;
        }
        case NodeType::IfBlock: generateIf(node); break;
        case NodeType::WhileLoop: generateWhile(node); break;
        case NodeType::ThroughLoop: generateThrough(node); break;
        case NodeType::ReturnStmt: {
            if (node->value) {
                emitLine("return " + exprToC(node->value) + ";");
            } else {
                emitLine("return to_none();");
            }
            break;
        }
        case NodeType::BreakStmt: emitLine("break;"); break;
        case NodeType::ContinueStmt: emitLine("continue;"); break;
        case NodeType::ExpressionStmt: {
            emitLine(exprToC(node->value) + ";");
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
    } else {
        emitLine("// complex assignment target unsupported in codegen");
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
    // Simplified — only works for range-based iteration
    emitLine("// through loop");
    emitLine("{");
    indentLevel++;
    // This is simplified; a full implementation would need list iteration support
    emitLine("// simplified iteration");
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
            // Escape the string for C
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
        case NodeType::Identifier:
            return node->name;
        case NodeType::BinaryExpr: {
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
            return "to_none() /* unsupported op: " + node->op + " */";
        }
        case NodeType::UnaryExpr: {
            std::string op = exprToC(node->operand);
            if (node->op == "-") return "to_neg(" + op + ")";
            if (node->op == "not") return "to_bool(!to_truthy(" + op + "))";
            return "to_none()";
        }
        case NodeType::CallExpr: {
            if (node->callee->type == NodeType::Identifier) {
                std::string args;
                for (size_t i = 0; i < node->arguments.size(); i++) {
                    if (i > 0) args += ", ";
                    args += exprToC(node->arguments[i]);
                }
                return "fn_" + node->callee->name + "(" + args + ")";
            }
            return "to_none() /* unsupported call */";
        }
        case NodeType::StringInterpolation: {
            // Build concatenation
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
            return "to_none() /* unsupported expr */";
    }
}

void CodeGenerator::generateBlock(const std::vector<ASTNodePtr>& stmts) {
    for (auto& stmt : stmts) {
        generateStatement(stmt);
    }
}
