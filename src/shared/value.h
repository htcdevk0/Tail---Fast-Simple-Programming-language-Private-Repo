#pragma once
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <cstdint>  // ← ADICIONE ESTA LINHA

class Value {
public:
    enum Type {
        NIL,
        INT,
        FLOAT,
        BOOL,
        STRING,
        INT_ARRAY,
        FLOAT_ARRAY,
        STRING_ARRAY
    };
    
    Value() : type(NIL) { data.intVal = 0; }
    Value(int64_t v) : type(INT) { data.intVal = v; }
    Value(double v) : type(FLOAT) { data.floatVal = v; }
    Value(bool v) : type(BOOL) { data.boolVal = v; }
    Value(const std::string& v) : type(STRING) { data.strVal = new std::string(v); }
    Value(const std::vector<int64_t>& v) : type(INT_ARRAY) { data.intArrayVal = new std::vector<int64_t>(v); }
    Value(const std::vector<double>& v) : type(FLOAT_ARRAY) { data.floatArrayVal = new std::vector<double>(v); }
    Value(const std::vector<std::string>& v) : type(STRING_ARRAY) { data.strArrayVal = new std::vector<std::string>(v); }
    
    ~Value() { cleanup(); }
    Value(const Value& other) { copyFrom(other); }
    Value& operator=(const Value& other) {
        if (this != &other) {
            cleanup();
            copyFrom(other);
        }
        return *this;
    }
    
    // Getters
    int64_t asInt() const {
        if (type != INT) throw std::runtime_error("Value is not an int");
        return data.intVal;
    }
    
    double asFloat() const {
        if (type != FLOAT) throw std::runtime_error("Value is not a float");
        return data.floatVal;
    }
    
    bool asBool() const {
        if (type != BOOL) throw std::runtime_error("Value is not a bool");
        return data.boolVal;
    }
    
    std::string asStr() const {
        if (type != STRING) throw std::runtime_error("Value is not a string");
        return *data.strVal;
    }
    
    // Type checking
    bool isInt() const { return type == INT; }
    bool isFloat() const { return type == FLOAT; }
    bool isBool() const { return type == BOOL; }
    bool isStr() const { return type == STRING; }
    bool isIntArray() const { return type == INT_ARRAY; }
    bool isFloatArray() const { return type == FLOAT_ARRAY; }
    bool isStrArray() const { return type == STRING_ARRAY; }
    bool isNil() const { return type == NIL; }
    
    // String representation (for debugging)
    std::string toString() const {
        switch (type) {
            case NIL: return "nil";
            case INT: return std::to_string(data.intVal);
            case FLOAT: return std::to_string(data.floatVal);
            case BOOL: return data.boolVal ? "true" : "false";
            case STRING: return *data.strVal;
            case INT_ARRAY: return "[int array of size " + std::to_string(data.intArrayVal->size()) + "]";
            case FLOAT_ARRAY: return "[float array of size " + std::to_string(data.floatArrayVal->size()) + "]";
            case STRING_ARRAY: return "[string array of size " + std::to_string(data.strArrayVal->size()) + "]";
            default: return "unknown";
        }
    }
    
private:
    Type type;
    union Data {
        int64_t intVal;
        double floatVal;
        bool boolVal;
        std::string* strVal;
        std::vector<int64_t>* intArrayVal;
        std::vector<double>* floatArrayVal;
        std::vector<std::string>* strArrayVal;
        
        Data() : intVal(0) {}
        ~Data() {}
    } data;
    
    void cleanup() {
        switch (type) {
            case STRING: delete data.strVal; break;
            case INT_ARRAY: delete data.intArrayVal; break;
            case FLOAT_ARRAY: delete data.floatArrayVal; break;
            case STRING_ARRAY: delete data.strArrayVal; break;
            default: break;
        }
        type = NIL;
        data.intVal = 0;
    }
    
    void copyFrom(const Value& other) {
        type = other.type;
        switch (type) {
            case NIL: data.intVal = 0; break;
            case INT: data.intVal = other.data.intVal; break;
            case FLOAT: data.floatVal = other.data.floatVal; break;
            case BOOL: data.boolVal = other.data.boolVal; break;
            case STRING: data.strVal = new std::string(*other.data.strVal); break;
            case INT_ARRAY: data.intArrayVal = new std::vector<int64_t>(*other.data.intArrayVal); break;
            case FLOAT_ARRAY: data.floatArrayVal = new std::vector<double>(*other.data.floatArrayVal); break;
            case STRING_ARRAY: data.strArrayVal = new std::vector<std::string>(*other.data.strArrayVal); break;
        }
    }
};