#include "parser.h"
#include "error.h"
#include <iostream>

Parser::Parser(const std::vector<Token>& tokens, const std::string& filename)
    : tokens(tokens), filename(filename) {}

const Token& Parser::current() const {
    return tokens[pos];
}

const Token& Parser::peek(int offset) const {
    size_t idx = pos + offset;
    if (idx >= tokens.size()) return tokens.back();
    return tokens[idx];
}

const Token& Parser::advance() {
    const Token& tok = tokens[pos];
    if (pos < tokens.size() - 1) pos++;
    return tok;
}

bool Parser::check(TokenType type) const {
    return current().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}

Token Parser::expect(TokenType type, const std::string& message) {
    if (check(type)) {
        return advance();
    }
    throw ToError(filename, current().line, current().column,
        message + " (got " + tokenTypeName(current().type) + " '" + current().value + "')");
}

bool Parser::atEnd() const {
    return current().type == TokenType::EOF_TOKEN;
}

void Parser::skipNewlines() {
    while (check(TokenType::NEWLINE)) advance();
}

void Parser::expectNewlineOrEnd() {
    if (!atEnd() && !check(TokenType::NEWLINE) && !check(TokenType::DEDENT) && !check(TokenType::EOF_TOKEN)) {
        throw ToError(filename, current().line, current().column,
            "Expected end of line, got " + tokenTypeName(current().type));
    }
    if (check(TokenType::NEWLINE)) advance();
}

ASTNodePtr Parser::parse() {
    return parseProgram();
}

ASTNodePtr Parser::parseProgram() {
    std::vector<ASTNodePtr> stmts;
    skipNewlines();
    while (!atEnd()) {
        stmts.push_back(parseStatement());
        skipNewlines();
    }
    return ASTNode::makeProgram(std::move(stmts));
}

ASTNodePtr Parser::parseStatement() {
    skipNewlines();

    switch (current().type) {
        case TokenType::PRINT: return parsePrint();
        case TokenType::RETURN: return parseReturn();
        case TokenType::BREAK: return parseBreak();
        case TokenType::CONTINUE: return parseContinue();
        case TokenType::IF: return parseIf();
        case TokenType::GIVEN: return parseGiven();
        case TokenType::WHILE: return parseWhile();
        case TokenType::THROUGH: return parseThrough();
        case TokenType::TO: return parseFunctionDef();
        case TokenType::AT: {
            // Collect decorators: @name [newline] @name [newline] ... to funcname
            std::vector<std::string> decorators;
            while (check(TokenType::AT)) {
                advance(); // skip @
                Token name = expect(TokenType::IDENTIFIER, "Expected decorator name after '@'");
                decorators.push_back(name.value);
                skipNewlines();
            }
            if (!check(TokenType::TO)) {
                throw ToError(filename, current().line, current().column,
                    "Decorator must be followed by a function definition");
            }
            auto funcNode = parseFunctionDef();
            funcNode->decorators = decorators;
            return funcNode;
        }
        case TokenType::BUILD: return parseClassDef();
        case TokenType::TRY: return parseTryCatch();
        case TokenType::USE: return parseImport();
        case TokenType::CONST: return parseConstDecl();
        case TokenType::ASSERT: return parseAssert();
        case TokenType::YIELD: {
            int line = current().line;
            advance(); // skip 'yield'
            auto node = std::make_shared<ASTNode>();
            node->type = NodeType::YieldStmt;
            node->line = line;
            if (!check(TokenType::NEWLINE) && !check(TokenType::DEDENT) && !atEnd()) {
                node->value = parseExpression();
            }
            expectNewlineOrEnd();
            return node;
        }
        case TokenType::SHAPE: return parseShapeDef();
        case TokenType::LBRACKET: {
            // Could be destructuring: [a, b, ...rest] = expr
            // or a regular expression starting with list literal
            // Lookahead to check for destructure pattern
            size_t saved = pos;
            bool isDestructure = false;
            advance(); // skip [
            if (check(TokenType::IDENTIFIER) || check(TokenType::DOT_DOT_DOT)) {
                size_t probe = pos;
                while (probe < tokens.size()) {
                    if (tokens[probe].type == TokenType::RBRACKET) {
                        probe++;
                        if (probe < tokens.size() && tokens[probe].type == TokenType::EQUAL) {
                            isDestructure = true;
                        }
                        break;
                    }
                    if (tokens[probe].type != TokenType::IDENTIFIER &&
                        tokens[probe].type != TokenType::COMMA &&
                        tokens[probe].type != TokenType::DOT_DOT_DOT) break;
                    probe++;
                }
            }
            pos = saved;
            if (isDestructure) return parseListDestructure();
            return parseAssignmentOrExpr();
        }
        case TokenType::LBRACE: {
            // Could be dict destructure: {name, age} = expr
            size_t saved = pos;
            bool isDestructure = false;
            advance(); // skip {
            if (check(TokenType::IDENTIFIER)) {
                size_t probe = pos;
                while (probe < tokens.size()) {
                    if (tokens[probe].type == TokenType::RBRACE) {
                        probe++;
                        if (probe < tokens.size() && tokens[probe].type == TokenType::EQUAL) {
                            isDestructure = true;
                        }
                        break;
                    }
                    if (tokens[probe].type != TokenType::IDENTIFIER &&
                        tokens[probe].type != TokenType::COMMA) break;
                    probe++;
                }
            }
            pos = saved;
            if (isDestructure) return parseDictDestructure();
            return parseAssignmentOrExpr();
        }
        default: return parseAssignmentOrExpr();
    }
}

