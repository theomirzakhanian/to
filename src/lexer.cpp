#include "lexer.h"
#include "error.h"
#include <sstream>
#include <algorithm>

std::unordered_map<std::string, TokenType> Lexer::keywords = {
    {"to", TokenType::TO},
    {"if", TokenType::IF},
    {"or", TokenType::OR},
    {"else", TokenType::ELSE},
    {"given", TokenType::GIVEN},
    {"through", TokenType::THROUGH},
    {"as", TokenType::AS},
    {"while", TokenType::WHILE},
    {"build", TokenType::BUILD},
    {"my", TokenType::MY},
    {"const", TokenType::CONST},
    {"use", TokenType::USE},
    {"from", TokenType::FROM},
    {"try", TokenType::TRY},
    {"catch", TokenType::CATCH},
    {"finally", TokenType::FINALLY},
    {"return", TokenType::RETURN},
    {"print", TokenType::PRINT},
    {"and", TokenType::AND},
    {"not", TokenType::NOT},
    {"true", TokenType::TRUE_LIT},
    {"false", TokenType::FALSE_LIT},
    {"none", TokenType::NONE_LIT},
    {"break", TokenType::BREAK},
    {"continue", TokenType::CONTINUE},
    {"in", TokenType::IN},
    {"async", TokenType::ASYNC},
    {"await", TokenType::AWAIT},
    {"then", TokenType::THEN},
    {"assert", TokenType::ASSERT},
    {"shape", TokenType::SHAPE},
    {"fits", TokenType::FITS},
    {"options", TokenType::OPTIONS},
};

Lexer::Lexer(const std::string& source, const std::string& filename)
    : source(source), filename(filename) {}

char Lexer::current() const {
    if (atEnd()) return '\0';
    return source[pos];
}

char Lexer::peek(int offset) const {
    size_t idx = pos + offset;
    if (idx >= source.size()) return '\0';
    return source[idx];
}

char Lexer::advance() {
    char c = source[pos++];
    if (c == '\n') { line++; column = 1; }
    else { column++; }
    return c;
}

bool Lexer::atEnd() const {
    return pos >= source.size();
}

void Lexer::skipWhitespaceInLine() {
    while (!atEnd() && (current() == ' ' || current() == '\t') ) {
        advance();
    }
}

Token Lexer::makeToken(TokenType type, const std::string& value) {
    return Token(type, value, line, column);
}

Token Lexer::readString() {
    int startLine = line;
    int startCol = column;
    char quoteChar = current(); // either '"' or '\''
    advance(); // skip opening quote
    std::string result;

    int interpDepth = 0; // track {}-nesting for interpolation
    while (!atEnd()) {
        if (current() == quoteChar && interpDepth == 0) break; // end of string

        if (current() == '\\' && interpDepth == 0) {
            advance();
            switch (current()) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                case '\'': result += '\''; break;
                case '{': result += '{'; break;
                case '}': result += '}'; break;
                default: result += '\\'; result += current(); break;
            }
        } else if (current() == '{' && interpDepth == 0) {
            interpDepth++;
            result += current();
        } else if (current() == '}' && interpDepth > 0) {
            interpDepth--;
            result += current();
        } else if (current() == '{' && interpDepth > 0) {
            interpDepth++;
            result += current();
        } else if ((current() == '"' || current() == '\'') && interpDepth > 0) {
            // Nested string inside interpolation — read it as part of the raw content
            char innerQuote = current();
            result += current();
            advance();
            while (!atEnd() && current() != innerQuote) {
                if (current() == '\\') {
                    result += current();
                    advance();
                    if (!atEnd()) { result += current(); }
                } else {
                    result += current();
                }
                advance();
            }
            if (!atEnd()) result += current(); // closing quote
        } else {
            result += current();
        }
        advance();
    }

    if (atEnd()) {
        throw ToError(filename, startLine, startCol, "Unterminated string literal");
    }
    advance(); // skip closing quote
    return Token(TokenType::STRING, result, startLine, startCol);
}

