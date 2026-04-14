#pragma once
#include "lexer.h"
#include "ast.h"
#include <string>

class Parser {
public:
    Parser(const std::vector<Token>& tokens, const std::string& filename = "<stdin>");
    ASTNodePtr parse();

private:
    std::vector<Token> tokens;
    std::string filename;
    size_t pos = 0;

    // Token access
    const Token& current() const;
    const Token& peek(int offset = 1) const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token expect(TokenType type, const std::string& message);
    bool atEnd() const;

    // Skip newlines/whitespace tokens
    void skipNewlines();
    void expectNewlineOrEnd();

    // Parsing methods
    ASTNodePtr parseProgram();
    ASTNodePtr parseStatement();
    ASTNodePtr parseAssignmentOrExpr();
    ASTNodePtr parsePrint();
    ASTNodePtr parseReturn();
    ASTNodePtr parseBreak();
    ASTNodePtr parseContinue();
    ASTNodePtr parseIf();
    ASTNodePtr parseGiven();
    ASTNodePtr parseWhile();
    ASTNodePtr parseThrough();
    ASTNodePtr parseFunctionDef();
    ASTNodePtr parseClassDef();
    ASTNodePtr parseTryCatch();
    ASTNodePtr parseImport();
    ASTNodePtr parseConstDecl();
    ASTNodePtr parseAssert();
    ASTNodePtr parseShapeDef();
    ASTNodePtr parseListDestructure();
    ASTNodePtr parseDictDestructure();

    // Block parsing
    std::vector<ASTNodePtr> parseBlock();

    // Expression parsing (precedence climbing)
    ASTNodePtr parseExpression();
    ASTNodePtr parsePipe();
    ASTNodePtr parseOr();
    ASTNodePtr parseAnd();
    ASTNodePtr parseNot();
    ASTNodePtr parseComparison();
    ASTNodePtr parseRange();
    ASTNodePtr parseAddSub();
    ASTNodePtr parseMulDiv();
    ASTNodePtr parseUnary();
    ASTNodePtr parsePostfix();
    ASTNodePtr parsePrimary();

    // Helpers
    ASTNodePtr parseStringWithInterpolation(const std::string& raw, int line);
    std::vector<ASTNodePtr> parseArgList();
};
