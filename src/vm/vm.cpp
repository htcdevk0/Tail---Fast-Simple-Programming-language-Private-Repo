#include "vm.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <limits>
#include <iomanip>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#elif defined(_MSC_VER)
#pragma warning(disable : 4100)
#pragma warning(disable : 4189)
#endif

namespace TVM
{

    VM::VM() : program(nullptr), running(false), trace(false), pc(0)
    {
        initNativeFunctions();
    }

    VM::~VM()
    {
        // Cleanup
    }

    void VM::initNativeFunctions()
    {
        // Console functions
        nativeFuncs["Console.println"] = [](VM &vm)
        {
            Value val = vm.pop();
            std::cout << val.toString(vm.program) << std::endl;
            vm.push(Value()); // nil
        };

        nativeFuncs["Console.print"] = [](VM &vm)
        {
            Value val = vm.pop();
            std::cout << val.toString(vm.program);
            vm.push(Value()); // nil
        };

        // System functions
        nativeFuncs["System.command"] = [](VM &vm)
        {
            Value cmd = vm.pop();
            int result = system(cmd.toString(vm.program).c_str());
            vm.push(Value(static_cast<int64_t>(result)));
        };

        nativeFuncs["System.clear"] = [](VM &vm)
        {
#ifdef _WIN32
            system("cls");
#else
            system("clear");
#endif
            vm.push(Value()); // nil
        };

        nativeFuncs["System.pause"] = [](VM &vm)
        {
            Value msg = vm.pop();
            if (msg.type != TYPE_NIL)
            {
                std::cout << msg.toString(vm.program);
            }
            else
            {
                std::cout << "Press Enter to continue...";
            }
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            vm.push(Value()); // nil
        };

        nativeFuncs["System.platform"] = [](VM &vm)
        {
#ifdef _WIN32
            std::string platform = "windows";
#elif __APPLE__
            std::string platform = "macos";
#elif __linux__
            std::string platform = "linux";
#else
            std::string platform = "unknown";
#endif
            uint32_t idx = vm.program->strings.size();
            const_cast<BytecodeFile *>(vm.program)->strings.push_back(platform);
            vm.push(Value(platform, idx));
        };

        nativeFuncs["System.env"] = [](VM &vm)
        {
            Value varName = vm.pop();
            const char *value = std::getenv(varName.toString(vm.program).c_str());
            if (value)
            {
                uint32_t idx = vm.program->strings.size();
                const_cast<BytecodeFile *>(vm.program)->strings.push_back(value);
                vm.push(Value(value, idx));
            }
            else
            {
                vm.push(Value()); // nil
            }
        };

        // IO functions
        nativeFuncs["IO.input"] = [](VM &vm)
        {
            Value prompt = vm.pop();
            if (prompt.type != TYPE_NIL)
            {
                std::cout << prompt.toString(vm.program);
            }
            std::string input;
            std::getline(std::cin, input);
            uint32_t idx = vm.program->strings.size();
            const_cast<BytecodeFile *>(vm.program)->strings.push_back(input);
            vm.push(Value(input, idx));
        };

        nativeFuncs["IO.toInt"] = [](VM &vm)
        {
            Value strVal = vm.pop();
            try
            {
                int64_t value = std::stoll(strVal.toString(vm.program));
                vm.push(Value(value));
            }
            catch (...)
            {
                vm.runtimeError("Failed to convert string to int");
            }
        };

        nativeFuncs["IO.toFloat"] = [](VM &vm)
        {
            Value strVal = vm.pop();
            try
            {
                double value = std::stod(strVal.toString(vm.program));
                vm.push(Value(value));
            }
            catch (...)
            {
                vm.runtimeError("Failed to convert string to float");
            }
        };

        // Str functions
        nativeFuncs["Str.array"] = [](VM &vm)
        {
            // Get number of arguments from stack
            // This is simplified - would need proper implementation
            vm.push(Value()); // nil for now
        };

        nativeFuncs["Str.length"] = [](VM &vm)
        {
            Value arr = vm.pop();
            (void)arr;
            vm.push(Value(static_cast<int64_t>(0)));
        };
        // Random functions (simplified)
        nativeFuncs["Random.int"] = [](VM &vm)
        {
            // Simplified random
            static int seed = 12345;
            seed = (seed * 1103515245 + 12345) & 0x7fffffff;
            vm.push(Value(static_cast<int64_t>(seed % 100)));
        };
    }

