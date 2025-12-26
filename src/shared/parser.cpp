#include "parser.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstdint>
#include <filesystem>

namespace Tail
{
    std::string Parser::extractBaseName(const std::string &path)
    {
        size_t lastSlash = path.find_last_of("/\\");
        std::string filename = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;

        size_t dotPos = filename.find_last_of('.');
        if (dotPos != std::string::npos)
        {
            filename = filename.substr(0, dotPos);
        }

        return filename;
    }

    Parser::Parser(const std::vector<Token> &t) : tokens(t), pos(0) {}

    std::vector<std::shared_ptr<Stmt>> Parser::parse()
    {
        std::vector<std::shared_ptr<Stmt>> statements;

        while (!isAtEnd())
        {
            try
            {
                auto stmt = parseDeclaration();
                if (stmt)
                {
                    statements.push_back(stmt);
                }
            }
            catch (const std::runtime_error &e)
            {
                errors.push_back(e.what());
                synchronize();
            }
        }

        return statements;
    }

    bool Parser::isAtEnd() const
    {
        return pos >= tokens.size() || tokens[pos].type == TokenType::EOF_TOKEN;
    }

    const Token &Parser::peek() const
    {
        if (isAtEnd())
            return tokens.back();
        return tokens[pos];
    }

    const Token &Parser::advance()
    {
        if (!isAtEnd())
            pos++;
        return tokens[pos - 1];
    }

    bool Parser::check(TokenType type) const
    {
        if (isAtEnd())
            return false;
        return tokens[pos].type == type;
    }

    bool Parser::match(TokenType type)
    {
        if (check(type))
        {
            advance();
            return true;
        }
        return false;
    }

    const Token &Parser::consume(TokenType type, const std::string &message)
    {
        if (check(type))
            return advance();
        error(peek(), message);
        throw std::runtime_error(message);
    }

    void Parser::error(const Token &token, const std::string &message)
    {
        std::stringstream ss;
        ss << "Parse error at line " << token.line
           << ", column " << token.column << ": " << message;
        errors.push_back(ss.str());
    }

    void Parser::synchronize()
    {
        advance();

        while (!isAtEnd())
        {
            if (tokens[pos - 1].type == TokenType::SEMICOLON)
                return;

            switch (peek().type)
            {
            case TokenType::FN:
            case TokenType::IF:
            case TokenType::FOR:
            case TokenType::WHILE:
            case TokenType::RETURN:
            case TokenType::INCLUDE:
                return;
            default:
                advance();
                break;
            }
        }
    }

    std::shared_ptr<Stmt> Parser::parseDeclaration()
    {
        try
        {
            if (match(TokenType::INCLUDE))
            {
                return parseInclude();
            }
            if (match(TokenType::FN))
            {
                return parseFunction();
            }
            return parseStatement();
        }
        catch (const std::runtime_error &e)
        {
            errors.push_back(e.what());
            synchronize();
            return nullptr;
        }
    }

    std::shared_ptr<Stmt> Parser::parseInclude()
    {
        Token nameToken = consume(TokenType::IDENTIFIER, "Expected library name after 'include'");
        consume(TokenType::SEMICOLON, "Expected ';' after include");

        std::cout << "DEBUG PARSER: Processing include: " << nameToken.text << std::endl;

        std::string includePath = nameToken.text;
        std::string baseName = extractBaseName(includePath);

        includedFiles[baseName] = includePath;

        std::cout << "DEBUG PARSER: Stored include '" << baseName << "' -> '" << includePath << "'" << std::endl;
        std::cout << "DEBUG PARSER: Total includes now: " << includedFiles.size() << std::endl;

        return nullptr;
    }

    std::shared_ptr<Stmt> Parser::parseFunction()
    {
        auto name = consume(TokenType::IDENTIFIER, "Expected function name");

        consume(TokenType::LEFT_PAREN, "Expected '(' after function name");

        std::vector<FunctionParam> parameters;

        if (!check(TokenType::RIGHT_PAREN))
        {
            do
            {
                Token typeToken = peek();

                if (!isTypeToken(typeToken.type))
                {
                    std::stringstream ss;
                    ss << "Expected parameter type (int, float, str, bool, byte), got: '";
                    ss << typeToken.text << "'";
                    error(typeToken, ss.str());
                    throw std::runtime_error("Expected parameter type");
                }

                advance();

                auto paramName = consume(TokenType::IDENTIFIER, "Expected parameter name");

                parameters.push_back({typeToken.text, paramName.text});

            } while (match(TokenType::COMMA));
        }

        consume(TokenType::RIGHT_PAREN, "Expected ')' after parameters");

        consume(TokenType::LEFT_BRACE, "Expected '{' before function body");

        auto body = parseBlock();

        auto func = std::make_shared<FunctionStmt>();
        func->name = name.text;
        func->parameters = parameters;
        func->body = std::dynamic_pointer_cast<BlockStmt>(body)->statements;

        return func;
    }

