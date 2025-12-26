#include "compiler.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <filesystem>

namespace Tail
{

    Compiler::Compiler()
    {
        contextStack.push_back(FunctionContext());
    }
    TVM::BytecodeFile Compiler::compile(const std::vector<std::shared_ptr<Stmt>> &ast)
    {
        std::cout << "DEBUG: Compiling AST with " << ast.size() << " statements" << std::endl;

        for (const auto &stmt : ast)
        {
            if (auto func = std::dynamic_pointer_cast<FunctionStmt>(stmt))
            {
                if (func->name != "Main")
                {
                    std::cout << "DEBUG: Compiling " << func->name << " first" << std::endl;
                    compileFunction(*func);
                }
            }
        }

        bool hasMain = false;
        for (const auto &stmt : ast)
        {
            if (auto func = std::dynamic_pointer_cast<FunctionStmt>(stmt))
            {
                if (func->name == "Main")
                {
                    std::cout << "DEBUG: Compiling Main last" << std::endl;
                    compileFunction(*func);
                    hasMain = true;
                    break;
                }
            }
        }

        if (!hasMain)
        {
            throw std::runtime_error("Main function not found");
        }

        if (bytecode.code.empty() || bytecode.code.back().opcode != TVM::OP_HALT)
        {
            emit(TVM::OP_HALT);
        }

        std::cout << "DEBUG: Generated " << bytecode.code.size() << " instructions" << std::endl;
        std::cout << "DEBUG: Generated " << bytecode.constants.size() << " constants" << std::endl;

        bytecode.dump();
        return bytecode;
    }
    void Compiler::compileStmt(const std::shared_ptr<Stmt> &stmt)
    {
        if (auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(stmt))
        {
            compileVarDecl(*varDecl);
        }
        else if (auto assign = std::dynamic_pointer_cast<AssignStmt>(stmt))
        {
            compileAssign(*assign);
        }
        else if (auto exprStmt = std::dynamic_pointer_cast<ExprStmt>(stmt))
        {
            compileExprStmt(*exprStmt);
        }
        else if (auto block = std::dynamic_pointer_cast<BlockStmt>(stmt))
        {
            compileBlock(*block);
        }
        else if (auto ifStmt = std::dynamic_pointer_cast<IfStmt>(stmt))
        {
            compileIf(*ifStmt);
        }
        else if (auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(stmt))
        {
            compileWhile(*whileStmt);
        }
        else if (auto forStmt = std::dynamic_pointer_cast<ForStmt>(stmt))
        {
            compileFor(*forStmt);
        }
        else if (auto returnStmt = std::dynamic_pointer_cast<ReturnStmt>(stmt))
        {
            compileReturn(*returnStmt);
        }
        else if (auto breakStmt = std::dynamic_pointer_cast<BreakStmt>(stmt))
        {
            compileBreak(*breakStmt);
        }
        else if (auto continueStmt = std::dynamic_pointer_cast<ContinueStmt>(stmt))
        {
            compileContinue(*continueStmt);
        }
        else if (auto func = std::dynamic_pointer_cast<FunctionStmt>(stmt))
        {
            compileFunction(*func);
        }
        else if (auto arrayDecl = std::dynamic_pointer_cast<ArrayDeclStmt>(stmt))
        {
            compileArrayDecl(*arrayDecl);
        }
        else
        {
            throw std::runtime_error("Unknown statement type");
        }
    }

    void Compiler::compileExpr(const std::shared_ptr<Expr> &expr)
    {
        if (auto lit = std::dynamic_pointer_cast<LiteralExpr>(expr))
        {
            compileLiteral(*lit);
        }
        else if (auto var = std::dynamic_pointer_cast<VariableExpr>(expr))
        {
            compileVariable(*var);
        }
        else if (auto bin = std::dynamic_pointer_cast<BinaryExpr>(expr))
        {
            compileBinary(*bin);
        }
        else if (auto cmp = std::dynamic_pointer_cast<CompareExpr>(expr))
        {
            compileCompare(*cmp);
        }
        else if (auto log = std::dynamic_pointer_cast<LogicalExpr>(expr))
        {
            compileLogical(*log);
        }
        else if (auto call = std::dynamic_pointer_cast<CallExpr>(expr))
        {
            compileCall(*call);
        }
        else if (auto arr = std::dynamic_pointer_cast<ArrayExpr>(expr))
        {
            compileArray(*arr);
        }
        else if (auto idx = std::dynamic_pointer_cast<IndexExpr>(expr))
        {
            compileIndex(*idx);
        }
        else
        {
            throw std::runtime_error("Unknown expression type");
        }
    }

