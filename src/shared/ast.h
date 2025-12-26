#pragma once
#include <memory>
#include <vector>
#include <string>
#include "value.h"

namespace Tail
{

    class Interpreter;
    class Environment;

    struct Expr
    {
        virtual ~Expr() = default;
        virtual std::string toString() const = 0;
    };

    struct Stmt
    {
        virtual ~Stmt() = default;
        virtual std::string toString() const = 0;
    };

    struct LiteralExpr : Expr
    {
        Value value;
        LiteralExpr(const Value &v);
        std::string toString() const override;
    };

    struct VariableExpr : Expr
    {
        std::string name;
        VariableExpr(const std::string &n);
        std::string toString() const override;
    };

    struct BinaryExpr : Expr
    {
        std::shared_ptr<Expr> left;
        std::string op;
        std::shared_ptr<Expr> right;
        BinaryExpr(std::shared_ptr<Expr> l, const std::string &o, std::shared_ptr<Expr> r);
        std::string toString() const override;
    };

    struct CompareExpr : Expr
    {
        std::shared_ptr<Expr> left;
        std::string op;
        std::shared_ptr<Expr> right;
        CompareExpr(std::shared_ptr<Expr> l, const std::string &o, std::shared_ptr<Expr> r);
        std::string toString() const override;
    };

    struct LogicalExpr : Expr
    {
        std::shared_ptr<Expr> left;
        std::string op;
        std::shared_ptr<Expr> right;
        LogicalExpr(std::shared_ptr<Expr> l, const std::string &o, std::shared_ptr<Expr> r);
        std::string toString() const override;
    };

    struct CallExpr : Expr
    {
        std::string className;
        std::string methodName;
        std::vector<std::shared_ptr<Expr>> args;
        bool isNative;

        CallExpr(const std::string &cls, const std::string &method,
                 const std::vector<std::shared_ptr<Expr>> &a, bool native = false);
        CallExpr(const std::string &method, const std::vector<std::shared_ptr<Expr>> &a);
        std::string toString() const override;
    };

    struct ArrayExpr : Expr
    {
        std::vector<std::shared_ptr<Expr>> elements;
        ArrayExpr(const std::vector<std::shared_ptr<Expr>> &elems);
        std::string toString() const override;
    };

    struct IndexExpr : Expr
    {
        std::shared_ptr<Expr> array;
        std::shared_ptr<Expr> index;
        IndexExpr(std::shared_ptr<Expr> arr, std::shared_ptr<Expr> idx);
        std::string toString() const override;
    };

    struct ExprStmt : Stmt
    {
        std::shared_ptr<Expr> expression;
        ExprStmt(std::shared_ptr<Expr> expr);
        std::string toString() const override;
    };

    struct VarDeclStmt : Stmt
    {
        bool isMutable;
        std::string type;
        std::string name;
        std::shared_ptr<Expr> initializer;

        VarDeclStmt(bool mut, const std::string &t, const std::string &n,
                    std::shared_ptr<Expr> init = nullptr);
        std::string toString() const override;
    };

    struct AssignStmt : Stmt
    {
        std::string name;
        std::shared_ptr<Expr> value;
        AssignStmt(const std::string &n, std::shared_ptr<Expr> v);
        std::string toString() const override;
    };

    struct BlockStmt : Stmt
    {
        std::vector<std::shared_ptr<Stmt>> statements;
        BlockStmt();
        std::string toString() const override;
    };

    struct FunctionParam
    {
        std::string type;
        std::string name;
    };

    struct FunctionStmt : Stmt
    {
        std::string name;
        std::string qualifiedName;
        std::vector<FunctionParam> parameters;
        std::vector<std::shared_ptr<Stmt>> body;

        FunctionStmt();
        std::string toString() const override;
    };

    struct ReturnStmt : Stmt
    {
        std::shared_ptr<Expr> value;
        ReturnStmt(std::shared_ptr<Expr> v = nullptr);
        std::string toString() const override;
    };

    struct IfStmt : Stmt
    {
        std::shared_ptr<Expr> condition;
        std::shared_ptr<BlockStmt> thenBranch;
        std::shared_ptr<Stmt> elseBranch;

        IfStmt(std::shared_ptr<Expr> cond, std::shared_ptr<BlockStmt> thenBr,
               std::shared_ptr<Stmt> elseBr = nullptr);
        std::string toString() const override;
    };

    struct WhileStmt : Stmt
    {
        std::shared_ptr<Expr> condition;
        std::shared_ptr<Stmt> body;

        WhileStmt(std::shared_ptr<Expr> cond, std::shared_ptr<Stmt> b);
        std::string toString() const override;
    };

    struct ForStmt : Stmt
    {
        std::shared_ptr<Stmt> initializer;
        std::shared_ptr<Expr> condition;
        std::shared_ptr<Expr> increment;
        std::shared_ptr<Stmt> body;

        ForStmt(std::shared_ptr<Stmt> init, std::shared_ptr<Expr> cond,
                std::shared_ptr<Expr> inc, std::shared_ptr<Stmt> b);
        std::string toString() const override;
    };

    struct BreakStmt : Stmt
    {
        BreakStmt();
        std::string toString() const override;
    };

    struct ContinueStmt : Stmt
    {
        ContinueStmt();
        std::string toString() const override;
    };

    struct ArrayDeclStmt : Stmt
    {
        std::string type;
        std::string name;
        std::shared_ptr<Expr> size;
        std::shared_ptr<Expr> initializer;

        ArrayDeclStmt(const std::string &t, const std::string &n,
                      std::shared_ptr<Expr> s = nullptr,
                      std::shared_ptr<Expr> init = nullptr);
        std::string toString() const override;
    };
}