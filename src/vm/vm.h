#pragma once
#include "../shared/bytecode.h"
#include <vector>
#include <stack>
#include <map>
#include <functional>
#include <string>
#include <memory>
#include <iostream>

namespace TVM {

class VM {
public:
    VM();
    ~VM();
    
    void execute(BytecodeFile& bytecode);
    
    bool isRunning() const { return running; }
    void stop() { running = false; }
    
    // Debug
    void setTrace(bool enable) { trace = enable; }
    void dumpState();
    
private:
    BytecodeFile* program;
    bool running;
    bool trace;
    
    uint32_t pc;                    // Program counter
    std::vector<Value> stack;       // Value stack
    std::vector<Value> globals;     // Global variables
    std::vector<Value> locals;      // Local variables (current frame)
    
    struct CallFrame {
        uint32_t returnAddr;
        uint32_t localStart;
        uint32_t argCount;
        const TVM::FunctionInfo* func;
    };
    
    std::vector<CallFrame> callStack;
    
    std::map<std::string, std::function<void(VM&)>> nativeFuncs;
    
    void initNativeFunctions();
    
    Value pop();
    void push(const Value& val);
    Value& peek(int offset = 0);
    
    const Constant& getConstant(uint32_t index) const;
    const std::string& getString(uint32_t index) const;
    
    void executeInstruction(Instruction& instr);
    void callFunction(uint32_t funcIndex);
    void returnFromFunction();
    void callNative(uint32_t nativeIndex);
    
    void opAdd();
    void opSub();
    void opMul();
    void opDiv();
    void opMod();
    void opNeg();
    void opInc();
    void opDec();
    
    void opEq();
    void opNeq();
    void opLt();
    void opLte();
    void opGt();
    void opGte();
    
    void opAnd();
    void opOr();
    void opNot();
    
    void opLoad(uint32_t index);
    void opStore(uint32_t index);
    void opLoadGlobal(uint32_t index);
    void opStoreGlobal(uint32_t index);
    
    void opJmp(uint32_t address);
    void opJmpIf(uint32_t address);
    void opJmpIfNot(uint32_t address);
    
    // Arrays
    void opNewArray(uint32_t constIndex);
    void opLoadIndex();
    void opStoreIndex();
    void opArrayLen();
    
    // I/O
    void opPrint();
    void opRead();
    void opPrintln();
    
    void opHalt();
    
    // Runtime errors
    void runtimeError(const std::string& message) const;
    
    // Debug
    void traceInstruction(Instruction& instr);
    void traceStack() const;
};

} // namespace TVM