    std::shared_ptr<Stmt> Parser::parseStatement()
    {
        if (match(TokenType::IF))
            return parseIfStatement();
        if (match(TokenType::WHILE))
            return parseWhileStatement();
        if (match(TokenType::FOR))
            return parseForStatement();
        if (match(TokenType::RETURN))
            return parseReturnStatement();
        if (match(TokenType::BREAK))
            return parseBreakStatement();
        if (match(TokenType::CONTINUE))
            return parseContinueStatement();
        if (match(TokenType::LEFT_BRACE))
            return parseBlock();

        size_t savedPos = pos;
        bool isMutable = match(TokenType::UNMUT) || match(TokenType::MUT);
        (void)isMutable;

        if (isTypeToken(peek().type))
        {
            pos = savedPos;
            return parseVarDeclaration();
        }

        if (check(TokenType::IDENTIFIER) && pos + 1 < tokens.size() &&
            tokens[pos + 1].type == TokenType::LEFT_BRACKET)
        {
            return parseArrayDeclaration();
        }

        auto expr = parseExpression();
        consume(TokenType::SEMICOLON, "Expected ';' after expression");
        return std::make_shared<ExprStmt>(expr);
    }

    std::shared_ptr<Stmt> Parser::parseVarDeclaration()
    {
        bool isMutable = true;
        if (match(TokenType::UNMUT))
        {
            isMutable = false;
        }
        else if (match(TokenType::MUT))
        {
            isMutable = true;
        }

        auto typeToken = advance();
        if (!isTypeToken(typeToken.type))
        {
            error(typeToken, "Expected type name");
            return nullptr;
        }

        auto name = consume(TokenType::IDENTIFIER, "Expected variable name");

        std::shared_ptr<Expr> initializer = nullptr;
        if (match(TokenType::EQUAL))
        {
            initializer = parseExpression();
        }

        consume(TokenType::SEMICOLON, "Expected ';' after variable declaration");

        return std::make_shared<VarDeclStmt>(isMutable, typeToken.text, name.text, initializer);
    }

    std::shared_ptr<Stmt> Parser::parseArrayDeclaration()
    {
        auto typeToken = advance();
        auto name = consume(TokenType::IDENTIFIER, "Expected array name");
        consume(TokenType::LEFT_BRACKET, "Expected '[' after array name");

        std::shared_ptr<Expr> size = nullptr;
        if (!check(TokenType::RIGHT_BRACKET))
        {
            size = parseExpression();
        }
        consume(TokenType::RIGHT_BRACKET, "Expected ']' after array size");

        std::shared_ptr<Expr> initializer = nullptr;
        if (match(TokenType::EQUAL))
        {
            initializer = parseExpression();
        }

        consume(TokenType::SEMICOLON, "Expected ';' after array declaration");

        return std::make_shared<ArrayDeclStmt>(typeToken.text, name.text, size, initializer);
    }

    std::shared_ptr<Stmt> Parser::parseBlock()
    {
        auto block = std::make_shared<BlockStmt>();

        while (!check(TokenType::RIGHT_BRACE) && !isAtEnd())
        {
            auto stmt = parseDeclaration();
            if (stmt)
            {
                block->statements.push_back(stmt);
            }
        }

        consume(TokenType::RIGHT_BRACE, "Expected '}' after block");
        return block;
    }

    std::shared_ptr<Stmt> Parser::parseIfStatement()
    {
        consume(TokenType::LEFT_PAREN, "Expected '(' after 'if'");
        auto condition = parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected ')' after condition");

        auto thenBranch = parseBlock();

        std::shared_ptr<Stmt> elseBranch = nullptr;
        if (match(TokenType::ELSE))
        {
            if (match(TokenType::IF))
            {
                elseBranch = parseIfStatement();
            }
            else
            {
                elseBranch = parseBlock();
            }
        }

        return std::make_shared<IfStmt>(condition, std::dynamic_pointer_cast<BlockStmt>(thenBranch), elseBranch);
    }

    std::shared_ptr<Stmt> Parser::parseWhileStatement()
    {
        consume(TokenType::LEFT_PAREN, "Expected '(' after 'while'");
        auto condition = parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected ')' after condition");

        auto body = parseStatement();

        return std::make_shared<WhileStmt>(condition, body);
    }