ASTNodePtr Parser::parsePrint() {
    int line = current().line;

    // If followed by '(', treat as function call expression: print(a, b, c)
    if (peek().type == TokenType::LPAREN) {
        // Parse as expression statement: print(...) is a call
        return parseAssignmentOrExpr();
    }

    advance(); // skip 'print'
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::PrintStmt;
    node->line = line;
    node->value = parseExpression();
    expectNewlineOrEnd();
    return node;
}

ASTNodePtr Parser::parseReturn() {
    int line = current().line;
    advance(); // skip 'return'
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::ReturnStmt;
    node->line = line;
    if (!check(TokenType::NEWLINE) && !check(TokenType::DEDENT) && !atEnd()) {
        node->value = parseExpression();
    }
    expectNewlineOrEnd();
    return node;
}

ASTNodePtr Parser::parseBreak() {
    int line = current().line;
    advance();
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::BreakStmt;
    node->line = line;
    expectNewlineOrEnd();
    return node;
}

ASTNodePtr Parser::parseContinue() {
    int line = current().line;
    advance();
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::ContinueStmt;
    node->line = line;
    expectNewlineOrEnd();
    return node;
}

ASTNodePtr Parser::parseIf() {
    int line = current().line;
    advance(); // skip 'if'
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::IfBlock;
    node->line = line;
    node->condition = parseExpression();
    expect(TokenType::COLON, "Expected ':' after if condition");
    expectNewlineOrEnd();
    node->body = parseBlock();

    // Parse 'or' branches (elif)
    skipNewlines();
    while (check(TokenType::OR)) {
        advance(); // skip 'or'
        IfBranch branch;
        branch.condition = parseExpression();
        expect(TokenType::COLON, "Expected ':' after or condition");
        expectNewlineOrEnd();
        branch.body = parseBlock();
        node->orBranches.push_back(std::move(branch));
        skipNewlines();
    }

    // Parse 'else'
    if (check(TokenType::ELSE)) {
        advance();
        expect(TokenType::COLON, "Expected ':' after else");
        expectNewlineOrEnd();
        node->elseBody = parseBlock();
    }

    return node;
}