    void Compiler::compileVarDecl(const VarDeclStmt &stmt)
    {
        if (stmt.initializer)
        {
            compileExpr(stmt.initializer);
            uint32_t localIdx = currentContext().addLocal(stmt.name);
            emit(TVM::OP_STORE, localIdx);
        }
        else
        {

            if (stmt.type == "int")
            {
                emitPushInt(0);
            }
            else if (stmt.type == "float")
            {
                emitPushFloat(0.0);
            }
            else if (stmt.type == "bool")
            {
                emitPushBool(false);
            }
            else if (stmt.type == "str")
            {
                emitPushString("");
            }
            else
            {
                emitPushNil();
            }
            uint32_t localIdx = currentContext().addLocal(stmt.name);
            emit(TVM::OP_STORE, localIdx);
        }
    }

    void Compiler::compileAssign(const AssignStmt &stmt)
    {
        compileExpr(stmt.value);
        uint32_t idx = resolveLocal(stmt.name);
        if (idx != UINT32_MAX)
        {
            emit(TVM::OP_STORE, idx);
        }
        else
        {
            idx = resolveGlobal(stmt.name);
            if (idx != UINT32_MAX)
            {
                emit(TVM::OP_STORE_GLOBAL, idx);
            }
            else
            {
                throw std::runtime_error("Undefined variable: " + stmt.name);
            }
        }
    }

    void Compiler::compileExprStmt(const ExprStmt &stmt)
    {
        compileExpr(stmt.expression);

        if (auto call = std::dynamic_pointer_cast<CallExpr>(stmt.expression))
        {
            if (call->isNative)
            {
                std::string fullName = call->className + "." + call->methodName;
                if (fullName == "Console.println" || fullName == "Console.print")
                {
                    return;
                }
            }
        }

        emit(TVM::OP_POP);
    }

    void Compiler::compileBlock(const BlockStmt &stmt)
    {

        contextStack.push_back(FunctionContext());

        for (const auto &blockStmt : stmt.statements)
        {
            compileStmt(blockStmt);
        }

        contextStack.pop_back();
    }

    void Compiler::compileIf(const IfStmt &stmt)
    {
        compileExpr(stmt.condition);

        uint32_t thenJump = emitJump(TVM::OP_JMP_IFNOT);

        compileStmt(stmt.thenBranch);

        uint32_t elseJump = 0;
        if (stmt.elseBranch)
        {
            elseJump = emitJump(TVM::OP_JMP);
            patchJump(thenJump);

            compileStmt(stmt.elseBranch);
            patchJump(elseJump);
        }
        else
        {
            patchJump(thenJump);
        }
    }

    void Compiler::compileWhile(const WhileStmt &stmt)
    {

        LoopContext loop;
        loopStack.push_back(loop);

        uint32_t loopStart = bytecode.code.size();

        compileExpr(stmt.condition);
        uint32_t exitJump = emitJump(TVM::OP_JMP_IFNOT);

        compileStmt(stmt.body);

        if (!loopStack.back().continuePatches.empty())
        {
            patchJumps(loopStack.back().continuePatches, bytecode.code.size());
        }

        emit(TVM::OP_JMP, loopStart);

        patchJump(exitJump);

        if (!loopStack.back().breakPatches.empty())
        {
            patchJumps(loopStack.back().breakPatches, bytecode.code.size());
        }

        loopStack.pop_back();
    }

