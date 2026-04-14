#pragma once
#include "ast.h"
#include <string>
#include <sstream>

class Formatter {
public:
    Formatter(int indentSize = 2) : indentSize(indentSize) {}

    std::string format(ASTNodePtr program);

private:
    int indentSize;
    int indent = 0;
    std::ostringstream out;

    void emitLine(const std::string& line);
    void emitBlank();

    void formatNode(ASTNodePtr node);
    void formatBlock(const std::vector<ASTNodePtr>& stmts);
    std::string formatExpr(ASTNodePtr node);
    std::string formatParams(const std::vector<std::string>& params,
                              const std::vector<std::string>& types,
                              const std::string& returnType);
};