ASTNodePtr Parser::parseGiven() {
    int line = current().line;
    advance(); // skip 'given'
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::GivenBlock;
    node->line = line;
    node->givenExpr = parseExpression();
    match(TokenType::COLON); // optional colon for consistency
    expectNewlineOrEnd();

    // Expect INDENT
    expect(TokenType::INDENT, "Expected indented block after given");
    skipNewlines();

    while (!check(TokenType::DEDENT) && !atEnd()) {
        skipNewlines();
        if (check(TokenType::DEDENT)) break;

        GivenBranch branch;
        if (check(TokenType::IF)) {
            advance(); // skip 'if'

            // Check for dict pattern: if {key: value, bind}:
            if (check(TokenType::LBRACE)) {
                size_t saved = pos;
                advance(); // skip {
                bool isPattern = false;
                // A pattern has identifiers, optionally followed by ':' (with a value) or ','
                if (check(TokenType::IDENTIFIER) || check(TokenType::RBRACE)) {
                    size_t probe = pos;
                    int depth = 1;
                    while (probe < tokens.size() && depth > 0) {
                        if (tokens[probe].type == TokenType::LBRACE) depth++;
                        else if (tokens[probe].type == TokenType::RBRACE) depth--;
                        if (depth == 0) break;
                        probe++;
                    }
                    if (depth == 0 && probe + 1 < tokens.size() &&
                        tokens[probe + 1].type == TokenType::COLON) {
                        // Check if it's followed by a newline (pattern branch) vs `=` (dict literal assignment)
                        // Actually, in given branch, after the pattern we expect ':' then newline
                        // A dict LITERAL as condition ends with '==' or similar — so { ... } : is a pattern
                        isPattern = true;
                    }
                }
                pos = saved;
                if (isPattern) {
                    advance(); // skip {
                    branch.patternKind = 1;
                    while (!check(TokenType::RBRACE)) {
                        Token key = expect(TokenType::IDENTIFIER, "Expected key in pattern");
                        branch.patternKeys.push_back(key.value);
                        // Optional value: key: literal
                        if (match(TokenType::COLON)) {
                            branch.patternValues.push_back(parseExpression());
                            branch.patternBindings.push_back(""); // no binding, just match
                        } else {
                            branch.patternValues.push_back(nullptr); // just bind
                            branch.patternBindings.push_back(key.value);
                        }
                        if (!check(TokenType::RBRACE)) {
                            expect(TokenType::COMMA, "Expected ',' in pattern");
                        }
                    }
                    expect(TokenType::RBRACE, "Expected '}'");
                } else {
                    branch.condition = parseExpression();
                }
            } else if (check(TokenType::LBRACKET)) {
                // List pattern: if [a, b, ...rest]:
                size_t saved = pos;
                advance(); // skip [
                bool isPattern = false;
                if (check(TokenType::IDENTIFIER) || check(TokenType::DOT_DOT_DOT) || check(TokenType::RBRACKET)) {
                    size_t probe = pos;
                    int depth = 1;
                    bool onlyIdentsAndCommas = true;
                    while (probe < tokens.size() && depth > 0) {
                        auto t = tokens[probe].type;
                        if (t == TokenType::LBRACKET) depth++;
                        else if (t == TokenType::RBRACKET) { depth--; if (depth == 0) break; }
                        else if (t != TokenType::IDENTIFIER && t != TokenType::COMMA && t != TokenType::DOT_DOT_DOT) {
                            onlyIdentsAndCommas = false;
                            break;
                        }
                        probe++;
                    }
                    if (depth == 0 && onlyIdentsAndCommas &&
                        probe + 1 < tokens.size() && tokens[probe + 1].type == TokenType::COLON) {
                        isPattern = true;
                    }
                }
                pos = saved;
                if (isPattern) {
                    advance(); // skip [
                    branch.patternKind = 2;
                    while (!check(TokenType::RBRACKET)) {
                        if (match(TokenType::DOT_DOT_DOT)) {
                            Token n = expect(TokenType::IDENTIFIER, "Expected name after '...'");
                            branch.patternRestName = n.value;
                        } else {
                            Token n = expect(TokenType::IDENTIFIER, "Expected binding name");
                            branch.patternBindings.push_back(n.value);
                        }
                        if (!check(TokenType::RBRACKET)) {
                            expect(TokenType::COMMA, "Expected ','");
                        }
                    }
                    expect(TokenType::RBRACKET, "Expected ']'");
                } else {
                    branch.condition = parseExpression();
                }
            } else if (check(TokenType::IDENTIFIER)) {
                // Look for sum type pattern: TagName(a, b):
                size_t saved = pos;
                Token ident = advance(); // consume identifier
                if (check(TokenType::LPAREN)) {
                    size_t parenSaved = pos;
                    advance(); // skip (
                    bool isPattern = false;
                    if (check(TokenType::RPAREN) || check(TokenType::IDENTIFIER)) {
                        size_t probe = pos;
                        // Check that contents are identifiers separated by commas
                        bool valid = true;
                        if (tokens[probe].type == TokenType::IDENTIFIER) {
                            probe++;
                            while (probe < tokens.size() && tokens[probe].type == TokenType::COMMA) {
                                probe++;
                                if (probe < tokens.size() && tokens[probe].type == TokenType::IDENTIFIER) probe++;
                                else { valid = false; break; }
                            }
                        }
                        if (valid && probe < tokens.size() && tokens[probe].type == TokenType::RPAREN) {
                            probe++;
                            if (probe < tokens.size() && tokens[probe].type == TokenType::COLON) {
                                isPattern = true;
                            }
                        }
                    }
                    if (isPattern) {
                        // Parse sum type pattern
                        branch.patternKind = 3;
                        branch.patternTag = ident.value;
                        if (!check(TokenType::RPAREN)) {
                            Token f = expect(TokenType::IDENTIFIER, "Expected binding name");
                            branch.patternBindings.push_back(f.value);
                            while (match(TokenType::COMMA)) {
                                f = expect(TokenType::IDENTIFIER, "Expected binding name");
                                branch.patternBindings.push_back(f.value);
                            }
                        }
                        expect(TokenType::RPAREN, "Expected ')'");
                    } else {
                        pos = saved;
                        branch.condition = parseExpression();
                    }
                } else {
                    pos = saved;
                    branch.condition = parseExpression();
                }
            } else {
                branch.condition = parseExpression();
            }

            expect(TokenType::COLON, "Expected ':' after given branch condition");
            expectNewlineOrEnd();
            branch.body = parseBlock();
        } else if (check(TokenType::ELSE)) {
            advance();
            expect(TokenType::COLON, "Expected ':' after else");
            expectNewlineOrEnd();
            branch.body = parseBlock();
            branch.condition = nullptr; // else branch
        } else {
            throw ToError(filename, current().line, current().column,
                "Expected 'if' or 'else' in given block");
        }
        node->givenBranches.push_back(std::move(branch));
        skipNewlines();
    }

    if (check(TokenType::DEDENT)) advance();
    return node;
}

ASTNodePtr Parser::parseWhile() {
    int line = current().line;
    advance(); // skip 'while'
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::WhileLoop;
    node->line = line;
    node->condition = parseExpression();
    expect(TokenType::COLON, "Expected ':' after while condition");
    expectNewlineOrEnd();
    node->body = parseBlock();
    return node;
}

ASTNodePtr Parser::parseThrough() {
    int line = current().line;
    advance(); // skip 'through'
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::ThroughLoop;
    node->line = line;
    node->iterable = parseExpression();
    expect(TokenType::AS, "Expected 'as' after iterable in through loop");
    Token varTok = expect(TokenType::IDENTIFIER, "Expected loop variable name after 'as'");
    node->loopVar = varTok.value;
    expect(TokenType::COLON, "Expected ':' after through loop header");
    expectNewlineOrEnd();
    node->body = parseBlock();
    return node;
}

