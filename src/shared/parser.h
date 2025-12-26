#pragma once
#include "lexer.h"
#include "ast.h"
#include <memory>
#include <vector>

namespace Tail
{

    struct GetExpr : Expr
    {
        std::shared_ptr<Expr> object;
        std::string name;

        GetExpr(std::shared_ptr<Expr> obj, const std::string &n)
            : object(obj), name(n) {}

        std::string toString() const override
        {
            return object->toString() + "." + name;
        }
    };

    class Parser
    {
    public:
        Parser(const std::vector<Token> &tokens);

        std::vector<std::shared_ptr<Stmt>> parse();
        std::vector<std::string> getErrors() const { return errors; }
        bool isStandardLibrary(const std::string &name);

        const std::map<std::string, std::string> &getIncludedFiles() const
        {
            return includedFiles;
        }

    private:
        std::vector<Token> tokens;
        size_t pos;
        std::vector<std::string> errors;

        // Helper methods
        bool isAtEnd() const;
        const Token &peek() const;
        const Token &advance();
        bool check(TokenType type) const;
        bool match(TokenType type);
        const Token &consume(TokenType type, const std::string &message);
        void error(const Token &token, const std::string &message);
        void synchronize();
        std::shared_ptr<Stmt> parseInclude();

        // Parsing methods
        std::shared_ptr<Stmt> parseStatement();
        std::shared_ptr<Stmt> parseDeclaration();
        std::shared_ptr<Stmt> parseVarDeclaration();
        std::shared_ptr<Stmt> parseFunction();
        std::shared_ptr<Stmt> parseBlock();
        std::shared_ptr<Stmt> parseIfStatement();
        std::shared_ptr<Stmt> parseWhileStatement();
        std::shared_ptr<Stmt> parseForStatement();
        std::shared_ptr<Stmt> parseReturnStatement();
        std::shared_ptr<Stmt> parseBreakStatement();
        std::shared_ptr<Stmt> parseContinueStatement();
        std::shared_ptr<Stmt> parseArrayDeclaration();

        std::shared_ptr<Expr> parseExpression();
        std::shared_ptr<Expr> parseAssignment();
        std::shared_ptr<Expr> parseLogicalOr();
        std::shared_ptr<Expr> parseLogicalAnd();
        std::shared_ptr<Expr> parseEquality();
        std::shared_ptr<Expr> parseComparison();
        std::shared_ptr<Expr> parseTerm();
        std::shared_ptr<Expr> parseFactor();
        std::shared_ptr<Expr> parseUnary();
        std::shared_ptr<Expr> parseCall();
        std::shared_ptr<Expr> parsePrimary();
        std::shared_ptr<Expr> parseArrayLiteral();

        std::shared_ptr<Expr> finishCall(std::shared_ptr<Expr> callee);
        std::shared_ptr<Expr> finishMethodCall(std::shared_ptr<Expr> object,
                                               const std::string &method);

        // Type checking
        bool isTypeToken(TokenType type) const;
        std::map<std::string, std::string> includedFiles;
        static std::string extractBaseName(const std::string &path);
    };
}