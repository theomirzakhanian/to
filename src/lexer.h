#pragma once
#include <string>
#include <vector>
#include <unordered_map>

enum class TokenType {
    // Literals
    INTEGER, FLOAT, STRING,

    // Identifiers & Keywords
    IDENTIFIER,
    TO, IF, OR, ELSE, GIVEN, THROUGH, AS, WHILE,
    BUILD, MY, CONST, USE, FROM, TRY, CATCH, FINALLY,
    RETURN, PRINT, AND, NOT, TRUE_LIT, FALSE_LIT, NONE_LIT,
    BREAK, CONTINUE, IN,
    ASYNC, AWAIT,
    THEN, ASSERT, SHAPE, FITS, OPTIONS,
    DOT_DOT_DOT, // ...
    YIELD,
    QUESTION_DOT, // ?.
    AT, // @

    // Operators
    PLUS, MINUS, STAR, SLASH, MODULO,
    EQUAL, EQUAL_EQUAL, NOT_EQUAL,
    LESS, LESS_EQUAL, GREATER, GREATER_EQUAL,
    DOT, DOT_DOT, // . and ..
    PLUS_EQUAL, MINUS_EQUAL, STAR_EQUAL, SLASH_EQUAL,
    ARROW, // ->

    // Delimiters
    COLON, COMMA,
    LPAREN, RPAREN,
    LBRACKET, RBRACKET,
    LBRACE, RBRACE,

    // Special
    NEWLINE, INDENT, DEDENT, EOF_TOKEN,
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;

    Token(TokenType type, std::string value, int line, int column)
        : type(type), value(std::move(value)), line(line), column(column) {}
};

class Lexer {
public:
    explicit Lexer(const std::string& source, const std::string& filename = "<stdin>");
    std::vector<Token> tokenize();

private:
    std::string source;
    std::string filename;
    size_t pos = 0;
    int line = 1;
    int column = 1;
    std::vector<int> indentStack = {0};
    int bracketDepth = 0; // tracks nesting inside (), [], {}
    static std::unordered_map<std::string, TokenType> keywords;

    char current() const;
    char peek(int offset = 1) const;
    char advance();
    bool atEnd() const;
    void skipWhitespaceInLine();
    Token makeToken(TokenType type, const std::string& value);

    Token readString();
    Token readHeredoc();
    Token readNumber();
    Token readIdentifier();
    void skipComment();
    void skipMultiLineComment();

    std::vector<Token> processIndentation(const std::string& lineText, int lineNum);
};

std::string tokenTypeName(TokenType type);