ASTNodePtr Parser::parseFunctionDef() {
    int line = current().line;
    advance(); // skip 'to'
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::FunctionDef;
    node->line = line;
    Token nameTok = expect(TokenType::IDENTIFIER, "Expected function name after 'to'");
    node->name = nameTok.value;
    expect(TokenType::LPAREN, "Expected '(' after function name");

    // Parse parameters with optional type hints: name or name: type
    if (!check(TokenType::RPAREN)) {
        Token p = expect(TokenType::IDENTIFIER, "Expected parameter name");
        node->params.push_back(p.value);
        if (match(TokenType::COLON)) {
            Token t = expect(TokenType::IDENTIFIER, "Expected type name after ':'");
            node->paramTypes.push_back(t.value);
        } else {
            node->paramTypes.push_back("");
        }
        while (match(TokenType::COMMA)) {
            p = expect(TokenType::IDENTIFIER, "Expected parameter name");
            node->params.push_back(p.value);
            if (match(TokenType::COLON)) {
                Token t = expect(TokenType::IDENTIFIER, "Expected type name after ':'");
                node->paramTypes.push_back(t.value);
            } else {
                node->paramTypes.push_back("");
            }
        }
    }
    expect(TokenType::RPAREN, "Expected ')' after parameters");
    // Optional return type: -> type
    if (match(TokenType::ARROW)) {
        Token rt = expect(TokenType::IDENTIFIER, "Expected return type after '->'");
        node->returnTypeHint = rt.value;
    }
    expect(TokenType::COLON, "Expected ':' after function definition");
    expectNewlineOrEnd();
    node->body = parseBlock();
    return node;
}

ASTNodePtr Parser::parseClassDef() {
    int line = current().line;
    advance(); // skip 'build'
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::ClassDef;
    node->line = line;
    Token nameTok = expect(TokenType::IDENTIFIER, "Expected class name after 'build'");
    node->name = nameTok.value;

    // Check for enum: build Color as options:
    if (match(TokenType::AS)) {
        expect(TokenType::OPTIONS, "Expected 'options' after 'as'");
        node->type = NodeType::EnumDef;
        expect(TokenType::COLON, "Expected ':' after 'options'");
        expectNewlineOrEnd();
        expect(TokenType::INDENT, "Expected indented block for enum values");
        skipNewlines();
        while (!check(TokenType::DEDENT) && !atEnd()) {
            skipNewlines();
            if (check(TokenType::DEDENT)) break;
            Token val = expect(TokenType::IDENTIFIER, "Expected enum value name");
            node->enumValues.push_back(val.value);
            // Optional fields: Ok(value), Err(message, code)
            std::vector<std::string> fields;
            if (match(TokenType::LPAREN)) {
                if (!check(TokenType::RPAREN)) {
                    Token f = expect(TokenType::IDENTIFIER, "Expected field name");
                    fields.push_back(f.value);
                    while (match(TokenType::COMMA)) {
                        f = expect(TokenType::IDENTIFIER, "Expected field name");
                        fields.push_back(f.value);
                    }
                }
                expect(TokenType::RPAREN, "Expected ')'");
            }
            node->enumFields.push_back(fields);
            expectNewlineOrEnd();
            skipNewlines();
        }
        if (check(TokenType::DEDENT)) advance();
        return node;
    }

    // Optional inheritance: build Dog from Animal:
    if (match(TokenType::FROM)) {
        Token parentTok = expect(TokenType::IDENTIFIER, "Expected parent class name after 'from'");
        node->parentClass = parentTok.value;
    }
    // Optional trait: build Point fits Printable:
    if (match(TokenType::FITS)) {
        Token shapeTok = expect(TokenType::IDENTIFIER, "Expected shape name after 'fits'");
        node->fitsShape = shapeTok.value;
    }
    expect(TokenType::COLON, "Expected ':' after class name");
    expectNewlineOrEnd();

    // Parse class body — expect methods
    expect(TokenType::INDENT, "Expected indented block for class body");
    skipNewlines();

    while (!check(TokenType::DEDENT) && !atEnd()) {
        skipNewlines();
        if (check(TokenType::DEDENT)) break;

        expect(TokenType::TO, "Expected method definition ('to') in class body");
        Token methodName = expect(TokenType::IDENTIFIER, "Expected method name");
        expect(TokenType::LPAREN, "Expected '(' after method name");

        MethodDef method;
        method.name = methodName.value;
        method.line = methodName.line;

        if (!check(TokenType::RPAREN)) {
            Token p = expect(TokenType::IDENTIFIER, "Expected parameter name");
            method.params.push_back(p.value);
            if (match(TokenType::COLON)) {
                Token t = expect(TokenType::IDENTIFIER, "Expected type name");
                method.paramTypes.push_back(t.value);
            } else {
                method.paramTypes.push_back("");
            }
            while (match(TokenType::COMMA)) {
                p = expect(TokenType::IDENTIFIER, "Expected parameter name");
                method.params.push_back(p.value);
                if (match(TokenType::COLON)) {
                    Token t = expect(TokenType::IDENTIFIER, "Expected type name");
                    method.paramTypes.push_back(t.value);
                } else {
                    method.paramTypes.push_back("");
                }
            }
        }
        expect(TokenType::RPAREN, "Expected ')' after method parameters");
        if (match(TokenType::ARROW)) {
            Token rt = expect(TokenType::IDENTIFIER, "Expected return type after '->'");
            method.returnTypeHint = rt.value;
        }
        expect(TokenType::COLON, "Expected ':' after method definition");
        expectNewlineOrEnd();
        method.body = parseBlock();

        node->methods.push_back(std::move(method));
        skipNewlines();
    }

    if (check(TokenType::DEDENT)) advance();
    return node;
}