    void Compiler::compileFor(const ForStmt &stmt)
    {

        if (stmt.initializer)
        {
            compileStmt(stmt.initializer);
        }

        LoopContext loop;
        loopStack.push_back(loop);

        uint32_t loopStart = bytecode.code.size();

        if (stmt.condition)
        {
            compileExpr(stmt.condition);
            uint32_t exitJump = emitJump(TVM::OP_JMP_IFNOT);
            loop.breakPatches.push_back(exitJump);
        }

        compileStmt(stmt.body);

        if (!loopStack.back().continuePatches.empty())
        {
            patchJumps(loopStack.back().continuePatches, bytecode.code.size());
        }

        if (stmt.increment)
        {
            compileExpr(stmt.increment);
            emit(TVM::OP_POP);
        }

        emit(TVM::OP_JMP, loopStart);

        uint32_t exitAddr = bytecode.code.size();

        if (!loopStack.back().breakPatches.empty())
        {
            patchJumps(loopStack.back().breakPatches, exitAddr);
        }

        loopStack.pop_back();
    }

    void Compiler::compileReturn(const ReturnStmt &stmt)
    {
        if (stmt.value)
        {
            compileExpr(stmt.value);
        }
        else
        {
            emitPushNil();
        }
        emit(TVM::OP_RET);
    }

    void Compiler::compileBreak(const BreakStmt &stmt)
    {
        (void)stmt;
        if (loopStack.empty())
        {
            throw std::runtime_error("Break outside loop");
        }
        uint32_t jump = emitJump(TVM::OP_JMP);
        loopStack.back().breakPatches.push_back(jump);
    }

    void Compiler::compileContinue(const ContinueStmt &stmt)
    {
        (void)stmt;
        if (loopStack.empty())
        {
            throw std::runtime_error("Continue outside loop");
        }
        uint32_t jump = emitJump(TVM::OP_JMP);
        loopStack.back().continuePatches.push_back(jump);
    }

    void Compiler::compileFunction(const FunctionStmt &stmt, const std::string &sourceFileName)
    {
        uint32_t funcAddr = bytecode.code.size();

        std::string functionName = stmt.name;

        if (!sourceFileName.empty() && stmt.name != "Main")
        {
            std::filesystem::path p(sourceFileName);
            std::string moduleName = p.stem().string();
            functionName = moduleName + "_" + stmt.name;
            std::cout << "DEBUG compileFunction: Renaming " << stmt.name
                      << " → " << functionName << std::endl;
        }

        functionAddrs[functionName] = funcAddr;

        if (stmt.name != "Main")
        {
            functionAddrs[stmt.name] = funcAddr;
        }

        std::cout << "DEBUG: Compiling function " << functionName
                  << " (original: " << stmt.name << ") at address " << funcAddr << std::endl;

        FunctionContext countingCtx;
        countingCtx.paramCount = stmt.parameters.size();

        for (size_t i = 0; i < stmt.parameters.size(); i++)
        {
            countingCtx.nextLocal++;
        }

        countLocals(stmt.body, countingCtx);

        FunctionContext funcCtx;
        funcCtx.startAddr = funcAddr;
        funcCtx.paramCount = stmt.parameters.size();

        for (size_t i = 0; i < stmt.parameters.size(); i++)
        {
            funcCtx.addLocal(stmt.parameters[i].name, true);
        }

        contextStack.push_back(funcCtx);

        for (const auto &bodyStmt : stmt.body)
        {
            compileStmt(bodyStmt);
        }

        if (bytecode.code.empty() ||
            (bytecode.code.back().opcode != TVM::OP_RET &&
             bytecode.code.back().opcode != TVM::OP_HALT))
        {
            std::cout << "DEBUG: Adding implicit return for function " << functionName << std::endl;
            emitPushNil();
            emit(TVM::OP_RET);
        }

        contextStack.pop_back();

        TVM::FunctionInfo info;
        info.name = functionName;
        info.address = funcAddr;
        info.arity = stmt.parameters.size();
        info.locals = countingCtx.nextLocal;
        bytecode.functions.push_back(info);

        std::cout << "DEBUG: Function " << functionName << " compiled, size: "
                  << (bytecode.code.size() - funcAddr) << " instructions" << std::endl;
        std::cout << "DEBUG: Function " << functionName << " has " << countingCtx.nextLocal
                  << " locals (params: " << stmt.parameters.size()
                  << ", vars: " << (countingCtx.nextLocal - stmt.parameters.size())
                  << ")" << std::endl;
    }
    void Compiler::countLocals(const std::vector<std::shared_ptr<Stmt>> &stmts, FunctionContext &ctx)
    {
        for (const auto &stmt : stmts)
        {
            if (auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(stmt))
            {
                ctx.nextLocal++;
            }
            else if (auto block = std::dynamic_pointer_cast<BlockStmt>(stmt))
            {
                countLocals(block->statements, ctx);
            }
            else if (auto ifStmt = std::dynamic_pointer_cast<IfStmt>(stmt))
            {
                countLocals({ifStmt->thenBranch}, ctx);
                if (ifStmt->elseBranch)
                {
                    countLocals({ifStmt->elseBranch}, ctx);
                }
            }
            else if (auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(stmt))
            {
                countLocals({whileStmt->body}, ctx);
            }
            else if (auto forStmt = std::dynamic_pointer_cast<ForStmt>(stmt))
            {
                if (forStmt->initializer)
                {
                    if (auto initVar = std::dynamic_pointer_cast<VarDeclStmt>(forStmt->initializer))
                    {
                        ctx.nextLocal++;
                    }
                }
                countLocals({forStmt->body}, ctx);
            }
        }
    }

