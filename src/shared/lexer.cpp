#include "lexer.h"
#include <cctype>
#include <algorithm>

namespace Tail
{

    Lexer::Lexer(const std::string &src)
        : source(src), start(0), current(0), line(1), column(1)
    {

        // Initialize keywords
        keywords = {
            {"and", TokenType::AND},
            {"or", TokenType::OR},
            {"not", TokenType::NOT},
            {"if", TokenType::IF},
            {"else", TokenType::ELSE},
            {"for", TokenType::FOR},
            {"while", TokenType::WHILE},
            {"do", TokenType::DO},
            {"break", TokenType::BREAK},
            {"continue", TokenType::CONTINUE},
            {"return", TokenType::RETURN},
            {"true", TokenType::TRUE},
            {"false", TokenType::FALSE},
            {"nil", TokenType::NIL},
            {"fn", TokenType::FN},
            {"include", TokenType::INCLUDE},
            {"int", TokenType::INT},
            {"float", TokenType::FLOAT_TYPE},
            {"str", TokenType::STR},
            {"bool", TokenType::BOOL},
            {"byte", TokenType::BYTE},
            {"unmut", TokenType::UNMUT},
            {"mut", TokenType::MUT}};
    }

    std::vector<Token> Lexer::tokenize()
    {
        tokens.clear();
        errors.clear();

        while (!isAtEnd())
        {
            start = current;
            scanToken();
        }

        tokens.push_back(Token(TokenType::EOF_TOKEN, "", line, column));
        return tokens;
    }

    bool Lexer::isAtEnd() const
    {
        return current >= source.size();
    }

    char Lexer::advance()
    {
        if (isAtEnd())
            return '\0';
        char c = source[current++];
        if (c == '\n')
        {
            line++;
            column = 1;
        }
        else
        {
            column++;
        }
        return c;
    }

    char Lexer::peek() const
    {
        if (isAtEnd())
            return '\0';
        return source[current];
    }

    char Lexer::peekNext() const
    {
        if (current + 1 >= source.size())
            return '\0';
        return source[current + 1];
    }

    bool Lexer::match(char expected)
    {
        if (isAtEnd())
            return false;
        if (source[current] != expected)
            return false;
        advance();
        return true;
    }

    void Lexer::addToken(TokenType type)
    {
        std::string text = source.substr(start, current - start);
        tokens.push_back(Token(type, text, line, column));
    }

    void Lexer::addToken(TokenType type, const std::string &text)
    {
        tokens.push_back(Token(type, text, line, column));
    }

    void Lexer::scanToken()
    {
        char c = advance();

        switch (c)
        {
        // Single character tokens
        case '(':
            addToken(TokenType::LEFT_PAREN);
            break;
        case ')':
            addToken(TokenType::RIGHT_PAREN);
            break;
        case '{':
            addToken(TokenType::LEFT_BRACE);
            break;
        case '}':
            addToken(TokenType::RIGHT_BRACE);
            break;
        case '[':
            addToken(TokenType::LEFT_BRACKET);
            break;
        case ']':
            addToken(TokenType::RIGHT_BRACKET);
            break;
        case ',':
            addToken(TokenType::COMMA);
            break;
        case '.':
            addToken(TokenType::DOT);
            break;
        case ';':
            addToken(TokenType::SEMICOLON);
            break;
        case ':':
            addToken(TokenType::COLON);
            break;

        // Operators
        case '!':
            addToken(match('=') ? TokenType::BANG_EQUAL : TokenType::BANG);
            break;
        case '=':
            addToken(match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL);
            break;
        case '>':
            addToken(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER);
            break;
        case '<':
            addToken(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS);
            break;
        case '+':
            addToken(match('=') ? TokenType::PLUS_EQUAL : TokenType::PLUS);
            break;
        case '-':
            addToken(match('=') ? TokenType::MINUS_EQUAL : TokenType::MINUS);
            break;
        case '*':
            addToken(match('=') ? TokenType::STAR_EQUAL : TokenType::STAR);
            break;
        case '/':
            if (match('/'))
            {
                // Comment, skip to end of line
                while (peek() != '\n' && !isAtEnd())
                    advance();
            }
            else if (match('='))
            {
                addToken(TokenType::SLASH_EQUAL);
            }
            else
            {
                addToken(TokenType::SLASH);
            }
            break;
        case '%':
            addToken(match('=') ? TokenType::MOD_EQUAL : TokenType::MOD);
            break;

        // Whitespace
        case ' ':
        case '\r':
        case '\t':
            // Ignore whitespace
            break;
        case '\n':
            line++;
            column = 1;
            break;

        // String literal
        case '"':
            scanString();
            break;

        default:
            if (isdigit(c))
            {
                scanNumber();
            }
            else if (isalpha(c) || c == '_')
            {
                scanIdentifier();
            }
            else
            {
                error("Unexpected character: " + std::string(1, c));
            }
            break;
        }
    }

    void Lexer::scanString()
    {
        std::string value;
        while (peek() != '"' && !isAtEnd())
        {
            if (peek() == '\n')
            {
                line++;
                column = 1;
            }
            if (peek() == '\\')
            {
                advance(); // Skip backslash
                switch (peek())
                {
                case 'n':
                    value += '\n';
                    break;
                case 't':
                    value += '\t';
                    break;
                case 'r':
                    value += '\r';
                    break;
                case '"':
                    value += '"';
                    break;
                case '\\':
                    value += '\\';
                    break;
                default:
                    value += '\\';
                    value += peek();
                    break;
                }
                advance();
            }
            else
            {
                value += advance();
            }
        }

        if (isAtEnd())
        {
            error("Unterminated string");
            return;
        }

        advance(); // Closing quote
        addToken(TokenType::STRING, value);
    }

    void Lexer::scanNumber()
    {
        bool isFloat = false;

        while (isdigit(peek()))
            advance();

        // Check for decimal point
        if (peek() == '.' && isdigit(peekNext()))
        {
            isFloat = true;
            advance(); // Consume '.'
            while (isdigit(peek()))
                advance();
        }

        std::string text = source.substr(start, current - start);
        addToken(isFloat ? TokenType::FLOAT : TokenType::NUMBER, text);
    }

    void Lexer::scanIdentifier()
    {
        while (isalnum(peek()) || peek() == '_')
            advance();

        std::string text = source.substr(start, current - start);

        // Check if it's a keyword
        auto it = keywords.find(text);
        if (it != keywords.end())
        {
            addToken(it->second);
        }
        else
        {
            addToken(TokenType::IDENTIFIER, text);
        }
    }

    void Lexer::error(const std::string &message)
    {
        std::string errorMsg = "Lexer error at line " + std::to_string(line) +
                               ", column " + std::to_string(column) + ": " + message;
        errors.push_back(errorMsg);
    }

    // Static member initialization
    std::map<std::string, TokenType> Lexer::keywords;
}