ASTNodePtr Parser::parseTryCatch() {
    int line = current().line;
    advance(); // skip 'try'
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::TryCatchFinally;
    node->line = line;
    expect(TokenType::COLON, "Expected ':' after try");
    expectNewlineOrEnd();
    node->tryBody = parseBlock();

    skipNewlines();
    if (check(TokenType::CATCH)) {
        advance();
        Token errName = expect(TokenType::IDENTIFIER, "Expected error variable name after 'catch'");
        node->catchClause.errorName = errName.value;
        expect(TokenType::COLON, "Expected ':' after catch");
        expectNewlineOrEnd();
        node->catchClause.body = parseBlock();
    }

    skipNewlines();
    if (check(TokenType::FINALLY)) {
        advance();
        expect(TokenType::COLON, "Expected ':' after finally");
        expectNewlineOrEnd();
        node->finallyBody = parseBlock();
    }

    return node;
}

ASTNodePtr Parser::parseImport() {
    int line = current().line;
    advance(); // skip 'use'
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::ImportStmt;
    node->line = line;

    // use math
    // use sin, cos from math
    // use greet from "helpers.to"

    Token first = expect(TokenType::IDENTIFIER, "Expected module or symbol name after 'use'");
    std::vector<std::string> names = {first.value};

    while (match(TokenType::COMMA)) {
        Token n = expect(TokenType::IDENTIFIER, "Expected symbol name");
        names.push_back(n.value);
    }

    if (match(TokenType::FROM)) {
        // Importing specific names from a module
        node->importNames = names;
        if (check(TokenType::STRING)) {
            node->modulePath = current().value;
            advance();
        } else {
            Token mod = expect(TokenType::IDENTIFIER, "Expected module name or path after 'from'");
            node->modulePath = mod.value;
        }
    } else {
        // Importing a whole module
        node->modulePath = first.value;
    }

    expectNewlineOrEnd();
    return node;
}

ASTNodePtr Parser::parseConstDecl() {
    int line = current().line;
    advance(); // skip 'const'
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::ConstDecl;
    node->line = line;
    Token nameTok = expect(TokenType::IDENTIFIER, "Expected variable name after 'const'");
    node->name = nameTok.value;
    expect(TokenType::EQUAL, "Expected '=' in const declaration");
    node->value = parseExpression();
    expectNewlineOrEnd();
    return node;
}

ASTNodePtr Parser::parseAssert() {
    int line = current().line;
    advance(); // skip 'assert'
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::AssertStmt;
    node->line = line;
    node->value = parseExpression();
    expectNewlineOrEnd();
    return node;
}

ASTNodePtr Parser::parseShapeDef() {
    int line = current().line;
    advance(); // skip 'shape'
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::ShapeDef;
    node->line = line;
    Token nameTok = expect(TokenType::IDENTIFIER, "Expected shape name");
    node->name = nameTok.value;
    expect(TokenType::COLON, "Expected ':' after shape name");
    expectNewlineOrEnd();

    // Parse method signatures
    expect(TokenType::INDENT, "Expected indented block for shape body");
    skipNewlines();
    while (!check(TokenType::DEDENT) && !atEnd()) {
        skipNewlines();
        if (check(TokenType::DEDENT)) break;
        expect(TokenType::TO, "Expected method signature in shape");
        Token methodName = expect(TokenType::IDENTIFIER, "Expected method name");
        expect(TokenType::LPAREN, "Expected '('");
        std::vector<std::string> paramTypes;
        if (!check(TokenType::RPAREN)) {
            // Parse param names (optionally with types)
            expect(TokenType::IDENTIFIER, "Expected param");
            if (match(TokenType::COLON)) {
                Token t = expect(TokenType::IDENTIFIER, "Expected type");
                paramTypes.push_back(t.value);
            }
            while (match(TokenType::COMMA)) {
                expect(TokenType::IDENTIFIER, "Expected param");
                if (match(TokenType::COLON)) {
                    Token t = expect(TokenType::IDENTIFIER, "Expected type");
                    paramTypes.push_back(t.value);
                }
            }
        }
        expect(TokenType::RPAREN, "Expected ')'");
        // Optional return type
        std::string retType;
        if (match(TokenType::ARROW)) {
            Token rt = expect(TokenType::IDENTIFIER, "Expected return type");
            retType = rt.value;
        }
        node->shapeMethodSigs.push_back({methodName.value, paramTypes});
        expectNewlineOrEnd();
        skipNewlines();
    }
    if (check(TokenType::DEDENT)) advance();
    return node;
}

ASTNodePtr Parser::parseListDestructure() {
    int line = current().line;
    advance(); // skip [
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::DestructureList;
    node->line = line;

    while (!check(TokenType::RBRACKET)) {
        if (match(TokenType::DOT_DOT_DOT)) {
            Token rest = expect(TokenType::IDENTIFIER, "Expected variable name after '...'");
            node->restName = rest.value;
        } else {
            Token name = expect(TokenType::IDENTIFIER, "Expected variable name in destructure");
            node->destructNames.push_back(name.value);
        }
        if (!check(TokenType::RBRACKET)) {
            expect(TokenType::COMMA, "Expected ',' in destructure");
        }
    }
    expect(TokenType::RBRACKET, "Expected ']'");
    expect(TokenType::EQUAL, "Expected '=' after destructure pattern");
    node->value = parseExpression();
    expectNewlineOrEnd();
    return node;
}

