#include "vm/vm.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: tail <file.tailc>" << std::endl;
        std::cerr << "Executes Tail bytecode in the Tail Virtual Machine." << std::endl;
        std::cerr << std::endl;
        std::cerr << "First compile your Tail source code:" << std::endl;
        std::cerr << "  tailc program.tail" << std::endl;
        std::cerr << "Then execute it:" << std::endl;
        std::cerr << "  tail program.tailc" << std::endl;
        return 1;
    }
    
    std::string inputFile = argv[1];
    
    // Check file extension
    std::filesystem::path inputPath(inputFile);
    if (inputPath.extension() != ".tailc") {
        std::cerr << "Warning: Expected .tailc file extension" << std::endl;
    }
    
    std::ifstream file(inputFile, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Error: Cannot open file '" << inputFile << "'" << std::endl;
        return 1;
    }
    
    try {
        // Read entire file
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> data(size);
        if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
            std::cerr << "Error: Failed to read file" << std::endl;
            return 1;
        }
        
        std::cout << "Loading " << inputFile << " (" << size << " bytes)..." << std::endl;
        
        // Deserialize bytecode
        TVM::BytecodeFile bytecode;
        if (!bytecode.deserialize(data)) {
            std::cerr << "Error: Invalid bytecode file" << std::endl;
            return 1;
        }
        
        // Verify magic number
        if (bytecode.magic != 0x5441494C) {
            std::cerr << "Error: Not a valid Tail bytecode file" << std::endl;
            return 1;
        }
        
        std::cout << "Tail Virtual Machine v1.0" << std::endl;
        std::cout << "=========================" << std::endl;
        
        // Execute
        TVM::VM vm;
        
        // Enable tracing with environment variable
        const char* traceEnv = std::getenv("TAIL_TRACE");
        if (traceEnv && std::string(traceEnv) == "1") {
            vm.setTrace(true);
            std::cout << "[Tracing enabled]" << std::endl;
        }
        
        vm.execute(bytecode);
        
        std::cout << "=========================" << std::endl;
        std::cout << "Program finished." << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Runtime error: " << e.what() << std::endl;
        return 1;
    }
}