    void Compiler::compileArrayDecl(const ArrayDeclStmt &stmt)
    {
        TVM::ValueType elemType;
        if (stmt.type == "int")
            elemType = TVM::TYPE_ARRAY_INT;
        else if (stmt.type == "float")
            elemType = TVM::TYPE_ARRAY_FLOAT;
        else if (stmt.type == "str")
            elemType = TVM::TYPE_ARRAY_STRING;
        else
            throw std::runtime_error("Unsupported array type: " + stmt.type);

        if (stmt.size)
        {
            compileExpr(stmt.size);
            compileNewArray(elemType, 0);
        }
        else if (stmt.initializer)
        {
            compileExpr(stmt.initializer);
        }
        else
        {

            compileNewArray(elemType, 0);
        }

        uint32_t localIdx = currentContext().addLocal(stmt.name);
        emit(TVM::OP_STORE, localIdx);
    }

    void Compiler::compileLiteral(const LiteralExpr &expr)
    {
        if (expr.value.isInt())
        {
            emitPushInt(expr.value.asInt());
        }
        else if (expr.value.isFloat())
        {
            emitPushFloat(expr.value.asFloat());
        }
        else if (expr.value.isBool())
        {
            emitPushBool(expr.value.asBool());
        }
        else if (expr.value.isStr())
        {
            emitPushString(expr.value.asStr());
        }
        else if (expr.value.isNil())
        {
            emitPushNil();
        }
        else
        {
            throw std::runtime_error("Unsupported literal type");
        }
    }

    void Compiler::compileVariable(const VariableExpr &expr)
    {
        uint32_t idx = resolveLocal(expr.name);
        if (idx != UINT32_MAX)
        {
            emit(TVM::OP_LOAD, idx);
        }
        else
        {
            idx = resolveGlobal(expr.name);
            if (idx != UINT32_MAX)
            {
                emit(TVM::OP_LOAD_GLOBAL, idx);
            }
            else
            {
                throw std::runtime_error("Undefined variable: " + expr.name);
            }
        }
    }

    void Compiler::compileBinary(const BinaryExpr &expr)
    {
        compileExpr(expr.left);
        compileExpr(expr.right);

        if (expr.op == "+")
            emit(TVM::OP_ADD);
        else if (expr.op == "-")
            emit(TVM::OP_SUB);
        else if (expr.op == "*")
            emit(TVM::OP_MUL);
        else if (expr.op == "/")
            emit(TVM::OP_DIV);
        else if (expr.op == "%")
            emit(TVM::OP_MOD);
        else
            throw std::runtime_error("Unknown binary operator: " + expr.op);
    }

    void Compiler::compileCompare(const CompareExpr &expr)
    {
        compileExpr(expr.left);
        compileExpr(expr.right);

        if (expr.op == "==")
            emit(TVM::OP_EQ);
        else if (expr.op == "!=")
            emit(TVM::OP_NEQ);
        else if (expr.op == "<")
            emit(TVM::OP_LT);
        else if (expr.op == "<=")
            emit(TVM::OP_LTE);
        else if (expr.op == ">")
            emit(TVM::OP_GT);
        else if (expr.op == ">=")
            emit(TVM::OP_GTE);
        else
            throw std::runtime_error("Unknown comparison operator: " + expr.op);
    }

