#include "bytecode.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>

namespace TVM {

// Helper functions for proper endianness
static void writeUint32(std::vector<uint8_t>& data, uint32_t value) {
    data.push_back(static_cast<uint8_t>(value & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

static void writeUint16(std::vector<uint8_t>& data, uint16_t value) {
    data.push_back(static_cast<uint8_t>(value & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

static void writeInt64(std::vector<uint8_t>& data, int64_t value) {
    for (int i = 0; i < 8; i++) {
        data.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

static void writeDouble(std::vector<uint8_t>& data, double value) {
    // Copy bytes directly to avoid endianness issues with memcpy
    uint8_t bytes[8];
    memcpy(bytes, &value, 8);
    for (int i = 0; i < 8; i++) {
        data.push_back(bytes[i]);
    }
}

static uint32_t readUint32(const uint8_t*& ptr) {
    uint32_t result = 
        static_cast<uint32_t>(ptr[0]) |
        (static_cast<uint32_t>(ptr[1]) << 8) |
        (static_cast<uint32_t>(ptr[2]) << 16) |
        (static_cast<uint32_t>(ptr[3]) << 24);
    ptr += 4;
    return result;
}

static uint16_t readUint16(const uint8_t*& ptr) {
    uint16_t result = 
        static_cast<uint16_t>(ptr[0]) |
        (static_cast<uint16_t>(ptr[1]) << 8);
    ptr += 2;
    return result;
}

static int64_t readInt64(const uint8_t*& ptr) {
    int64_t result = 0;
    for (int i = 0; i < 8; i++) {
        result |= static_cast<int64_t>(ptr[i]) << (i * 8);
    }
    ptr += 8;
    return result;
}

static double readDouble(const uint8_t*& ptr) {
    double result;
    memcpy(&result, ptr, 8);
    ptr += 8;
    return result;
}

std::vector<uint8_t> BytecodeFile::serialize() const {
    std::vector<uint8_t> data;
    
    // Header - always little-endian
    writeUint32(data, magic);          // "TAIL" = 0x5441494C
    writeUint16(data, version);        // Version 1
    writeUint16(data, flags);          // Flags 0
    
    // Code section
    writeUint32(data, static_cast<uint32_t>(code.size()));
    for (const auto& instr : code) {
        data.push_back(static_cast<uint8_t>(instr.opcode));
        writeUint32(data, instr.operand);
    }
    
    // Constants
    writeUint32(data, static_cast<uint32_t>(constants.size()));
    for (const auto& cst : constants) {
        data.push_back(static_cast<uint8_t>(cst.type));
        switch (cst.type) {
            case TYPE_INT:
                writeInt64(data, cst.as.intVal);
                break;
            case TYPE_FLOAT:
                writeDouble(data, cst.as.floatVal);
                break;
            case TYPE_BOOL:
                data.push_back(cst.as.boolVal ? 1 : 0);
                break;
            case TYPE_STRING:
            case TYPE_ARRAY_INT:
            case TYPE_ARRAY_FLOAT:
            case TYPE_ARRAY_STRING:
                writeUint32(data, cst.as.stringIdx);
                break;
            case TYPE_NIL:
                // For nil, write 8 zero bytes
                for (int i = 0; i < 8; i++) {
                    data.push_back(0);
                }
                break;
            default:
                // Unknown type, write zeros
                for (int i = 0; i < 8; i++) {
                    data.push_back(0);
                }
                break;
        }
    }
    
    // Strings
    writeUint32(data, static_cast<uint32_t>(strings.size()));
    for (const auto& str : strings) {
        writeUint32(data, static_cast<uint32_t>(str.size()));
        data.insert(data.end(), str.begin(), str.end());
    }
    
    // Int arrays
    writeUint32(data, static_cast<uint32_t>(intArrays.size()));
    for (const auto& arr : intArrays) {
        writeUint32(data, static_cast<uint32_t>(arr.size()));
        for (int64_t val : arr) {
            writeInt64(data, val);
        }
    }
    
    // Float arrays
    writeUint32(data, static_cast<uint32_t>(floatArrays.size()));
    for (const auto& arr : floatArrays) {
        writeUint32(data, static_cast<uint32_t>(arr.size()));
        for (double val : arr) {
            writeDouble(data, val);
        }
    }
    
    // String arrays
    writeUint32(data, static_cast<uint32_t>(stringArrays.size()));
    for (const auto& arr : stringArrays) {
        writeUint32(data, static_cast<uint32_t>(arr.size()));
        for (const auto& str : arr) {
            writeUint32(data, static_cast<uint32_t>(str.size()));
            data.insert(data.end(), str.begin(), str.end());
        }
    }
    
    // Functions
    writeUint32(data, static_cast<uint32_t>(functions.size()));
    for (const auto& func : functions) {
        writeUint32(data, static_cast<uint32_t>(func.name.size()));
        data.insert(data.end(), func.name.begin(), func.name.end());
        writeUint32(data, func.address);
        data.push_back(func.arity);
        data.push_back(func.locals);
    }
    
    // Native imports
    writeUint32(data, static_cast<uint32_t>(nativeImports.size()));
    for (const auto& native : nativeImports) {
        writeUint32(data, static_cast<uint32_t>(native.size()));
        data.insert(data.end(), native.begin(), native.end());
    }
    
    return data;
}

bool BytecodeFile::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 8) return false;
    
    const uint8_t* ptr = data.data();
    const uint8_t* end = ptr + data.size();
    
    // Header
    if (ptr + 8 > end) return false;
    magic = readUint32(ptr);
    if (magic != 0x5441494C) return false; // "TAIL"
    version = readUint16(ptr);
    flags = readUint16(ptr);
    
    // Code section
    if (ptr + 4 > end) return false;
    uint32_t codeSize = readUint32(ptr);
    if (ptr + codeSize * 5 > end) return false; // 1 byte opcode + 4 bytes operand per instruction
    
    code.resize(codeSize);
    for (uint32_t i = 0; i < codeSize; i++) {
        if (ptr + 1 > end) return false;
        code[i].opcode = static_cast<OpCode>(*ptr++);
        code[i].operand = readUint32(ptr);
    }
    
    // Constants
    if (ptr + 4 > end) return false;
    uint32_t constCount = readUint32(ptr);
    constants.resize(constCount);
    
    for (uint32_t i = 0; i < constCount; i++) {
        if (ptr + 1 > end) return false;
        constants[i].type = static_cast<ValueType>(*ptr++);
        
        switch (constants[i].type) {
            case TYPE_INT:
                if (ptr + 8 > end) return false;
                constants[i].as.intVal = readInt64(ptr);
                break;
            case TYPE_FLOAT:
                if (ptr + 8 > end) return false;
                constants[i].as.floatVal = readDouble(ptr);
                break;
            case TYPE_BOOL:
                if (ptr + 1 > end) return false;
                constants[i].as.boolVal = (*ptr++ != 0);
                break;
            case TYPE_STRING:
            case TYPE_ARRAY_INT:
            case TYPE_ARRAY_FLOAT:
            case TYPE_ARRAY_STRING:
                if (ptr + 4 > end) return false;
                constants[i].as.stringIdx = readUint32(ptr);
                break;
            case TYPE_NIL:
                // Skip 8 bytes for nil
                if (ptr + 8 > end) return false;
                ptr += 8;
                break;
            default:
                // Unknown type, skip 8 bytes
                if (ptr + 8 > end) return false;
                ptr += 8;
                break;
        }
    }
    
    // Strings
    if (ptr + 4 > end) return false;
    uint32_t strCount = readUint32(ptr);
    strings.resize(strCount);
    
    for (uint32_t i = 0; i < strCount; i++) {
        if (ptr + 4 > end) return false;
        uint32_t len = readUint32(ptr);
        if (ptr + len > end) return false;
        
        strings[i].resize(len);
        for (uint32_t j = 0; j < len; j++) {
            strings[i][j] = static_cast<char>(ptr[j]);
        }
        ptr += len;
    }
    
    // Int arrays
    if (ptr + 4 > end) return false;
    uint32_t intArrayCount = readUint32(ptr);
    intArrays.resize(intArrayCount);
    
    for (uint32_t i = 0; i < intArrayCount; i++) {
        if (ptr + 4 > end) return false;
        uint32_t len = readUint32(ptr);
        
        intArrays[i].resize(len);
        for (uint32_t j = 0; j < len; j++) {
            if (ptr + 8 > end) return false;
            intArrays[i][j] = readInt64(ptr);
        }
    }
    
    // Float arrays
    if (ptr + 4 > end) return false;
    uint32_t floatArrayCount = readUint32(ptr);
    floatArrays.resize(floatArrayCount);
    
    for (uint32_t i = 0; i < floatArrayCount; i++) {
        if (ptr + 4 > end) return false;
        uint32_t len = readUint32(ptr);
        
        floatArrays[i].resize(len);
        for (uint32_t j = 0; j < len; j++) {
            if (ptr + 8 > end) return false;
            floatArrays[i][j] = readDouble(ptr);
        }
    }
    
    // String arrays
    if (ptr + 4 > end) return false;
    uint32_t strArrayCount = readUint32(ptr);
    stringArrays.resize(strArrayCount);
    
    for (uint32_t i = 0; i < strArrayCount; i++) {
        if (ptr + 4 > end) return false;
        uint32_t len = readUint32(ptr);
        
        stringArrays[i].resize(len);
        for (uint32_t j = 0; j < len; j++) {
            if (ptr + 4 > end) return false;
            uint32_t slen = readUint32(ptr);
            if (ptr + slen > end) return false;
            
            stringArrays[i][j].resize(slen);
            for (uint32_t k = 0; k < slen; k++) {
                stringArrays[i][j][k] = static_cast<char>(ptr[k]);
            }
            ptr += slen;
        }
    }
    
    // Functions
    if (ptr + 4 > end) return false;
    uint32_t funcCount = readUint32(ptr);
    functions.resize(funcCount);
    
    for (uint32_t i = 0; i < funcCount; i++) {
        if (ptr + 4 > end) return false;
        uint32_t nameLen = readUint32(ptr);
        if (ptr + nameLen > end) return false;
        
        functions[i].name.resize(nameLen);
        for (uint32_t j = 0; j < nameLen; j++) {
            functions[i].name[j] = static_cast<char>(ptr[j]);
        }
        ptr += nameLen;
        
        if (ptr + 4 > end) return false;
        functions[i].address = readUint32(ptr);
        
        if (ptr + 1 > end) return false;
        functions[i].arity = *ptr++;
        
        if (ptr + 1 > end) return false;
        functions[i].locals = *ptr++;
    }
    
    // Native imports
    if (ptr + 4 > end) return false;
    uint32_t nativeCount = readUint32(ptr);
    nativeImports.resize(nativeCount);
    
    for (uint32_t i = 0; i < nativeCount; i++) {
        if (ptr + 4 > end) return false;
        uint32_t len = readUint32(ptr);
        if (ptr + len > end) return false;
        
        nativeImports[i].resize(len);
        for (uint32_t j = 0; j < len; j++) {
            nativeImports[i][j] = static_cast<char>(ptr[j]);
        }
        ptr += len;
    }
    
    // Check we read exactly all data
    if (ptr != end) {
        // Not necessarily an error, but warn
        std::cerr << "Warning: " << (end - ptr) << " extra bytes in bytecode file" << std::endl;
    }
    
    return true;
}

std::string Value::toString(const BytecodeFile* prog) const {
    switch (type) {
        case TYPE_NIL:
            return "nil";
        case TYPE_INT:
            return std::to_string(as.intVal);
        case TYPE_FLOAT:
            return std::to_string(as.floatVal);
        case TYPE_BOOL:
            return as.boolVal ? "true" : "false";
        case TYPE_STRING:
            if (prog && as.stringIdx < prog->strings.size())
                return prog->strings[as.stringIdx];
            return "[string]";
        case TYPE_ARRAY_INT:
            return "[int array]";
        case TYPE_ARRAY_FLOAT:
            return "[float array]";
        case TYPE_ARRAY_STRING:
            return "[string array]";
        default:
            return "[unknown]";
    }
}

bool Value::isTruthy() const {
    switch (type) {
        case TYPE_NIL:
            return false;
        case TYPE_INT:
            return as.intVal != 0;
        case TYPE_FLOAT:
            return as.floatVal != 0.0;
        case TYPE_BOOL:
            return as.boolVal;
        case TYPE_STRING:
            return true; // Strings are always truthy
        default:
            return true;
    }
}

void BytecodeFile::dump() const {
    std::cout << "=== TAIL Bytecode Dump ===" << std::endl;
    std::cout << "Version: " << version << std::endl;
    std::cout << "Code size: " << code.size() << " instructions" << std::endl;
    std::cout << "Constants: " << constants.size() << std::endl;
    std::cout << "Strings: " << strings.size() << std::endl;
    std::cout << "Int arrays: " << intArrays.size() << std::endl;
    std::cout << "Float arrays: " << floatArrays.size() << std::endl;
    std::cout << "String arrays: " << stringArrays.size() << std::endl;
    std::cout << "Functions: " << functions.size() << std::endl;
    std::cout << "Native imports: " << nativeImports.size() << std::endl;
    
    // Dump code
    if (!code.empty()) {
        std::cout << "\n=== Code ===" << std::endl;
        for (size_t i = 0; i < code.size(); i++) {
            std::cout << std::setw(4) << std::setfill('0') << i << ": ";
            switch (code[i].opcode) {
                case OP_PUSH: std::cout << "PUSH " << code[i].operand; break;
                case OP_POP: std::cout << "POP"; break;
                case OP_DUP: std::cout << "DUP"; break;
                case OP_SWAP: std::cout << "SWAP"; break;
                case OP_ADD: std::cout << "ADD"; break;
                case OP_SUB: std::cout << "SUB"; break;
                case OP_MUL: std::cout << "MUL"; break;
                case OP_DIV: std::cout << "DIV"; break;
                case OP_MOD: std::cout << "MOD"; break;
                case OP_NEG: std::cout << "NEG"; break;
                case OP_INC: std::cout << "INC"; break;
                case OP_DEC: std::cout << "DEC"; break;
                case OP_EQ: std::cout << "EQ"; break;
                case OP_NEQ: std::cout << "NEQ"; break;
                case OP_LT: std::cout << "LT"; break;
                case OP_LTE: std::cout << "LTE"; break;
                case OP_GT: std::cout << "GT"; break;
                case OP_GTE: std::cout << "GTE"; break;
                case OP_AND: std::cout << "AND"; break;
                case OP_OR: std::cout << "OR"; break;
                case OP_NOT: std::cout << "NOT"; break;
                case OP_LOAD: std::cout << "LOAD " << code[i].operand; break;
                case OP_STORE: std::cout << "STORE " << code[i].operand; break;
                case OP_LOAD_GLOBAL: std::cout << "LOAD_GLOBAL " << code[i].operand; break;
                case OP_STORE_GLOBAL: std::cout << "STORE_GLOBAL " << code[i].operand; break;
                case OP_JMP: std::cout << "JMP " << code[i].operand; break;
                case OP_JMP_IF: std::cout << "JMP_IF " << code[i].operand; break;
                case OP_JMP_IFNOT: std::cout << "JMP_IFNOT " << code[i].operand; break;
                case OP_CALL: std::cout << "CALL " << code[i].operand; break;
                case OP_RET: std::cout << "RET"; break;
                case OP_CALL_NATIVE: std::cout << "CALL_NATIVE " << code[i].operand; break;
                case OP_NEW_ARRAY: std::cout << "NEW_ARRAY " << code[i].operand; break;
                case OP_LOAD_INDEX: std::cout << "LOAD_INDEX"; break;
                case OP_STORE_INDEX: std::cout << "STORE_INDEX"; break;
                case OP_ARRAY_LEN: std::cout << "ARRAY_LEN"; break;
                case OP_PRINT: std::cout << "PRINT"; break;
                case OP_READ: std::cout << "READ"; break;
                case OP_PRINTLN: std::cout << "PRINTLN"; break;
                case OP_HALT: std::cout << "HALT"; break;
                default: std::cout << "UNKNOWN(" << std::hex << (int)code[i].opcode << std::dec << ")"; break;
            }
            std::cout << std::endl;
        }
    }
    
    // Dump strings
    if (!strings.empty()) {
        std::cout << "\n=== Strings ===" << std::endl;
        for (size_t i = 0; i < strings.size(); i++) {
            std::cout << std::setw(4) << i << ": \"" << strings[i] << "\"" << std::endl;
        }
    }
    
    // Dump constants
    if (!constants.empty()) {
        std::cout << "\n=== Constants ===" << std::endl;
        for (size_t i = 0; i < constants.size(); i++) {
            std::cout << std::setw(4) << i << ": ";
            switch (constants[i].type) {
                case TYPE_NIL: std::cout << "NIL"; break;
                case TYPE_INT: std::cout << "INT " << constants[i].as.intVal; break;
                case TYPE_FLOAT: std::cout << "FLOAT " << constants[i].as.floatVal; break;
                case TYPE_BOOL: std::cout << "BOOL " << (constants[i].as.boolVal ? "true" : "false"); break;
                case TYPE_STRING: std::cout << "STRING idx=" << constants[i].as.stringIdx; break;
                case TYPE_ARRAY_INT: std::cout << "ARRAY_INT idx=" << constants[i].as.stringIdx; break;
                case TYPE_ARRAY_FLOAT: std::cout << "ARRAY_FLOAT idx=" << constants[i].as.stringIdx; break;
                case TYPE_ARRAY_STRING: std::cout << "ARRAY_STRING idx=" << constants[i].as.stringIdx; break;
                default: std::cout << "UNKNOWN"; break;
            }
            std::cout << std::endl;
        }
    }
    
    // Dump functions
    if (!functions.empty()) {
        std::cout << "\n=== Functions ===" << std::endl;
        for (const auto& func : functions) {
            std::cout << func.name << " @ " << func.address 
                      << " (arity=" << (int)func.arity 
                      << ", locals=" << (int)func.locals << ")" << std::endl;
        }
    }
    
    // Dump native imports
    if (!nativeImports.empty()) {
        std::cout << "\n=== Native Imports ===" << std::endl;
        for (size_t i = 0; i < nativeImports.size(); i++) {
            std::cout << i << ": " << nativeImports[i] << std::endl;
        }
    }
}

} // namespace TVM