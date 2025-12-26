#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <memory>

namespace TVM
{

    enum OpCode : uint8_t
    {
        // Stack operations
        OP_PUSH = 0x01,
        OP_POP = 0x02,
        OP_DUP = 0x03,
        OP_SWAP = 0x04,

        // Arithmetic
        OP_ADD = 0x10,
        OP_SUB = 0x11,
        OP_MUL = 0x12,
        OP_DIV = 0x13,
        OP_MOD = 0x14,
        OP_NEG = 0x15,
        OP_INC = 0x16,
        OP_DEC = 0x17,

        // Comparisons
        OP_EQ = 0x20,
        OP_NEQ = 0x21,
        OP_LT = 0x22,
        OP_LTE = 0x23,
        OP_GT = 0x24,
        OP_GTE = 0x25,

        // Logic
        OP_AND = 0x30,
        OP_OR = 0x31,
        OP_NOT = 0x32,

        // Variables
        OP_LOAD = 0x40,
        OP_STORE = 0x41,
        OP_LOAD_GLOBAL = 0x42,
        OP_STORE_GLOBAL = 0x43,

        // Control flow
        OP_JMP = 0x50,
        OP_JMP_IF = 0x51,
        OP_JMP_IFNOT = 0x52,
        OP_CALL = 0x53,
        OP_RET = 0x54,
        OP_CALL_NATIVE = 0x55,

        // Arrays
        OP_NEW_ARRAY = 0x60,
        OP_LOAD_INDEX = 0x61,
        OP_STORE_INDEX = 0x62,
        OP_ARRAY_LEN = 0x63,

        // I/O
        OP_PRINT = 0x70,
        OP_READ = 0x71,
        OP_PRINTLN = 0x72,

        // System
        OP_HALT = 0xFF
    };

    enum ValueType : uint8_t
    {
        TYPE_NIL = 0,
        TYPE_INT = 1,
        TYPE_FLOAT = 2,
        TYPE_BOOL = 3,
        TYPE_STRING = 4,
        TYPE_ARRAY_INT = 5,
        TYPE_ARRAY_FLOAT = 6,
        TYPE_ARRAY_STRING = 7
    };

    struct Constant
    {
        ValueType type;
        union
        {
            int64_t intVal;
            double floatVal;
            bool boolVal;
            uint32_t stringIdx;
            uint32_t arrayIdx;
        } as;

        Constant() : type(TYPE_NIL) { as.intVal = 0; }
        Constant(int64_t v) : type(TYPE_INT) { as.intVal = v; }
        Constant(double v) : type(TYPE_FLOAT) { as.floatVal = v; }
        Constant(bool v) : type(TYPE_BOOL) { as.boolVal = v; }
        Constant(const std::string&, uint32_t idx) : type(TYPE_STRING) { as.stringIdx = idx; }
    };

    struct Instruction
    {
        OpCode opcode;
        uint32_t operand;

        Instruction() : opcode(OP_HALT), operand(0) {}
        Instruction(OpCode op, uint32_t opnd = 0) : opcode(op), operand(opnd) {}
    };

    struct FunctionInfo
    {
        std::string name;
        uint32_t address;
        uint8_t arity;
        uint8_t locals;

        FunctionInfo() : address(0), arity(0), locals(0) {}
        FunctionInfo(const std::string &n, uint32_t addr, uint8_t a, uint8_t l)
            : name(n), address(addr), arity(a), locals(l) {}
    };

    struct BytecodeFile
    {
        // Header
        uint32_t magic = 0x5441494C; // "TAIL"
        uint16_t version = 1;
        uint16_t flags = 0;

        // Code section
        std::vector<Instruction> code;

        // Data section
        std::vector<Constant> constants;
        std::vector<std::string> strings;
        std::vector<std::vector<int64_t>> intArrays;
        std::vector<std::vector<double>> floatArrays;
        std::vector<std::vector<std::string>> stringArrays;

        // Function table
        std::vector<FunctionInfo> functions;

        // Native imports
        std::vector<std::string> nativeImports;

        // Serialization
        std::vector<uint8_t> serialize() const;
        bool deserialize(const std::vector<uint8_t> &data);

        // Debug
        void dump() const;
    };

    // Value for VM runtime
    class Value
    {
    public:
        ValueType type;
        union
        {
            int64_t intVal;
            double floatVal;
            bool boolVal;
            uint32_t stringIdx;
            uint32_t arrayIdx;
        } as;

        Value() : type(TYPE_NIL) { as.intVal = 0; }
        Value(int64_t v) : type(TYPE_INT) { as.intVal = v; }
        Value(double v) : type(TYPE_FLOAT) { as.floatVal = v; }
        Value(bool v) : type(TYPE_BOOL) { as.boolVal = v; }
        Value(const std::string&, uint32_t idx) : type(TYPE_STRING) { as.stringIdx = idx; }
        Value(uint32_t idx, ValueType t) : type(t) { as.arrayIdx = idx; }

        std::string toString(const BytecodeFile *prog = nullptr) const;
        bool isTruthy() const;
    };

}