    void Compiler::compileLogical(const LogicalExpr &expr)
    {
        if (expr.op == "!")
        {
            compileExpr(expr.right);
            emit(TVM::OP_NOT);
        }
        else if (expr.op == "&&")
        {
            compileExpr(expr.left);
            uint32_t jump = emitJump(TVM::OP_JMP_IFNOT);
            compileExpr(expr.right);
            patchJump(jump);
        }
        else if (expr.op == "||")
        {
            compileExpr(expr.left);
            uint32_t jump = emitJump(TVM::OP_JMP_IF);
            compileExpr(expr.right);
            patchJump(jump);
        }
        else
        {
            throw std::runtime_error("Unknown logical operator: " + expr.op);
        }
    }

    void Compiler::compileCall(const CallExpr &expr)
    {
        std::cout << "DEBUG compileCall: " << expr.className << "." << expr.methodName
                  << " (isNative: " << expr.isNative << ")" << std::endl;

        for (const auto &arg : expr.args)
        {
            compileExpr(arg);
        }

        if (expr.isNative)
        {
            std::string fullName = expr.className + "." + expr.methodName;

            if (fullName == "Console.println")
            {
                emit(TVM::OP_PRINTLN);
            }
            else if (fullName == "Console.print")
            {
                emit(TVM::OP_PRINT);
            }
            else if (fullName == "Console.read")
            {
                emit(TVM::OP_READ);
            }
            else
            {
                uint32_t idx = addNativeImport(fullName);
                emit(TVM::OP_CALL_NATIVE, idx);
            }
        }
        else
        {

            std::string functionToCall;

            if (!expr.className.empty())
            {

                functionToCall = expr.className + "_" + expr.methodName;
            }
            else
            {

                functionToCall = expr.methodName;
            }

            std::cout << "  Looking for function: " << functionToCall << std::endl;

            auto it = functionAddrs.find(functionToCall);
            if (it != functionAddrs.end())
            {
                emit(TVM::OP_CALL, it->second);
                return;
            }

            it = functionAddrs.find(expr.methodName);
            if (it != functionAddrs.end())
            {
                emit(TVM::OP_CALL, it->second);
                return;
            }

            std::cerr << "ERROR: Function '" << functionToCall << "' not found!" << std::endl;
            std::cerr << "Available functions:" << std::endl;
            for (const auto &[name, addr] : functionAddrs)
            {
                std::cerr << "  " << name << " @ " << addr << std::endl;
            }

            throw std::runtime_error("Function " + expr.className + "." + expr.methodName + " not found");
        }
    }

    void Compiler::compileArray(const ArrayExpr &expr)
    {

        if (expr.elements.empty())
        {
            throw std::runtime_error("Empty array needs type specification");
        }

        for (const auto &elem : expr.elements)
        {
            compileExpr(elem);
        }

        emitPushInt(expr.elements.size());
        emit(TVM::OP_NEW_ARRAY);
    }

    void Compiler::compileIndex(const IndexExpr &expr)
    {
        compileExpr(expr.array);
        compileExpr(expr.index);
        emit(TVM::OP_LOAD_INDEX);
    }

    void Compiler::emit(TVM::OpCode op, uint32_t operand)
    {
        bytecode.code.push_back(TVM::Instruction(op, operand));
    }

    void Compiler::emitPushInt(int64_t value)
    {
        uint32_t idx = addConstantInt(value);
        emit(TVM::OP_PUSH, idx);
    }

    void Compiler::emitPushFloat(double value)
    {
        uint32_t idx = addConstantFloat(value);
        emit(TVM::OP_PUSH, idx);
    }

    void Compiler::emitPushBool(bool value)
    {
        uint32_t idx = addConstantBool(value);
        emit(TVM::OP_PUSH, idx);
    }

    void Compiler::emitPushString(const std::string &value)
    {
        uint32_t idx = addConstantString(value);
        emit(TVM::OP_PUSH, idx);
    }