    void VM::execute(BytecodeFile &bytecode)
    {
        program = &bytecode;
        running = true;
        pc = 0;

        globals.clear();
        stack.clear();
        callStack.clear();
        locals.clear();

        const FunctionInfo *mainFunc = nullptr;
        for (const auto &func : program->functions)
        {
            if (func.name == "Main")
            {
                mainFunc = &func;
                break;
            }
        }

        if (!mainFunc)
        {
            throw std::runtime_error("Main function not found");
        }

        CallFrame frame;
        frame.returnAddr = UINT32_MAX;
        frame.localStart = 0;
        frame.argCount = 0;
        frame.func = mainFunc;
        callStack.push_back(frame);

        locals.resize(mainFunc->locals);
        pc = mainFunc->address;

        try
        {
            while (running && pc < program->code.size())
            {
                auto &instr = program->code[pc];

                if (trace)
                {
                    traceInstruction(instr);
                    traceStack();
                }

                executeInstruction(instr);

                if (instr.opcode != OP_JMP &&
                    instr.opcode != OP_JMP_IF &&
                    instr.opcode != OP_JMP_IFNOT &&
                    instr.opcode != OP_CALL &&
                    instr.opcode != OP_RET)
                {
                    pc++;
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "VM Runtime Error: " << e.what() << std::endl;
            dumpState();
            throw;
        }
    }

    void VM::executeInstruction(Instruction &instr)
    {
        switch (instr.opcode)
        {
        // Stack operations
        case OP_PUSH:
        {
            const auto &cst = getConstant(instr.operand);
            Value val;
            val.type = cst.type;

            switch (cst.type)
            {
            case TYPE_INT:
                val.as.intVal = cst.as.intVal;
                break;
            case TYPE_FLOAT:
                val.as.floatVal = cst.as.floatVal;
                break;
            case TYPE_BOOL:
                val.as.boolVal = cst.as.boolVal;
                break;
            case TYPE_STRING:
            case TYPE_ARRAY_INT:
            case TYPE_ARRAY_FLOAT:
            case TYPE_ARRAY_STRING:
                val.as.stringIdx = cst.as.stringIdx;
                break;
            case TYPE_NIL:
                val.as.intVal = 0;
                break;
            }
            push(val);
            break;
        }
        case OP_POP:
            pop();
            break;
        case OP_DUP:
        {
            Value top = peek();
            push(top);
            break;
        }
        case OP_SWAP:
        {
            Value a = pop();
            Value b = pop();
            push(a);
            push(b);
            break;
        }

        // Arithmetic
        case OP_ADD:
            opAdd();
            break;
        case OP_SUB:
            opSub();
            break;
        case OP_MUL:
            opMul();
            break;
        case OP_DIV:
            opDiv();
            break;
        case OP_MOD:
            opMod();
            break;
        case OP_NEG:
            opNeg();
            break;
        case OP_INC:
            opInc();
            break;
        case OP_DEC:
            opDec();
            break;

        // Comparisons
        case OP_EQ:
            opEq();
            break;
        case OP_NEQ:
            opNeq();
            break;
        case OP_LT:
            opLt();
            break;
        case OP_LTE:
            opLte();
            break;
        case OP_GT:
            opGt();
            break;
        case OP_GTE:
            opGte();
            break;

        // Logic
        case OP_AND:
            opAnd();
            break;
        case OP_OR:
            opOr();
            break;
        case OP_NOT:
            opNot();
            break;

        // Variables
        case OP_LOAD:
            opLoad(instr.operand);
            break;
        case OP_STORE:
            opStore(instr.operand);
            break;
        case OP_LOAD_GLOBAL:
            opLoadGlobal(instr.operand);
            break;
        case OP_STORE_GLOBAL:
            opStoreGlobal(instr.operand);
            break;

        // Control flow
        case OP_JMP:
            opJmp(instr.operand);
            break;
        case OP_JMP_IF:
            opJmpIf(instr.operand);
            break;
        case OP_JMP_IFNOT:
            opJmpIfNot(instr.operand);
            break;
        case OP_CALL:
            callFunction(instr.operand);
            break;
        case OP_RET:
            returnFromFunction();
            break;
        case OP_CALL_NATIVE:
            callNative(instr.operand);
            break;

        // Arrays
        case OP_NEW_ARRAY:
            opNewArray(instr.operand);
            break;
        case OP_LOAD_INDEX:
            opLoadIndex();
            break;
        case OP_STORE_INDEX:
            opStoreIndex();
            break;
        case OP_ARRAY_LEN:
            opArrayLen();
            break;

        // I/O
        case OP_PRINT:
            opPrint();
            break;
        case OP_READ:
            opRead();
            break;
        case OP_PRINTLN:
            opPrintln();
            break;

        // System
        case OP_HALT:
            opHalt();
            break;

        default:
            runtimeError("Unknown opcode: " + std::to_string(static_cast<int>(instr.opcode)));
        }
    }

    Value VM::pop()
    {
        if (stack.empty())
        {
            runtimeError("Stack underflow");
        }
        Value val = stack.back();
        stack.pop_back();
        return val;
    }

    void VM::push(const Value &val)
    {
        stack.push_back(val);
    }

    Value &VM::peek(int offset)
    {
        if (offset >= (int)stack.size())
        {
            runtimeError("Stack peek out of bounds");
        }
        return stack[stack.size() - 1 - offset];
    }

    const Constant &VM::getConstant(uint32_t index) const
    {
        if (index >= program->constants.size())
        {
            runtimeError("Constant index out of bounds");
        }
        return program->constants[index];
    }

    const std::string &VM::getString(uint32_t index) const
    {
        if (index >= program->strings.size())
        {
            runtimeError("String index out of bounds");
        }
        return program->strings[index];
    }
    void VM::opAdd()
    {
        Value b = pop();
        Value a = pop();

        if (a.type == TYPE_INT && b.type == TYPE_INT)
        {
            push(Value(a.as.intVal + b.as.intVal));
            return;
        }

        if (a.type == TYPE_FLOAT && b.type == TYPE_FLOAT)
        {
            push(Value(a.as.floatVal + b.as.floatVal));
            return;
        }

        if (a.type == TYPE_INT && b.type == TYPE_FLOAT)
        {
            push(Value(static_cast<double>(a.as.intVal) + b.as.floatVal));
            return;
        }

        if (a.type == TYPE_FLOAT && b.type == TYPE_INT)
        {
            push(Value(a.as.floatVal + static_cast<double>(b.as.intVal)));
            return;
        }

        if (a.type == TYPE_STRING || b.type == TYPE_STRING)
        {
            std::string result = a.toString(program) + b.toString(program);
            uint32_t idx = program->strings.size();
            const_cast<BytecodeFile *>(program)->strings.push_back(result);
            push(Value(result, idx));
            return;
        }

        push(Value());
    }

    void VM::opSub()
    {
        Value b = pop();
        Value a = pop();

        if (a.type == TYPE_INT && b.type == TYPE_INT)
        {
            push(Value(a.as.intVal - b.as.intVal));
        }
        else if (a.type == TYPE_FLOAT && b.type == TYPE_FLOAT)
        {
            push(Value(a.as.floatVal - b.as.floatVal));
        }
        else
        {
            runtimeError("Invalid types for subtraction");
        }
    }

    void VM::opMul()
    {
        Value b = pop();
        Value a = pop();

        if (a.type == TYPE_INT && b.type == TYPE_INT)
        {
            push(Value(a.as.intVal * b.as.intVal));
        }
        else if (a.type == TYPE_FLOAT && b.type == TYPE_FLOAT)
        {
            push(Value(a.as.floatVal * b.as.floatVal));
        }
        else
        {
            runtimeError("Invalid types for multiplication");
        }
    }

    void VM::opDiv()
    {
        Value b = pop();
        Value a = pop();

        if (b.type == TYPE_INT && b.as.intVal == 0)
        {
            runtimeError("Division by zero");
        }
        if (b.type == TYPE_FLOAT && b.as.floatVal == 0.0)
        {
            runtimeError("Division by zero");
        }

        if (a.type == TYPE_INT && b.type == TYPE_INT)
        {
            push(Value(a.as.intVal / b.as.intVal));
        }
        else if (a.type == TYPE_FLOAT && b.type == TYPE_FLOAT)
        {
            push(Value(a.as.floatVal / b.as.floatVal));
        }
        else
        {
            runtimeError("Invalid types for division");
        }
    }

    void VM::opMod()
    {
        Value b = pop();
        Value a = pop();

        if (a.type == TYPE_INT && b.type == TYPE_INT)
        {
            if (b.as.intVal == 0)
            {
                runtimeError("Modulo by zero");
            }
            push(Value(a.as.intVal % b.as.intVal));
        }
        else
        {
            runtimeError("Invalid types for modulo");
        }
    }

    void VM::opNeg()
    {
        Value a = pop();

        if (a.type == TYPE_INT)
        {
            push(Value(-a.as.intVal));
        }
        else if (a.type == TYPE_FLOAT)
        {
            push(Value(-a.as.floatVal));
        }
        else
        {
            runtimeError("Invalid type for negation");
        }
    }

    void VM::opInc()
    {
        Value &a = peek();

        if (a.type == TYPE_INT)
        {
            a.as.intVal++;
        }
        else if (a.type == TYPE_FLOAT)
        {
            a.as.floatVal += 1.0;
        }
        else
        {
            runtimeError("Invalid type for increment");
        }
    }

    void VM::opDec()
    {
        Value &a = peek();

        if (a.type == TYPE_INT)
        {
            a.as.intVal--;
        }
        else if (a.type == TYPE_FLOAT)
        {
            a.as.floatVal -= 1.0;
        }
        else
        {
            runtimeError("Invalid type for decrement");
        }
    }

    void VM::opEq()
    {
        Value b = pop();
        Value a = pop();
        push(Value(a.toString(program) == b.toString(program)));
    }

    void VM::opNeq()
    {
        Value b = pop();
        Value a = pop();
        push(Value(a.toString(program) != b.toString(program)));
    }

    void VM::opLt()
    {
        Value b = pop();
        Value a = pop();

        if (a.type == TYPE_INT && b.type == TYPE_INT)
        {
            push(Value(a.as.intVal < b.as.intVal));
        }
        else if (a.type == TYPE_FLOAT && b.type == TYPE_FLOAT)
        {
            push(Value(a.as.floatVal < b.as.floatVal));
        }
        else
        {
            runtimeError("Invalid types for comparison");
        }
    }

    void VM::opLte()
    {
        Value b = pop();
        Value a = pop();

        if (a.type == TYPE_INT && b.type == TYPE_INT)
        {
            push(Value(a.as.intVal <= b.as.intVal));
        }
        else if (a.type == TYPE_FLOAT && b.type == TYPE_FLOAT)
        {
            push(Value(a.as.floatVal <= b.as.floatVal));
        }
        else
        {
            runtimeError("Invalid types for comparison");
        }
    }

    void VM::opGt()
    {
        Value b = pop();
        Value a = pop();

        if (a.type == TYPE_INT && b.type == TYPE_INT)
        {
            push(Value(a.as.intVal > b.as.intVal));
        }
        else if (a.type == TYPE_FLOAT && b.type == TYPE_FLOAT)
        {
            push(Value(a.as.floatVal > b.as.floatVal));
        }
        else
        {
            runtimeError("Invalid types for comparison");
        }
    }

    void VM::opGte()
    {
        Value b = pop();
        Value a = pop();

        if (a.type == TYPE_INT && b.type == TYPE_INT)
        {
            push(Value(a.as.intVal >= b.as.intVal));
        }
        else if (a.type == TYPE_FLOAT && b.type == TYPE_FLOAT)
        {
            push(Value(a.as.floatVal >= b.as.floatVal));
        }
        else
        {
            runtimeError("Invalid types for comparison");
        }
    }

    void VM::opAnd()
    {
        Value b = pop();
        Value a = pop();
        push(Value(a.isTruthy() && b.isTruthy()));
    }

    void VM::opOr()
    {
        Value b = pop();
        Value a = pop();
        push(Value(a.isTruthy() || b.isTruthy()));
    }

    void VM::opNot()
    {
        Value a = pop();
        push(Value(!a.isTruthy()));
    }

    void VM::opLoad(uint32_t index)
    {
        if (index >= locals.size())
        {
            runtimeError("Local variable index out of bounds");
        }
        push(locals[index]);
    }

    void VM::opStore(uint32_t index)
    {
        if (index >= locals.size())
        {
            runtimeError("Local variable index out of bounds");
        }
        locals[index] = peek();
    }

    void VM::opLoadGlobal(uint32_t index)
    {
        if (index >= globals.size())
        {
            // Expand globals if needed
            globals.resize(index + 1);
        }
        push(globals[index]);
    }

    void VM::opStoreGlobal(uint32_t index)
    {
        if (index >= globals.size())
        {
            globals.resize(index + 1);
        }
        globals[index] = peek();
    }

    void VM::opJmp(uint32_t address)
    {
        if (address >= program->code.size())
        {
            runtimeError("Jump address out of bounds");
        }
        pc = address;
    }

    void VM::opJmpIf(uint32_t address)
    {
        Value cond = pop();
        if (cond.isTruthy())
        {
            opJmp(address);
        }
    }

    void VM::opJmpIfNot(uint32_t address)
    {
        Value cond = pop();
        if (!cond.isTruthy())
        {
            opJmp(address);
        }
    }

    void VM::callFunction(uint32_t funcIndex)
    {
        const FunctionInfo *func = nullptr;
        for (const auto &f : program->functions)
        {
            if (f.address == funcIndex)
            {
                func = &f;
                break;
            }
        }

        if (!func)
        {
            runtimeError("Function not found at address: " + std::to_string(funcIndex));
        }

        if (stack.size() < func->arity)
        {
            runtimeError("Not enough arguments for function " + func->name);
        }

        CallFrame frame;
        frame.returnAddr = pc + 1;
        frame.localStart = locals.size();
        frame.argCount = func->arity;
        frame.func = func;
        callStack.push_back(frame);

        locals.resize(frame.localStart + func->locals);

        for (int i = func->arity - 1; i >= 0; i--)
        {
            if (!stack.empty())
            {
                locals[frame.localStart + i] = pop();
            }
            else
            {
                locals[frame.localStart + i] = Value();
            }
        }

        for (int i = 0; i < func->arity; i++)
        {
            if (!stack.empty())
            {
                stack.pop_back();
            }
        }

        pc = func->address;
    }

    void VM::returnFromFunction()
    {
        if (callStack.empty())
        {
            running = false;
            return;
        }

        CallFrame frame = callStack.back();
        callStack.pop_back();

        if (frame.returnAddr == UINT32_MAX)
        {
            running = false;
            return;
        }

        Value returnValue;
        if (!stack.empty())
        {
            returnValue = pop();
        }

        locals.resize(frame.localStart);
        pc = frame.returnAddr;
        push(returnValue);
    }

    void VM::callNative(uint32_t nativeIndex)
    {
        if (nativeIndex >= program->nativeImports.size())
        {
            runtimeError("Native import index out of bounds");
        }

        const std::string &name = program->nativeImports[nativeIndex];
        auto it = nativeFuncs.find(name);
        if (it == nativeFuncs.end())
        {
            runtimeError("Native function not implemented: " + name);
        }

        // Call native function
        it->second(*this);
    }

    void VM::opNewArray(uint32_t constIndex)
    {
        // Simplified array creation
        // In real implementation, would create array based on type in constant
        (void)constIndex;
        push(Value()); // nil for now
    }

    void VM::opLoadIndex()
    {
        Value index = pop();
        Value array = pop();
        (void)array;

        if (index.type != TYPE_INT)
        {
            runtimeError("Array index must be integer");
        }

        // Simplified - always return nil
        push(Value());
    }

    void VM::opStoreIndex()
    {
        Value value = pop();
        Value index = pop();
        Value array = pop();
        (void)array;

        if (index.type != TYPE_INT)
        {
            runtimeError("Array index must be integer");
        }

        // Simplified - do nothing
        push(value);
    }

    void VM::opArrayLen()
    {
        Value array = pop();
        // Simplified - always return 0
        (void)array;
        push(Value(static_cast<int64_t>(0)));
    }

    void VM::opPrint()
    {
        Value val = pop();
        std::cout << val.toString(program);
    }

    void VM::opRead()
    {
        std::string input;
        std::getline(std::cin, input);
        uint32_t idx = program->strings.size();
        const_cast<BytecodeFile *>(program)->strings.push_back(input);
        push(Value(input, idx));
    }

    void VM::opPrintln()
    {
        Value val = pop();
        std::cout << val.toString(program) << std::endl;
    }

    void VM::opHalt()
    {
        running = false;
    }

    void VM::runtimeError(const std::string &message) const
    {
        std::stringstream ss;
        ss << "Runtime error at PC=" << pc << ": " << message;
        throw std::runtime_error(ss.str());
    }

    void VM::traceInstruction(Instruction &instr)
    {
        std::cout << "PC=" << std::setw(4) << pc << ": ";

        switch (instr.opcode)
        {
        case OP_PUSH:
        {
            const auto &cst = getConstant(instr.operand);
            Value val;
            val.type = cst.type;

            switch (cst.type)
            {
            case TYPE_INT:
                val.as.intVal = cst.as.intVal;
                break;
            case TYPE_FLOAT:
                val.as.floatVal = cst.as.floatVal;
                break;
            case TYPE_BOOL:
                val.as.boolVal = cst.as.boolVal;
                break;
            case TYPE_STRING:
            case TYPE_ARRAY_INT:
            case TYPE_ARRAY_FLOAT:
            case TYPE_ARRAY_STRING:
                val.as.stringIdx = cst.as.stringIdx;
                break;
            case TYPE_NIL:
                val.as.intVal = 0;
                break;
            }
            push(val);
            break;
        }
        case OP_POP:
            std::cout << "POP";
            break;
        case OP_ADD:
            std::cout << "ADD";
            break;
        case OP_SUB:
            std::cout << "SUB";
            break;
        case OP_MUL:
            std::cout << "MUL";
            break;
        case OP_DIV:
            std::cout << "DIV";
            break;
        case OP_MOD:
            std::cout << "MOD";
            break;
        case OP_LOAD:
            std::cout << "LOAD " << instr.operand;
            break;
        case OP_STORE:
            std::cout << "STORE " << instr.operand;
            break;
        case OP_JMP:
            std::cout << "JMP " << instr.operand;
            break;
        case OP_JMP_IF:
            std::cout << "JMP_IF " << instr.operand;
            break;
        case OP_JMP_IFNOT:
            std::cout << "JMP_IFNOT " << instr.operand;
            break;
        case OP_CALL:
            std::cout << "CALL " << instr.operand;
            break;
        case OP_RET:
            std::cout << "RET";
            break;
        case OP_CALL_NATIVE:
            std::cout << "CALL_NATIVE " << instr.operand;
            break;
        case OP_PRINT:
            std::cout << "PRINT";
            break;
        case OP_PRINTLN:
            std::cout << "PRINTLN";
            break;
        case OP_HALT:
            std::cout << "HALT";
            break;
        default:
            std::cout << "UNKNOWN(" << (int)instr.opcode << ")";
            break;
        }

        std::cout << std::endl;
    }

    void VM::traceStack() const
    {
        std::cout << "  Stack [" << stack.size() << "]: ";
        for (const auto &val : stack)
        {
            std::cout << val.toString(program) << " ";
        }
        std::cout << std::endl;
    }

    void VM::dumpState()
    {
        std::cout << "\n=== VM State Dump ===" << std::endl;
        std::cout << "PC: " << pc << std::endl;
        std::cout << "Running: " << (running ? "yes" : "no") << std::endl;
        std::cout << "Call stack depth: " << callStack.size() << std::endl;
        std::cout << "Locals: " << locals.size() << std::endl;
        std::cout << "Globals: " << globals.size() << std::endl;

        std::cout << "\nStack (" << stack.size() << " items):" << std::endl;
        for (int i = stack.size() - 1; i >= 0; i--)
        {
            std::cout << "  [" << i << "] " << stack[i].toString(program) << std::endl;
        }

        if (pc < program->code.size())
        {
            std::cout << "\nNext instruction:" << std::endl;
            traceInstruction(program->code[pc]);
        }
    }
} // namespace TVM