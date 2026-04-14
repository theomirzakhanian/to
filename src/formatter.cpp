#include "formatter.h"
#include <algorithm>

void Formatter::emitLine(const std::string& line) {
    for (int i = 0; i < indent * indentSize; i++) out << ' ';
    out << line << "\n";
}

void Formatter::emitBlank() {
    out << "\n";
}

std::string Formatter::format(ASTNodePtr program) {
    out.str("");
    out.clear();
    indent = 0;

    bool lastWasBlock = false;
    for (size_t i = 0; i < program->statements.size(); i++) {
        auto& stmt = program->statements[i];
        // Add blank line before function/class defs (unless at start)
        bool isBlock = stmt->type == NodeType::FunctionDef ||
                       stmt->type == NodeType::ClassDef ||
                       stmt->type == NodeType::ShapeDef ||
                       stmt->type == NodeType::EnumDef;
        if (isBlock && i > 0) emitBlank();
        formatNode(stmt);
        if (isBlock && i < program->statements.size() - 1) emitBlank();
        lastWasBlock = isBlock;
    }

    std::string result = out.str();
    // Trim trailing whitespace on each line
    std::ostringstream cleaned;
    std::istringstream lines(result);
    std::string line;
    while (std::getline(lines, line)) {
        size_t end = line.find_last_not_of(" \t");
        if (end != std::string::npos) {
            cleaned << line.substr(0, end + 1) << "\n";
        } else {
            cleaned << "\n";
        }
    }
    // Remove trailing blank lines
    result = cleaned.str();
    while (result.size() > 1 && result[result.size()-1] == '\n' && result[result.size()-2] == '\n') {
        result.pop_back();
    }
    return result;
}

void Formatter::formatBlock(const std::vector<ASTNodePtr>& stmts) {
    indent++;
    for (auto& stmt : stmts) {
        formatNode(stmt);
    }
    indent--;
}