ASTNodePtr Parser::parseDictDestructure() {
    int line = current().line;
    advance(); // skip {
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::DestructureDict;
    node->line = line;

    while (!check(TokenType::RBRACE)) {
        Token name = expect(TokenType::IDENTIFIER, "Expected key name in destructure");
        node->destructNames.push_back(name.value);
        if (!check(TokenType::RBRACE)) {
            expect(TokenType::COMMA, "Expected ',' in destructure");
        }
    }
    expect(TokenType::RBRACE, "Expected '}'");
    expect(TokenType::EQUAL, "Expected '=' after destructure pattern");
    node->value = parseExpression();
    expectNewlineOrEnd();
    return node;
}

std::vector<ASTNodePtr> Parser::parseBlock() {
    std::vector<ASTNodePtr> stmts;
    expect(TokenType::INDENT, "Expected indented block");
    skipNewlines();

    while (!check(TokenType::DEDENT) && !atEnd()) {
        stmts.push_back(parseStatement());
        skipNewlines();
    }

    if (check(TokenType::DEDENT)) advance();
    return stmts;
}

ASTNodePtr Parser::parseAssignmentOrExpr() {
    ASTNodePtr expr = parseExpression();

    // Check for assignment operators
    if (check(TokenType::EQUAL) || check(TokenType::PLUS_EQUAL) ||
        check(TokenType::MINUS_EQUAL) || check(TokenType::STAR_EQUAL) ||
        check(TokenType::SLASH_EQUAL)) {

        std::string op = current().value;
        advance();
        ASTNodePtr val = parseExpression();

        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::Assignment;
        node->line = expr->line;
        node->target = expr;
        node->value = val;
        node->assignOp = op;
        expectNewlineOrEnd();
        return node;
    }

    // It's an expression statement
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::ExpressionStmt;
    node->line = expr->line;
    node->value = expr;
    expectNewlineOrEnd();
    return node;
}

// Expression parsing with precedence

ASTNodePtr Parser::parseExpression() {
    return parsePipe();
}

ASTNodePtr Parser::parsePipe() {
    auto left = parseOr();
    while (check(TokenType::THEN)) {
        int line = current().line;
        advance();
        // Parse the call expression: then func(args) — left becomes first arg
        auto right = parsePostfix();
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::PipeExpr;
        node->left = left;
        node->right = right; // the function/call to pipe into
        node->line = line;
        left = node;
    }
    return left;
}

ASTNodePtr Parser::parseOr() {
    auto left = parseAnd();
    while (check(TokenType::OR)) {
        int line = current().line;
        advance();
        auto right = parseAnd();
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::BinaryExpr;
        node->op = "or";
        node->left = left;
        node->right = right;
        node->line = line;
        left = node;
    }
    return left;
}

ASTNodePtr Parser::parseAnd() {
    auto left = parseNot();
    while (check(TokenType::AND)) {
        int line = current().line;
        advance();
        auto right = parseNot();
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::BinaryExpr;
        node->op = "and";
        node->left = left;
        node->right = right;
        node->line = line;
        left = node;
    }
    return left;
}

ASTNodePtr Parser::parseNot() {
    if (check(TokenType::NOT)) {
        int line = current().line;
        advance();
        auto operand = parseNot();
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::UnaryExpr;
        node->op = "not";
        node->operand = operand;
        node->line = line;
        return node;
    }
    return parseComparison();
}

ASTNodePtr Parser::parseComparison() {
    auto left = parseRange();
    while (check(TokenType::EQUAL_EQUAL) || check(TokenType::NOT_EQUAL) ||
           check(TokenType::LESS) || check(TokenType::LESS_EQUAL) ||
           check(TokenType::GREATER) || check(TokenType::GREATER_EQUAL)) {
        int line = current().line;
        std::string op = current().value;
        advance();
        auto right = parseAddSub();
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::BinaryExpr;
        node->op = op;
        node->left = left;
        node->right = right;
        node->line = line;
        left = node;
    }
    return left;
}

ASTNodePtr Parser::parseRange() {
    auto left = parseAddSub();
    if (check(TokenType::DOT_DOT)) {
        int line = current().line;
        advance();
        auto right = parseAddSub();
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::BinaryExpr;
        node->op = "..";
        node->left = left;
        node->right = right;
        node->line = line;
        return node;
    }
    return left;
}

ASTNodePtr Parser::parseAddSub() {
    auto left = parseMulDiv();
    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        int line = current().line;
        std::string op = current().value;
        advance();
        auto right = parseMulDiv();
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::BinaryExpr;
        node->op = op;
        node->left = left;
        node->right = right;
        node->line = line;
        left = node;
    }
    return left;
}

ASTNodePtr Parser::parseMulDiv() {
    auto left = parseUnary();
    while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::MODULO)) {
        int line = current().line;
        std::string op = current().value;
        advance();
        auto right = parseUnary();
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::BinaryExpr;
        node->op = op;
        node->left = left;
        node->right = right;
        node->line = line;
        left = node;
    }
    return left;
}

