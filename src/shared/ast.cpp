#include "ast.h"
#include "value.h"
#include <sstream>

namespace Tail
{

    // LiteralExpr
    LiteralExpr::LiteralExpr(const Value &v) : value(v) {}
    std::string LiteralExpr::toString() const { return value.toString(); }

    // VariableExpr
    VariableExpr::VariableExpr(const std::string &n) : name(n) {}
    std::string VariableExpr::toString() const { return name; }

    // BinaryExpr
    BinaryExpr::BinaryExpr(std::shared_ptr<Expr> l, const std::string &o, std::shared_ptr<Expr> r)
        : left(l), op(o), right(r) {}
    std::string BinaryExpr::toString() const
    {
        return "(" + left->toString() + " " + op + " " + right->toString() + ")";
    }

    // CompareExpr
    CompareExpr::CompareExpr(std::shared_ptr<Expr> l, const std::string &o, std::shared_ptr<Expr> r)
        : left(l), op(o), right(r) {}
    std::string CompareExpr::toString() const
    {
        return "(" + left->toString() + " " + op + " " + right->toString() + ")";
    }

    // LogicalExpr
    LogicalExpr::LogicalExpr(std::shared_ptr<Expr> l, const std::string &o, std::shared_ptr<Expr> r)
        : left(l), op(o), right(r) {}
    std::string LogicalExpr::toString() const
    {
        if (op == "!")
        {
            return "(!" + right->toString() + ")";
        }
        return "(" + left->toString() + " " + op + " " + right->toString() + ")";
    }

    // CallExpr
    CallExpr::CallExpr(const std::string &cls, const std::string &method,
                       const std::vector<std::shared_ptr<Expr>> &a, bool native)
        : className(cls), methodName(method), args(a), isNative(native) {}

    CallExpr::CallExpr(const std::string &method, const std::vector<std::shared_ptr<Expr>> &a)
        : methodName(method), args(a), isNative(false) {}

    std::string CallExpr::toString() const
    {
        std::stringstream result;
        if (!className.empty())
        {
            result << className << "." << methodName;
        }
        else
        {
            result << methodName;
        }
        result << "(";
        for (size_t i = 0; i < args.size(); ++i)
        {
            if (i > 0)
                result << ", ";
            result << args[i]->toString();
        }
        result << ")";
        return result.str();
    }

    // ArrayExpr
    ArrayExpr::ArrayExpr(const std::vector<std::shared_ptr<Expr>> &elems) : elements(elems) {}
    std::string ArrayExpr::toString() const
    {
        std::string result = "{";
        for (size_t i = 0; i < elements.size(); ++i)
        {
            if (i > 0)
                result += ", ";
            result += elements[i]->toString();
        }
        result += "}";
        return result;
    }

    // IndexExpr
    IndexExpr::IndexExpr(std::shared_ptr<Expr> arr, std::shared_ptr<Expr> idx)
        : array(arr), index(idx) {}
    std::string IndexExpr::toString() const
    {
        return array->toString() + "[" + index->toString() + "]";
    }

    // ExprStmt
    ExprStmt::ExprStmt(std::shared_ptr<Expr> expr) : expression(expr) {}
    std::string ExprStmt::toString() const
    {
        return expression->toString() + ";";
    }

    // VarDeclStmt
    VarDeclStmt::VarDeclStmt(bool mut, const std::string &t, const std::string &n,
                             std::shared_ptr<Expr> init)
        : isMutable(mut), type(t), name(n), initializer(init) {}

    std::string VarDeclStmt::toString() const
    {
        std::string result = (isMutable ? "" : "unmut ") + type + " " + name;
        if (initializer)
        {
            result += " = " + initializer->toString();
        }
        result += ";";
        return result;
    }

    // AssignStmt
    AssignStmt::AssignStmt(const std::string &n, std::shared_ptr<Expr> v)
        : name(n), value(v) {}
    std::string AssignStmt::toString() const
    {
        return name + " = " + value->toString() + ";";
    }

    // BlockStmt
    BlockStmt::BlockStmt() {}
    std::string BlockStmt::toString() const
    {
        std::string result = "{\n";
        for (const auto &stmt : statements)
        {
            result += "  " + stmt->toString() + "\n";
        }
        result += "}";
        return result;
    }

    // FunctionStmt
    FunctionStmt::FunctionStmt() : qualifiedName("") {}
    std::string FunctionStmt::toString() const
    {
        std::string result = "fn " + name + "(";
        for (size_t i = 0; i < parameters.size(); ++i)
        {
            if (i > 0)
                result += ", ";
            result += parameters[i].type + " " + parameters[i].name;
        }
        result += ") {\n";
        for (const auto &stmt : body)
        {
            result += "  " + stmt->toString() + "\n";
        }
        result += "}";
        return result;
    }

    // ReturnStmt
    ReturnStmt::ReturnStmt(std::shared_ptr<Expr> v) : value(v) {}
    std::string ReturnStmt::toString() const
    {
        if (value)
        {
            return "return " + value->toString() + ";";
        }
        return "return;";
    }

    // IfStmt
    IfStmt::IfStmt(std::shared_ptr<Expr> cond, std::shared_ptr<BlockStmt> thenBr,
                   std::shared_ptr<Stmt> elseBr)
        : condition(cond), thenBranch(thenBr), elseBranch(elseBr) {}

    std::string IfStmt::toString() const
    {
        std::string result = "if " + condition->toString() + " " + thenBranch->toString();
        if (elseBranch)
        {
            result += " else " + elseBranch->toString();
        }
        return result;
    }

    // WhileStmt
    WhileStmt::WhileStmt(std::shared_ptr<Expr> cond, std::shared_ptr<Stmt> b)
        : condition(cond), body(b) {}
    std::string WhileStmt::toString() const
    {
        return "while (" + condition->toString() + ") " + body->toString();
    }

    // ForStmt
    ForStmt::ForStmt(std::shared_ptr<Stmt> init, std::shared_ptr<Expr> cond,
                     std::shared_ptr<Expr> inc, std::shared_ptr<Stmt> b)
        : initializer(init), condition(cond), increment(inc), body(b) {}

    std::string ForStmt::toString() const
    {
        std::string result = "for (";
        if (initializer)
        {
            result += initializer->toString();
        }
        result += "; ";
        if (condition)
        {
            result += condition->toString();
        }
        result += "; ";
        if (increment)
        {
            result += increment->toString();
        }
        result += ") " + body->toString();
        return result;
    }

    // BreakStmt
    BreakStmt::BreakStmt() {}
    std::string BreakStmt::toString() const
    {
        return "break;";
    }

    // ContinueStmt
    ContinueStmt::ContinueStmt() {}
    std::string ContinueStmt::toString() const
    {
        return "continue;";
    }

    // ArrayDeclStmt
    ArrayDeclStmt::ArrayDeclStmt(const std::string &t, const std::string &n,
                                 std::shared_ptr<Expr> s, std::shared_ptr<Expr> init)
        : type(t), name(n), size(s), initializer(init) {}

    std::string ArrayDeclStmt::toString() const
    {
        std::string result = type + " " + name + "[]";
        if (size)
        {
            result += "[" + size->toString() + "]";
        }
        if (initializer)
        {
            result += " = " + initializer->toString();
        }
        result += ";";
        return result;
    }
}