void Formatter::formatNode(ASTNodePtr node) {
    switch (node->type) {
        case NodeType::PrintStmt:
            emitLine("print " + formatExpr(node->value));
            break;

        case NodeType::ReturnStmt:
            if (node->value) emitLine("return " + formatExpr(node->value));
            else emitLine("return");
            break;

        case NodeType::YieldStmt:
            if (node->value) emitLine("yield " + formatExpr(node->value));
            else emitLine("yield");
            break;

        case NodeType::BreakStmt:
            emitLine("break");
            break;

        case NodeType::ContinueStmt:
            emitLine("continue");
            break;

        case NodeType::AssertStmt:
            emitLine("assert " + formatExpr(node->value));
            break;

        case NodeType::ExpressionStmt:
            emitLine(formatExpr(node->value));
            break;

        case NodeType::Assignment: {
            std::string target = formatExpr(node->target);
            std::string val = formatExpr(node->value);
            emitLine(target + " " + node->assignOp + " " + val);
            break;
        }

        case NodeType::ConstDecl:
            emitLine("const " + node->name + " = " + formatExpr(node->value));
            break;

        case NodeType::DestructureList: {
            std::string lhs = "[";
            for (size_t i = 0; i < node->destructNames.size(); i++) {
                if (i > 0) lhs += ", ";
                lhs += node->destructNames[i];
            }
            if (!node->restName.empty()) {
                if (!node->destructNames.empty()) lhs += ", ";
                lhs += "..." + node->restName;
            }
            lhs += "]";
            emitLine(lhs + " = " + formatExpr(node->value));
            break;
        }

        case NodeType::DestructureDict: {
            std::string lhs = "{";
            for (size_t i = 0; i < node->destructNames.size(); i++) {
                if (i > 0) lhs += ", ";
                lhs += node->destructNames[i];
            }
            lhs += "}";
            emitLine(lhs + " = " + formatExpr(node->value));
            break;
        }

        case NodeType::IfBlock: {
            emitLine("if " + formatExpr(node->condition) + ":");
            formatBlock(node->body);
            for (auto& br : node->orBranches) {
                emitLine("or " + formatExpr(br.condition) + ":");
                formatBlock(br.body);
            }
            if (!node->elseBody.empty()) {
                emitLine("else:");
                formatBlock(node->elseBody);
            }
            break;
        }

        case NodeType::GivenBlock: {
            emitLine("given " + formatExpr(node->givenExpr) + ":");
            indent++;
            for (auto& br : node->givenBranches) {
                if (!br.condition && br.patternKind == 0) {
                    emitLine("else:");
                } else if (br.patternKind == 1) {
                    // Dict pattern
                    std::string pat = "{";
                    for (size_t i = 0; i < br.patternKeys.size(); i++) {
                        if (i > 0) pat += ", ";
                        pat += br.patternKeys[i];
                        if (br.patternValues[i]) {
                            pat += ": " + formatExpr(br.patternValues[i]);
                        }
                    }
                    pat += "}";
                    emitLine("if " + pat + ":");
                } else if (br.patternKind == 2) {
                    std::string pat = "[";
                    for (size_t i = 0; i < br.patternBindings.size(); i++) {
                        if (i > 0) pat += ", ";
                        pat += br.patternBindings[i];
                    }
                    if (!br.patternRestName.empty()) {
                        if (!br.patternBindings.empty()) pat += ", ";
                        pat += "..." + br.patternRestName;
                    }
                    pat += "]";
                    emitLine("if " + pat + ":");
                } else if (br.patternKind == 3) {
                    std::string pat = br.patternTag + "(";
                    for (size_t i = 0; i < br.patternBindings.size(); i++) {
                        if (i > 0) pat += ", ";
                        pat += br.patternBindings[i];
                    }
                    pat += ")";
                    emitLine("if " + pat + ":");
                } else {
                    emitLine("if " + formatExpr(br.condition) + ":");
                }
                formatBlock(br.body);
            }
            indent--;
            break;
        }

        case NodeType::WhileLoop:
            emitLine("while " + formatExpr(node->condition) + ":");
            formatBlock(node->body);
            break;

        case NodeType::ThroughLoop:
            emitLine("through " + formatExpr(node->iterable) + " as " + node->loopVar + ":");
            formatBlock(node->body);
            break;

        case NodeType::FunctionDef: {
            std::string header = "to " + node->name + "(";
            header += formatParams(node->params, node->paramTypes, node->returnTypeHint);
            header += "):";
            // Decorators
            for (auto& dec : node->decorators) {
                emitLine("@" + dec);
            }
            emitLine(header);
            formatBlock(node->body);
            break;
        }

        case NodeType::ClassDef: {
            std::string header = "build " + node->name;
            if (!node->fitsShape.empty()) header += " fits " + node->fitsShape;
            if (!node->parentClass.empty()) header += " from " + node->parentClass;
            header += ":";
            emitLine(header);
            indent++;
            for (size_t i = 0; i < node->methods.size(); i++) {
                auto& m = node->methods[i];
                if (i > 0) emitBlank();
                std::string mHeader = "to " + m.name + "(";
                mHeader += formatParams(m.params, m.paramTypes, m.returnTypeHint);
                mHeader += "):";
                emitLine(mHeader);
                formatBlock(m.body);
            }
            indent--;
            break;
        }

        case NodeType::EnumDef: {
            emitLine("build " + node->name + " as options:");
            indent++;
            for (size_t i = 0; i < node->enumValues.size(); i++) {
                std::string line = node->enumValues[i];
                if (i < node->enumFields.size() && !node->enumFields[i].empty()) {
                    line += "(";
                    for (size_t j = 0; j < node->enumFields[i].size(); j++) {
                        if (j > 0) line += ", ";
                        line += node->enumFields[i][j];
                    }
                    line += ")";
                }
                emitLine(line);
            }
            indent--;
            break;
        }

        case NodeType::ShapeDef: {
            emitLine("shape " + node->name + ":");
            indent++;
            for (auto& [name, types] : node->shapeMethodSigs) {
                emitLine("to " + name + "()");
            }
            indent--;
            break;
        }

        case NodeType::TryCatchFinally: {
            emitLine("try:");
            formatBlock(node->tryBody);
            if (!node->catchClause.errorName.empty()) {
                emitLine("catch " + node->catchClause.errorName + ":");
                formatBlock(node->catchClause.body);
            }
            if (!node->finallyBody.empty()) {
                emitLine("finally:");
                formatBlock(node->finallyBody);
            }
            break;
        }

        case NodeType::ImportStmt: {
            if (node->importNames.empty()) {
                emitLine("use " + node->modulePath);
            } else {
                std::string names;
                for (size_t i = 0; i < node->importNames.size(); i++) {
                    if (i > 0) names += ", ";
                    names += node->importNames[i];
                }
                std::string path = node->modulePath;
                if (path.size() > 3 && path.substr(path.size()-3) == ".to") {
                    path = "\"" + path + "\"";
                }
                emitLine("use " + names + " from " + path);
            }
            break;
        }

        default:
            emitLine("~ [unformatted node]");
            break;
    }
}