ASTNodePtr Parser::parseUnary() {
    if (check(TokenType::MINUS)) {
        int line = current().line;
        advance();
        auto operand = parseUnary();
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::UnaryExpr;
        node->op = "-";
        node->operand = operand;
        node->line = line;
        return node;
    }
    return parsePostfix();
}

ASTNodePtr Parser::parsePostfix() {
    auto expr = parsePrimary();

    while (true) {
        if (check(TokenType::DOT) || check(TokenType::QUESTION_DOT)) {
            bool isOptional = check(TokenType::QUESTION_DOT);
            int line = current().line;
            advance();
            Token memberTok = expect(TokenType::IDENTIFIER, "Expected member name after '.'");

            // Check if this is a method call: obj.method(args)
            if (check(TokenType::LPAREN)) {
                advance(); // skip '('
                auto node = std::make_shared<ASTNode>();
                node->type = NodeType::CallExpr;
                node->line = line;

                // Create member access as callee
                auto memberAccess = std::make_shared<ASTNode>();
                memberAccess->type = NodeType::MemberAccess;
                memberAccess->object = expr;
                memberAccess->member = memberTok.value;
                memberAccess->optionalChain = isOptional;
                memberAccess->line = line;
                node->callee = memberAccess;

                // Parse arguments
                if (!check(TokenType::RPAREN)) {
                    node->arguments.push_back(parseExpression());
                    while (match(TokenType::COMMA)) {
                        node->arguments.push_back(parseExpression());
                    }
                }
                expect(TokenType::RPAREN, "Expected ')' after arguments");
                expr = node;
            } else {
                auto node = std::make_shared<ASTNode>();
                node->type = NodeType::MemberAccess;
                node->object = expr;
                node->member = memberTok.value;
                node->optionalChain = isOptional;
                node->line = line;
                expr = node;
            }
        } else if (check(TokenType::LBRACKET)) {
            int line = current().line;
            advance(); // skip '['
            auto index = parseExpression();
            expect(TokenType::RBRACKET, "Expected ']' after index");
            auto node = std::make_shared<ASTNode>();
            node->type = NodeType::IndexExpr;
            node->object = expr;
            node->indexExpr = index;
            node->line = line;
            expr = node;
        } else if (check(TokenType::LPAREN)) {
            int line = current().line;
            advance(); // skip '('
            auto node = std::make_shared<ASTNode>();
            node->type = NodeType::CallExpr;
            node->callee = expr;
            node->line = line;
            if (!check(TokenType::RPAREN)) {
                node->arguments.push_back(parseExpression());
                while (match(TokenType::COMMA)) {
                    node->arguments.push_back(parseExpression());
                }
            }
            expect(TokenType::RPAREN, "Expected ')' after arguments");
            expr = node;
        } else {
            break;
        }
    }

    return expr;
}