    std::shared_ptr<Stmt> Parser::parseForStatement()
    {
        consume(TokenType::LEFT_PAREN, "Expected '(' after 'for'");

        std::shared_ptr<Stmt> initializer = nullptr;
        if (!match(TokenType::SEMICOLON))
        {
            size_t savedPos = pos;
            bool isMutable = match(TokenType::UNMUT) || match(TokenType::MUT);
            (void)isMutable;

            bool isType = false;

            if (isTypeToken(peek().type))
            {
                isType = true;
            }

            pos = savedPos;

            if (isType)
            {
                initializer = parseVarDeclaration();
            }
            else
            {
                auto expr = parseExpression();
                initializer = std::make_shared<ExprStmt>(expr);
                consume(TokenType::SEMICOLON, "Expected ';' after for initializer");
            }
        }

        std::shared_ptr<Expr> condition = nullptr;
        if (!check(TokenType::SEMICOLON))
        {
            condition = parseExpression();
        }
        consume(TokenType::SEMICOLON, "Expected ';' after for condition");

        std::shared_ptr<Expr> increment = nullptr;
        if (!check(TokenType::RIGHT_PAREN))
        {
            increment = parseExpression();
        }
        consume(TokenType::RIGHT_PAREN, "Expected ')' after for clauses");

        auto body = parseStatement();

        return std::make_shared<ForStmt>(initializer, condition, increment, body);
    }

    std::shared_ptr<Stmt> Parser::parseReturnStatement()
    {
        std::shared_ptr<Expr> value = nullptr;
        if (!check(TokenType::SEMICOLON))
        {
            value = parseExpression();
        }

        consume(TokenType::SEMICOLON, "Expected ';' after return");
        return std::make_shared<ReturnStmt>(value);
    }

    std::shared_ptr<Stmt> Parser::parseBreakStatement()
    {
        consume(TokenType::SEMICOLON, "Expected ';' after break");
        return std::make_shared<BreakStmt>();
    }

    std::shared_ptr<Stmt> Parser::parseContinueStatement()
    {
        consume(TokenType::SEMICOLON, "Expected ';' after continue");
        return std::make_shared<ContinueStmt>();
    }

    std::shared_ptr<Expr> Parser::parseExpression()
    {
        return parseAssignment();
    }

    std::shared_ptr<Expr> Parser::parseAssignment()
    {
        auto expr = parseLogicalOr();

        if (match(TokenType::EQUAL))
        {
            auto value = parseAssignment();

            if (auto var = std::dynamic_pointer_cast<VariableExpr>(expr))
            {
                return std::make_shared<BinaryExpr>(var, "=", value);
            }

            error(peek(), "Invalid assignment target");
        }

        return expr;
    }

    std::shared_ptr<Expr> Parser::parseLogicalOr()
    {
        auto expr = parseLogicalAnd();

        while (match(TokenType::OR))
        {
            auto right = parseLogicalAnd();
            expr = std::make_shared<LogicalExpr>(expr, "||", right);
        }

        return expr;
    }

    std::shared_ptr<Expr> Parser::parseLogicalAnd()
    {
        auto expr = parseEquality();

        while (match(TokenType::AND))
        {
            auto right = parseEquality();
            expr = std::make_shared<LogicalExpr>(expr, "&&", right);
        }

        return expr;
    }

    std::shared_ptr<Expr> Parser::parseEquality()
    {
        auto expr = parseComparison();

        while (match(TokenType::BANG_EQUAL) || match(TokenType::EQUAL_EQUAL))
        {
            auto op = tokens[pos - 1];
            auto right = parseComparison();
            expr = std::make_shared<CompareExpr>(expr, op.text, right);
        }

        return expr;
    }

    std::shared_ptr<Expr> Parser::parseComparison()
    {
        auto expr = parseTerm();

        while (match(TokenType::GREATER) || match(TokenType::GREATER_EQUAL) ||
               match(TokenType::LESS) || match(TokenType::LESS_EQUAL))
        {
            auto op = tokens[pos - 1];
            auto right = parseTerm();
            expr = std::make_shared<CompareExpr>(expr, op.text, right);
        }

        return expr;
    }

    std::shared_ptr<Expr> Parser::parseUnary()
    {
        if (match(TokenType::BANG) || match(TokenType::MINUS))
        {
            auto op = tokens[pos - 1];
            auto right = parseUnary();
            return std::make_shared<LogicalExpr>(nullptr, op.text, right);
        }

        return parseCall();
    }

