#pragma once
#include "../shared/bytecode.h"
#include "../shared/ast.h"
#include <memory>
#include <map>
#include <vector>
#include <string>

namespace Tail
{

    class Compiler
    {
    public:
        Compiler();

        TVM::BytecodeFile compile(const std::vector<std::shared_ptr<Stmt>> &ast);

        // Debug
        void dump() const { bytecode.dump(); }

        bool isStandardLibraryFunction(const std::string &className, const std::string &methodName);

        void compileFunction(const FunctionStmt &stmt, const std::string &sourceFileName = "");

    private:
        struct LocalVar
        {
            std::string name;
            uint32_t index;
            bool isParam;

            LocalVar(const std::string &n, uint32_t idx, bool param = false)
                : name(n), index(idx), isParam(param) {}
        };

        struct FunctionContext
        {
            std::vector<LocalVar> locals;
            std::map<std::string, uint32_t> varMap; // name -> local index
            uint32_t nextLocal = 0;
            uint32_t startAddr = 0;
            uint32_t paramCount = 0;

            bool hasLocal(const std::string &name) const
            {
                return varMap.find(name) != varMap.end();
            }

            uint32_t addLocal(const std::string &name, bool isParam = false)
            {
                varMap[name] = nextLocal;
                locals.push_back(LocalVar(name, nextLocal, isParam));
                return nextLocal++;
            }
        };

        struct LoopContext
        {
            uint32_t breakAddr;    // Placeholder for break address
            uint32_t continueAddr; // Placeholder for continue address
            std::vector<uint32_t> breakPatches;
            std::vector<uint32_t> continuePatches;
        };

        // State
        TVM::BytecodeFile bytecode;
        std::vector<FunctionContext> contextStack;
        std::vector<LoopContext> loopStack;
        std::map<std::string, uint32_t> globalMap;
        std::map<std::string, uint32_t> functionAddrs; // name -> address

        // Helpers
        FunctionContext &currentContext() { return contextStack.back(); }

        // Compilation methods
        void compileStmt(const std::shared_ptr<Stmt> &stmt);
        void compileExpr(const std::shared_ptr<Expr> &expr);

        // Specific statement compilers
        void compileVarDecl(const VarDeclStmt &stmt);
        void compileAssign(const AssignStmt &stmt);
        void compileExprStmt(const ExprStmt &stmt);
        void compileBlock(const BlockStmt &stmt);
        void compileIf(const IfStmt &stmt);
        void compileWhile(const WhileStmt &stmt);
        void compileFor(const ForStmt &stmt);
        void compileReturn(const ReturnStmt &stmt);
        void compileBreak(const BreakStmt &stmt);
        void compileContinue(const ContinueStmt &stmt);
        void compileArrayDecl(const ArrayDeclStmt &stmt);

        // Expression compilers
        void compileLiteral(const LiteralExpr &expr);
        void compileVariable(const VariableExpr &expr);
        void compileBinary(const BinaryExpr &expr);
        void compileCompare(const CompareExpr &expr);
        void compileLogical(const LogicalExpr &expr);
        void compileCall(const CallExpr &expr);
        void compileArray(const ArrayExpr &expr);
        void compileIndex(const IndexExpr &expr);

        // Code generation
        void emit(TVM::OpCode op, uint32_t operand = 0);
        void emitPushInt(int64_t value);
        void emitPushFloat(double value);
        void emitPushBool(bool value);
        void emitPushString(const std::string &value);
        void emitPushNil();

        // Constants management
        uint32_t addConstantInt(int64_t value);
        uint32_t addConstantFloat(double value);
        uint32_t addConstantBool(bool value);
        uint32_t addConstantString(const std::string &value);
        uint32_t addConstantArray(const std::vector<int64_t> &arr);
        uint32_t addConstantArray(const std::vector<double> &arr);
        uint32_t addConstantArray(const std::vector<std::string> &arr);

        // Variables
        uint32_t resolveLocal(const std::string &name);
        uint32_t resolveGlobal(const std::string &name);
        void declareLocal(const std::string &name, bool isParam = false);

        // Control flow
        uint32_t emitJump(TVM::OpCode op);
        void patchJump(uint32_t jumpAddr);
        void patchJumps(const std::vector<uint32_t> &patches, uint32_t target);

        // Native functions
        uint32_t addNativeImport(const std::string &name);

        // Array support
        void compileNewArray(TVM::ValueType elemType, uint32_t size);

        void countLocals(const std::vector<std::shared_ptr<Stmt>> &stmts, FunctionContext &ctx);
    };

} // namespace Tail