    void Compiler::emitPushNil()
    {
        TVM::Constant cst;
        cst.type = TVM::TYPE_NIL;
        uint32_t idx = bytecode.constants.size();
        bytecode.constants.push_back(cst);
        emit(TVM::OP_PUSH, idx);
    }

    uint32_t Compiler::emitJump(TVM::OpCode op)
    {
        emit(op, 0xFFFFFFFF);
        return bytecode.code.size() - 1;
    }

    void Compiler::patchJump(uint32_t jumpAddr)
    {
        bytecode.code[jumpAddr].operand = bytecode.code.size();
    }

    void Compiler::patchJumps(const std::vector<uint32_t> &patches, uint32_t target)
    {
        for (uint32_t addr : patches)
        {
            bytecode.code[addr].operand = target;
        }
    }

    uint32_t Compiler::addConstantInt(int64_t value)
    {
        for (uint32_t i = 0; i < bytecode.constants.size(); i++)
        {
            if (bytecode.constants[i].type == TVM::TYPE_INT &&
                bytecode.constants[i].as.intVal == value)
            {
                return i;
            }
        }
        TVM::Constant cst(value);
        uint32_t idx = bytecode.constants.size();
        bytecode.constants.push_back(cst);
        return idx;
    }

    uint32_t Compiler::addConstantFloat(double value)
    {
        for (uint32_t i = 0; i < bytecode.constants.size(); i++)
        {
            if (bytecode.constants[i].type == TVM::TYPE_FLOAT &&
                bytecode.constants[i].as.floatVal == value)
            {
                return i;
            }
        }
        TVM::Constant cst(value);
        uint32_t idx = bytecode.constants.size();
        bytecode.constants.push_back(cst);
        return idx;
    }

    uint32_t Compiler::addConstantBool(bool value)
    {
        for (uint32_t i = 0; i < bytecode.constants.size(); i++)
        {
            if (bytecode.constants[i].type == TVM::TYPE_BOOL &&
                bytecode.constants[i].as.boolVal == value)
            {
                return i;
            }
        }
        TVM::Constant cst(value);
        uint32_t idx = bytecode.constants.size();
        bytecode.constants.push_back(cst);
        return idx;
    }

    uint32_t Compiler::addConstantString(const std::string &value)
    {

        for (uint32_t i = 0; i < bytecode.strings.size(); i++)
        {
            if (bytecode.strings[i] == value)
            {
                TVM::Constant cst(value, i);
                for (uint32_t j = 0; j < bytecode.constants.size(); j++)
                {
                    if (bytecode.constants[j].type == TVM::TYPE_STRING &&
                        bytecode.constants[j].as.stringIdx == i)
                    {
                        return j;
                    }
                }
            }
        }

        uint32_t strIdx = bytecode.strings.size();
        bytecode.strings.push_back(value);

        TVM::Constant cst(value, strIdx);
        uint32_t constIdx = bytecode.constants.size();
        bytecode.constants.push_back(cst);

        return constIdx;
    }

    uint32_t Compiler::resolveLocal(const std::string &name)
    {
        for (int i = contextStack.size() - 1; i >= 0; i--)
        {
            auto it = contextStack[i].varMap.find(name);
            if (it != contextStack[i].varMap.end())
            {
                return it->second;
            }
        }
        return UINT32_MAX;
    }

    uint32_t Compiler::resolveGlobal(const std::string &name)
    {
        auto it = globalMap.find(name);
        if (it != globalMap.end())
        {
            return it->second;
        }
        return UINT32_MAX;
    }

    uint32_t Compiler::addNativeImport(const std::string &name)
    {
        for (uint32_t i = 0; i < bytecode.nativeImports.size(); i++)
        {
            if (bytecode.nativeImports[i] == name)
            {
                return i;
            }
        }
        uint32_t idx = bytecode.nativeImports.size();
        bytecode.nativeImports.push_back(name);
        return idx;
    }

    void Compiler::compileNewArray(TVM::ValueType elemType, uint32_t size)
    {
        TVM::Constant cst;
        cst.type = elemType;
        cst.as.arrayIdx = size;
        uint32_t constIdx = bytecode.constants.size();
        bytecode.constants.push_back(cst);
        emit(TVM::OP_NEW_ARRAY, constIdx);
    }

}