    std::shared_ptr<Expr> Parser::parseCall()
    {
        auto expr = parsePrimary();

        while (true)
        {
            if (match(TokenType::LEFT_PAREN))
            {
                expr = finishCall(expr);
            }
            else if (match(TokenType::DOT))
            {
                auto name = consume(TokenType::IDENTIFIER, "Expected property name after '.'");
                expr = std::make_shared<GetExpr>(expr, name.text);
            }
            else
            {
                break;
            }
        }

        return expr;
    }
    std::shared_ptr<Expr> Parser::parsePrimary()
    {
        if (match(TokenType::NUMBER))
        {
            int64_t value = std::stoll(tokens[pos - 1].text);
            return std::make_shared<LiteralExpr>(Value((int64_t)value));
        }
        if (match(TokenType::FLOAT))
        {
            double value = std::stod(tokens[pos - 1].text);
            return std::make_shared<LiteralExpr>(Value(value));
        }
        if (match(TokenType::STRING))
        {
            std::string value = tokens[pos - 1].text;
            return std::make_shared<LiteralExpr>(Value(value));
        }
        if (match(TokenType::TRUE))
        {
            return std::make_shared<LiteralExpr>(Value(true));
        }
        if (match(TokenType::FALSE))
        {
            return std::make_shared<LiteralExpr>(Value(false));
        }
        if (match(TokenType::NIL))
        {
            return std::make_shared<LiteralExpr>(Value());
        }
        if (match(TokenType::IDENTIFIER))
        {
            return std::make_shared<VariableExpr>(tokens[pos - 1].text);
        }
        if (match(TokenType::LEFT_PAREN))
        {
            auto expr = parseExpression();
            consume(TokenType::RIGHT_PAREN, "Expected ')' after expression");
            return expr;
        }

        error(peek(), "Expected expression");
        return nullptr;
    }

    bool Parser::isTypeToken(TokenType type) const
    {
        switch (type)
        {
        case TokenType::INT:
        case TokenType::FLOAT_TYPE:
        case TokenType::STR:
        case TokenType::BOOL:
        case TokenType::BYTE:
            return true;
        default:
            return false;
        }
    }

    std::shared_ptr<Expr> Parser::finishCall(std::shared_ptr<Expr> callee)
    {
        std::vector<std::shared_ptr<Expr>> args;

        if (!check(TokenType::RIGHT_PAREN))
        {
            do
            {
                args.push_back(parseExpression());
            } while (match(TokenType::COMMA));
        }

        consume(TokenType::RIGHT_PAREN, "Expected ')' after arguments");

        if (auto get = std::dynamic_pointer_cast<GetExpr>(callee))
        {
            if (auto varObj = std::dynamic_pointer_cast<VariableExpr>(get->object))
            {
                bool isNative = false;

                static const std::vector<std::string> realNatives = {
                    "Console", "Math", "String", "Array", "File", "System"};

                bool isRealNative = false;
                for (const auto &lib : realNatives)
                {
                    if (varObj->name == lib)
                    {
                        isRealNative = true;
                        break;
                    }
                }

                if (isRealNative)
                {
                    isNative = true;
                }
                else
                {
                    isNative = false;
                }

                std::cout << "DEBUG PARSER: " << varObj->name << "." << get->name
                          << " -> isNative: " << isNative << std::endl;

                return std::make_shared<CallExpr>(varObj->name, get->name, args, isNative);
            }

            return std::make_shared<CallExpr>("", get->name, args, true);
        }
        else if (auto var = std::dynamic_pointer_cast<VariableExpr>(callee))
        {
            return std::make_shared<CallExpr>("", var->name, args, false);
        }

        return std::make_shared<CallExpr>("", "", args, false);
    }
    std::string extractFileName(const std::string &path)
    {
        size_t lastSlash = path.find_last_of("/\\");
        std::string filename = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;

        size_t dotPos = filename.find_last_of('.');
        if (dotPos != std::string::npos)
        {
            filename = filename.substr(0, dotPos);
        }

        return filename;
    }

    std::shared_ptr<Expr> Parser::parseFactor()
    {
        auto expr = parseUnary();

        while (match(TokenType::STAR) || match(TokenType::SLASH) || match(TokenType::MOD))
        {
            auto op = tokens[pos - 1];
            auto right = parseUnary();
            expr = std::make_shared<BinaryExpr>(expr, op.text, right);
        }

        return expr;
    }

    std::shared_ptr<Expr> Parser::parseTerm()
    {
        auto expr = parseFactor();

        while (match(TokenType::PLUS) || match(TokenType::MINUS))
        {
            auto op = tokens[pos - 1];
            auto right = parseFactor();
            expr = std::make_shared<BinaryExpr>(expr, op.text, right);
        }

        return expr;
    }
}