Token Lexer::readNumber() {
    int startCol = column;
    std::string num;
    bool isFloat = false;

    while (!atEnd() && (isdigit(current()) || current() == '.')) {
        if (current() == '.') {
            if (peek() == '.') break; // range operator ..
            if (isFloat) break;
            isFloat = true;
        }
        num += current();
        advance();
    }

    return Token(isFloat ? TokenType::FLOAT : TokenType::INTEGER, num, line, startCol);
}

Token Lexer::readIdentifier() {
    int startCol = column;
    std::string id;

    while (!atEnd() && (isalnum(current()) || current() == '_')) {
        id += current();
        advance();
    }

    auto it = keywords.find(id);
    if (it != keywords.end()) {
        return Token(it->second, id, line, startCol);
    }
    return Token(TokenType::IDENTIFIER, id, line, startCol);
}

void Lexer::skipComment() {
    while (!atEnd() && current() != '\n') {
        advance();
    }
}

void Lexer::skipMultiLineComment() {
    advance(); // skip ~
    advance(); // skip '
    while (!atEnd()) {
        if (current() == '\'' && peek() == '~') {
            advance(); // skip '
            advance(); // skip ~
            return;
        }
        advance();
    }
    throw ToError(filename, line, column, "Unterminated multi-line comment");
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    // Split source into lines for indentation processing
    // But we process character by character with indentation awareness

    bool atLineStart = true;

    while (!atEnd()) {
        // Handle beginning of line — indentation
        if (atLineStart) {
            atLineStart = false;
            int indent = 0;
            while (!atEnd() && current() == ' ') {
                indent++;
                advance();
            }
            // Skip tabs (convert to spaces: 1 tab = 4 spaces)
            while (!atEnd() && current() == '\t') {
                indent += 4;
                advance();
            }

            // Skip blank lines and comment-only lines
            if (atEnd() || current() == '\n') {
                if (!atEnd()) {
                    advance();
                    atLineStart = true;
                }
                continue;
            }
            if (current() == '~') {
                if (peek() == '\'') {
                    skipMultiLineComment();
                    continue;
                } else {
                    skipComment();
                    if (!atEnd()) { advance(); atLineStart = true; }
                    continue;
                }
            }

            // Process indentation changes (skip when inside brackets)
            if (bracketDepth == 0) {
                int currentIndent = indentStack.back();
                if (indent > currentIndent) {
                    indentStack.push_back(indent);
                    tokens.push_back(Token(TokenType::INDENT, "", line, 1));
                } else if (indent < currentIndent) {
                    while (indentStack.size() > 1 && indentStack.back() > indent) {
                        indentStack.pop_back();
                        tokens.push_back(Token(TokenType::DEDENT, "", line, 1));
                    }
                    if (indentStack.back() != indent) {
                        throw ToError(filename, line, 1, "Inconsistent indentation");
                    }
                }
            }
        }

        char c = current();

        // Skip spaces within a line
        if (c == ' ' || c == '\t') {
            skipWhitespaceInLine();
            continue;
        }

        // Newline
        if (c == '\n') {
            if (bracketDepth == 0) {
                tokens.push_back(Token(TokenType::NEWLINE, "\\n", line, column));
            }
            advance();
            atLineStart = true;
            continue;
        }

        // Comments
        if (c == '~') {
            if (peek() == '\'') {
                skipMultiLineComment();
            } else {
                skipComment();
            }
            continue;
        }

        // Strings (double or single quotes)
        if (c == '"' || c == '\'') {
            tokens.push_back(readString());
            continue;
        }

        // Numbers
        if (isdigit(c)) {
            tokens.push_back(readNumber());
            continue;
        }

        // Identifiers and keywords
        if (isalpha(c) || c == '_') {
            tokens.push_back(readIdentifier());
            continue;
        }

        // Operators and delimiters
        int startCol = column;
        switch (c) {
            case '+':
                advance();
                if (!atEnd() && current() == '=') { advance(); tokens.push_back(Token(TokenType::PLUS_EQUAL, "+=", line, startCol)); }
                else { tokens.push_back(Token(TokenType::PLUS, "+", line, startCol)); }
                break;
            case '-':
                advance();
                if (!atEnd() && current() == '=') { advance(); tokens.push_back(Token(TokenType::MINUS_EQUAL, "-=", line, startCol)); }
                else if (!atEnd() && current() == '>') { advance(); tokens.push_back(Token(TokenType::ARROW, "->", line, startCol)); }
                else { tokens.push_back(Token(TokenType::MINUS, "-", line, startCol)); }
                break;
            case '*':
                advance();
                if (!atEnd() && current() == '=') { advance(); tokens.push_back(Token(TokenType::STAR_EQUAL, "*=", line, startCol)); }
                else { tokens.push_back(Token(TokenType::STAR, "*", line, startCol)); }
                break;
            case '/':
                advance();
                if (!atEnd() && current() == '=') { advance(); tokens.push_back(Token(TokenType::SLASH_EQUAL, "/=", line, startCol)); }
                else { tokens.push_back(Token(TokenType::SLASH, "/", line, startCol)); }
                break;
            case '%':
                advance();
                tokens.push_back(Token(TokenType::MODULO, "%", line, startCol));
                break;
            case '=':
                advance();
                if (!atEnd() && current() == '=') { advance(); tokens.push_back(Token(TokenType::EQUAL_EQUAL, "==", line, startCol)); }
                else { tokens.push_back(Token(TokenType::EQUAL, "=", line, startCol)); }
                break;
            case '!':
                advance();
                if (!atEnd() && current() == '=') { advance(); tokens.push_back(Token(TokenType::NOT_EQUAL, "!=", line, startCol)); }
                else { throw ToError(filename, line, startCol, "Unexpected character '!'", "Did you mean '!=' for not-equal? Use 'not' for boolean negation."); }
                break;
            case '<':
                advance();
                if (!atEnd() && current() == '=') { advance(); tokens.push_back(Token(TokenType::LESS_EQUAL, "<=", line, startCol)); }
                else { tokens.push_back(Token(TokenType::LESS, "<", line, startCol)); }
                break;
            case '>':
                advance();
                if (!atEnd() && current() == '=') { advance(); tokens.push_back(Token(TokenType::GREATER_EQUAL, ">=", line, startCol)); }
                else { tokens.push_back(Token(TokenType::GREATER, ">", line, startCol)); }
                break;
            case '.':
                advance();
                if (!atEnd() && current() == '.') {
                    advance();
                    if (!atEnd() && current() == '.') { advance(); tokens.push_back(Token(TokenType::DOT_DOT_DOT, "...", line, startCol)); }
                    else { tokens.push_back(Token(TokenType::DOT_DOT, "..", line, startCol)); }
                }
                else { tokens.push_back(Token(TokenType::DOT, ".", line, startCol)); }
                break;
            case ':':
                advance();
                tokens.push_back(Token(TokenType::COLON, ":", line, startCol));
                break;
            case ',':
                advance();
                tokens.push_back(Token(TokenType::COMMA, ",", line, startCol));
                break;
            case '(':
                advance();
                bracketDepth++;
                tokens.push_back(Token(TokenType::LPAREN, "(", line, startCol));
                break;
            case ')':
                advance();
                if (bracketDepth > 0) bracketDepth--;
                tokens.push_back(Token(TokenType::RPAREN, ")", line, startCol));
                break;
            case '[':
                advance();
                bracketDepth++;
                tokens.push_back(Token(TokenType::LBRACKET, "[", line, startCol));
                break;
            case ']':
                advance();
                if (bracketDepth > 0) bracketDepth--;
                tokens.push_back(Token(TokenType::RBRACKET, "]", line, startCol));
                break;
            case '{':
                advance();
                bracketDepth++;
                tokens.push_back(Token(TokenType::LBRACE, "{", line, startCol));
                break;
            case '}':
                advance();
                if (bracketDepth > 0) bracketDepth--;
                tokens.push_back(Token(TokenType::RBRACE, "}", line, startCol));
                break;
            default:
                throw ToError(filename, line, startCol, std::string("Unexpected character '") + c + "'");
        }
    }

    // Emit remaining DEDENTs
    while (indentStack.size() > 1) {
        indentStack.pop_back();
        tokens.push_back(Token(TokenType::DEDENT, "", line, 1));
    }

    tokens.push_back(Token(TokenType::EOF_TOKEN, "", line, column));
    return tokens;
}

