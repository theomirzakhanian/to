#pragma once
#include "ast.h"
#include <string>
#include <sstream>
#include <unordered_set>

class CodeGenerator {
public:
    CodeGenerator(const std::string& filename = "<stdin>");

    // Generate C source code from AST
    std::string generate(ASTNodePtr program);

    // Compile the generated C code to a binary
    bool compile(const std::string& cSource, const std::string& outputPath);

private:
    std::string filename;
    std::ostringstream output;
    int indentLevel = 0;

    void emit(const std::string& code);
    void emitLine(const std::string& code);
    void emitIndent();

    void generateNode(ASTNodePtr node);
    void generateStatement(ASTNodePtr node);
    void generateExpression(ASTNodePtr node);
    void generateBlock(const std::vector<ASTNodePtr>& stmts);
    void generatePrint(ASTNodePtr node);
    void generateIf(ASTNodePtr node);
    void generateWhile(ASTNodePtr node);
    void generateThrough(ASTNodePtr node);
    void generateFunctionDef(ASTNodePtr node);
    void generateAssignment(ASTNodePtr node);

    // Track declared variables
    std::unordered_set<std::string> declaredVars;

    // Helpers
    std::string exprToC(ASTNodePtr node);
    void generateRuntime(); // emit runtime support code
};