std::string Formatter::formatExpr(ASTNodePtr node) {
    if (!node) return "none";

    switch (node->type) {
        case NodeType::IntegerLiteral:
            return std::to_string(node->intValue);

        case NodeType::FloatLiteral: {
            std::ostringstream oss;
            oss << node->floatValue;
            return oss.str();
        }

        case NodeType::StringLiteral: {
            // Escape special chars
            std::string escaped;
            for (char c : node->stringValue) {
                if (c == '"') escaped += "\\\"";
                else if (c == '\n') escaped += "\\n";
                else if (c == '\t') escaped += "\\t";
                else if (c == '\\') escaped += "\\\\";
                else escaped += c;
            }
            return "\"" + escaped + "\"";
        }

        case NodeType::BoolLiteral:
            return node->boolValue ? "true" : "false";

        case NodeType::NoneLiteral:
            return "none";

        case NodeType::Identifier:
            return node->name;

        case NodeType::BinaryExpr:
            return formatExpr(node->left) + " " + node->op + " " + formatExpr(node->right);

        case NodeType::UnaryExpr:
            if (node->op == "not") return "not " + formatExpr(node->operand);
            return node->op + formatExpr(node->operand);

        case NodeType::CallExpr: {
            std::string result = formatExpr(node->callee) + "(";
            for (size_t i = 0; i < node->arguments.size(); i++) {
                if (i > 0) result += ", ";
                result += formatExpr(node->arguments[i]);
            }
            return result + ")";
        }

        case NodeType::MemberAccess: {
            std::string dot = node->optionalChain ? "?." : ".";
            return formatExpr(node->object) + dot + node->member;
        }

        case NodeType::IndexExpr:
            return formatExpr(node->object) + "[" + formatExpr(node->indexExpr) + "]";

        case NodeType::ListLiteral: {
            std::string result = "[";
            for (size_t i = 0; i < node->elements.size(); i++) {
                if (i > 0) result += ", ";
                result += formatExpr(node->elements[i]);
            }
            return result + "]";
        }

        case NodeType::DictLiteral: {
            if (node->entries.empty()) return "{}";
            std::string result = "{";
            for (size_t i = 0; i < node->entries.size(); i++) {
                if (i > 0) result += ", ";
                result += node->entries[i].key + " = " + formatExpr(node->entries[i].value);
            }
            return result + "}";
        }

        case NodeType::StringInterpolation: {
            std::string result = "\"";
            for (auto& elem : node->elements) {
                if (elem->type == NodeType::StringLiteral) {
                    result += elem->stringValue;
                } else {
                    result += "{" + formatExpr(elem) + "}";
                }
            }
            return result + "\"";
        }

        case NodeType::LambdaExpr: {
            std::string result = "(";
            for (size_t i = 0; i < node->params.size(); i++) {
                if (i > 0) result += ", ";
                result += node->params[i];
            }
            return result + "): " + formatExpr(node->value);
        }

        case NodeType::AsyncExpr:
            return "async " + formatExpr(node->value);

        case NodeType::AwaitExpr:
            return "await " + formatExpr(node->value);

        case NodeType::PipeExpr:
            return formatExpr(node->left) + " then " + formatExpr(node->right);

        default:
            return "???";
    }
}

std::string Formatter::formatParams(const std::vector<std::string>& params,
                                     const std::vector<std::string>& types,
                                     const std::string& returnType) {
    std::string result;
    for (size_t i = 0; i < params.size(); i++) {
        if (i > 0) result += ", ";
        result += params[i];
        if (i < types.size() && !types[i].empty()) {
            result += ": " + types[i];
        }
    }
    if (!returnType.empty()) {
        result += ") -> " + returnType;
        return result; // caller adds the closing paren before this
    }
    return result;
}
