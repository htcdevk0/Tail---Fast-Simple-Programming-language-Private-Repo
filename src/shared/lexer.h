#pragma once
#include <string>
#include <vector>
#include <map>

namespace Tail
{

    enum class TokenType
    {
        // Single-character tokens
        LEFT_PAREN,
        RIGHT_PAREN,
        LEFT_BRACE,
        RIGHT_BRACE,
        LEFT_BRACKET,
        RIGHT_BRACKET,
        COMMA,
        DOT,
        SEMICOLON,
        COLON,

        // Operators
        BANG,
        BANG_EQUAL,
        EQUAL,
        EQUAL_EQUAL,
        GREATER,
        GREATER_EQUAL,
        LESS,
        LESS_EQUAL,
        PLUS,
        PLUS_EQUAL,
        MINUS,
        MINUS_EQUAL,
        STAR,
        STAR_EQUAL,
        SLASH,
        SLASH_EQUAL,
        MOD,
        MOD_EQUAL,

        // Literals
        IDENTIFIER,
        STRING,
        NUMBER,
        FLOAT,

        // Keywords
        AND,
        OR,
        NOT,
        IF,
        ELSE,
        FOR,
        WHILE,
        DO,
        BREAK,
        CONTINUE,
        RETURN,
        TRUE,
        FALSE,
        NIL,
        FN,
        INCLUDE,
        INT,
        FLOAT_TYPE,
        STR,
        BOOL,
        BYTE,
        UNMUT,
        MUT,

        // Special
        EOF_TOKEN,
        ERROR
    };

    struct Token
    {
        TokenType type;
        std::string text;
        size_t line;
        size_t column;

        Token(TokenType t, const std::string &txt, size_t ln, size_t col)
            : type(t), text(txt), line(ln), column(col) {}
    };

    class Lexer
    {
    public:
        Lexer(const std::string &source);

        std::vector<Token> tokenize();
        std::vector<std::string> getErrors() const { return errors; }

    private:
        std::string source;
        size_t start;
        size_t current;
        size_t line;
        size_t column;
        std::vector<Token> tokens;
        std::vector<std::string> errors;

        static std::map<std::string, TokenType> keywords;

        bool isAtEnd() const;
        char advance();
        char peek() const;
        char peekNext() const;
        bool match(char expected);

        void addToken(TokenType type);
        void addToken(TokenType type, const std::string &text);

        void scanToken();
        void scanString();
        void scanNumber();
        void scanIdentifier();

        void error(const std::string &message);
    };
}