ASTNodePtr Parser::parsePrimary() {
    // async expression: async func(args)
    if (check(TokenType::ASYNC)) {
        int line = current().line;
        advance();
        auto expr = parsePostfix(); // parse the call expression
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::AsyncExpr;
        node->value = expr;
        node->line = line;
        return node;
    }

    // await expression: await future
    if (check(TokenType::AWAIT)) {
        int line = current().line;
        advance();
        auto expr = parsePostfix();
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::AwaitExpr;
        node->value = expr;
        node->line = line;
        return node;
    }

    // Integer
    if (check(TokenType::INTEGER)) {
        auto tok = advance();
        return ASTNode::makeInt(std::stoll(tok.value), tok.line);
    }

    // Float
    if (check(TokenType::FLOAT)) {
        auto tok = advance();
        return ASTNode::makeFloat(std::stod(tok.value), tok.line);
    }

    // String (with potential interpolation)
    if (check(TokenType::STRING)) {
        auto tok = advance();
        // Check for interpolation
        if (tok.value.find('{') != std::string::npos) {
            return parseStringWithInterpolation(tok.value, tok.line);
        }
        return ASTNode::makeString(tok.value, tok.line);
    }

    // Boolean
    if (check(TokenType::TRUE_LIT)) {
        auto tok = advance();
        return ASTNode::makeBool(true, tok.line);
    }
    if (check(TokenType::FALSE_LIT)) {
        auto tok = advance();
        return ASTNode::makeBool(false, tok.line);
    }

    // None
    if (check(TokenType::NONE_LIT)) {
        auto tok = advance();
        return ASTNode::makeNone(tok.line);
    }

    // my or my.something
    if (check(TokenType::MY)) {
        int line = current().line;
        advance();
        if (check(TokenType::DOT)) {
            advance();
            Token memberTok = expect(TokenType::IDENTIFIER, "Expected member name after 'my.'");
            auto node = std::make_shared<ASTNode>();
            node->type = NodeType::MemberAccess;
            node->object = ASTNode::makeIdentifier("my", line);
            node->member = memberTok.value;
            node->line = line;
            return node;
        }
        // bare 'my' — returns the instance itself
        return ASTNode::makeIdentifier("my", line);
    }

    // Identifier (including 'print' as a value in expression context)
    if (check(TokenType::IDENTIFIER) || check(TokenType::PRINT)) {
        auto tok = advance();
        return ASTNode::makeIdentifier(tok.value, tok.line);
    }

    // List literal
    if (check(TokenType::LBRACKET)) {
        int line = current().line;
        advance();
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::ListLiteral;
        node->line = line;
        skipNewlines();
        if (!check(TokenType::RBRACKET)) {
            node->elements.push_back(parseExpression());
            while (match(TokenType::COMMA)) {
                skipNewlines();
                if (check(TokenType::RBRACKET)) break;
                node->elements.push_back(parseExpression());
            }
        }
        skipNewlines();
        expect(TokenType::RBRACKET, "Expected ']' to close list");
        return node;
    }

    // Dictionary literal
    if (check(TokenType::LBRACE)) {
        int line = current().line;
        advance();
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::DictLiteral;
        node->line = line;
        skipNewlines();
        if (!check(TokenType::RBRACE)) {
            Token key = expect(TokenType::IDENTIFIER, "Expected key name in dictionary");
            expect(TokenType::EQUAL, "Expected '=' after dictionary key");
            ASTNodePtr val = parseExpression();
            node->entries.push_back({key.value, val});
            while (match(TokenType::COMMA)) {
                skipNewlines();
                if (check(TokenType::RBRACE)) break;
                key = expect(TokenType::IDENTIFIER, "Expected key name in dictionary");
                expect(TokenType::EQUAL, "Expected '=' after dictionary key");
                val = parseExpression();
                node->entries.push_back({key.value, val});
            }
        }
        skipNewlines();
        expect(TokenType::RBRACE, "Expected '}' to close dictionary");
        return node;
    }

    // Parenthesized expression or lambda: (x, y): x + y
    if (check(TokenType::LPAREN)) {
        // Lookahead to detect lambda: (IDENT, ...) :
        size_t saved = pos;
        bool isLambda = false;
        advance(); // skip (

        // Empty params lambda: (): expr
        if (check(TokenType::RPAREN)) {
            size_t saved2 = pos;
            advance();
            if (check(TokenType::COLON)) {
                isLambda = true;
            }
            pos = saved2;
        }
        // Check if all contents are IDENT separated by COMMA, ending with ) :
        if (!isLambda && (check(TokenType::IDENTIFIER) || check(TokenType::RPAREN))) {
            size_t probe = pos;
            bool valid = true;
            if (tokens[probe].type == TokenType::IDENTIFIER) {
                probe++;
                while (probe < tokens.size() && tokens[probe].type == TokenType::COMMA) {
                    probe++;
                    if (probe < tokens.size() && tokens[probe].type == TokenType::IDENTIFIER) probe++;
                    else { valid = false; break; }
                }
            }
            if (valid && probe < tokens.size() && tokens[probe].type == TokenType::RPAREN) {
                probe++;
                if (probe < tokens.size() && tokens[probe].type == TokenType::COLON) {
                    isLambda = true;
                }
            }
        }
        pos = saved; // restore
        advance(); // skip ( again

        if (isLambda) {
            int line = current().line;
            auto node = std::make_shared<ASTNode>();
            node->type = NodeType::LambdaExpr;
            node->line = line;
            // Parse params
            if (!check(TokenType::RPAREN)) {
                Token p = expect(TokenType::IDENTIFIER, "Expected parameter name");
                node->params.push_back(p.value);
                while (match(TokenType::COMMA)) {
                    p = expect(TokenType::IDENTIFIER, "Expected parameter name");
                    node->params.push_back(p.value);
                }
            }
            expect(TokenType::RPAREN, "Expected ')' after lambda parameters");
            expect(TokenType::COLON, "Expected ':' after lambda parameters");
            // Lambda body is a single expression
            node->value = parseExpression();
            return node;
        }

        auto expr = parseExpression();
        expect(TokenType::RPAREN, "Expected ')'");
        return expr;
    }

    throw ToError(filename, current().line, current().column,
        "Unexpected token: " + tokenTypeName(current().type) + " '" + current().value + "'");
}

ASTNodePtr Parser::parseStringWithInterpolation(const std::string& raw, int line) {
    // Parse string interpolation: "Hello {name}, you are {age} years old"
    // Split into parts: string and expression alternating
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::StringInterpolation;
    node->line = line;

    std::string current_str;
    size_t i = 0;
    while (i < raw.size()) {
        if (raw[i] == '{') {
            // Save current string part
            if (!current_str.empty()) {
                node->elements.push_back(ASTNode::makeString(current_str, line));
                current_str.clear();
            }
            // Find matching }
            i++;
            std::string expr_str;
            int depth = 1;
            while (i < raw.size() && depth > 0) {
                if (raw[i] == '{') depth++;
                else if (raw[i] == '}') { depth--; if (depth == 0) break; }
                expr_str += raw[i];
                i++;
            }
            i++; // skip closing }

            // Parse the expression string
            Lexer exprLexer(expr_str + "\n", filename);
            auto exprTokens = exprLexer.tokenize();
            Parser exprParser(exprTokens, filename);
            auto exprNode = exprParser.parseExpression();
            node->elements.push_back(exprNode);
        } else {
            current_str += raw[i];
            i++;
        }
    }
    if (!current_str.empty()) {
        node->elements.push_back(ASTNode::makeString(current_str, line));
    }

    // If only one element and it's a string, just return that
    if (node->elements.size() == 1 && node->elements[0]->type == NodeType::StringLiteral) {
        return node->elements[0];
    }

    return node;
}