std::string tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::INTEGER: return "INTEGER";
        case TokenType::FLOAT: return "FLOAT";
        case TokenType::STRING: return "STRING";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::TO: return "TO";
        case TokenType::IF: return "IF";
        case TokenType::OR: return "OR";
        case TokenType::ELSE: return "ELSE";
        case TokenType::GIVEN: return "GIVEN";
        case TokenType::THROUGH: return "THROUGH";
        case TokenType::AS: return "AS";
        case TokenType::WHILE: return "WHILE";
        case TokenType::BUILD: return "BUILD";
        case TokenType::MY: return "MY";
        case TokenType::CONST: return "CONST";
        case TokenType::USE: return "USE";
        case TokenType::FROM: return "FROM";
        case TokenType::TRY: return "TRY";
        case TokenType::CATCH: return "CATCH";
        case TokenType::FINALLY: return "FINALLY";
        case TokenType::RETURN: return "RETURN";
        case TokenType::PRINT: return "PRINT";
        case TokenType::AND: return "AND";
        case TokenType::NOT: return "NOT";
        case TokenType::TRUE_LIT: return "TRUE";
        case TokenType::FALSE_LIT: return "FALSE";
        case TokenType::NONE_LIT: return "NONE";
        case TokenType::BREAK: return "BREAK";
        case TokenType::CONTINUE: return "CONTINUE";
        case TokenType::IN: return "IN";
        case TokenType::ASYNC: return "ASYNC";
        case TokenType::AWAIT: return "AWAIT";
        case TokenType::THEN: return "THEN";
        case TokenType::ASSERT: return "ASSERT";
        case TokenType::SHAPE: return "SHAPE";
        case TokenType::FITS: return "FITS";
        case TokenType::OPTIONS: return "OPTIONS";
        case TokenType::DOT_DOT_DOT: return "DOT_DOT_DOT";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::MODULO: return "MODULO";
        case TokenType::EQUAL: return "EQUAL";
        case TokenType::EQUAL_EQUAL: return "EQUAL_EQUAL";
        case TokenType::NOT_EQUAL: return "NOT_EQUAL";
        case TokenType::LESS: return "LESS";
        case TokenType::LESS_EQUAL: return "LESS_EQUAL";
        case TokenType::GREATER: return "GREATER";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        case TokenType::DOT: return "DOT";
        case TokenType::DOT_DOT: return "DOT_DOT";
        case TokenType::PLUS_EQUAL: return "PLUS_EQUAL";
        case TokenType::MINUS_EQUAL: return "MINUS_EQUAL";
        case TokenType::STAR_EQUAL: return "STAR_EQUAL";
        case TokenType::SLASH_EQUAL: return "SLASH_EQUAL";
        case TokenType::ARROW: return "ARROW";
        case TokenType::COLON: return "COLON";
        case TokenType::COMMA: return "COMMA";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::NEWLINE: return "NEWLINE";
        case TokenType::INDENT: return "INDENT";
        case TokenType::DEDENT: return "DEDENT";
        case TokenType::EOF_TOKEN: return "EOF";
    }
    return "UNKNOWN";
}
