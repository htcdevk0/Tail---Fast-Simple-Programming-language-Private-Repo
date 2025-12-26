#include "compiler/compiler.h"
#include "shared/lexer.h"
#include "shared/parser.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <sstream>
#include <set>
#include <algorithm>
#include <cctype>
#include <map>

bool endsWith(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}


std::string findIncludeFile(const std::string& includeName, const std::string& currentDir) {
    
    std::vector<std::string> searchPaths = {
        includeName + ".tail",                    
        currentDir + "/" + includeName + ".tail", 
        "../include/" + includeName + ".tail",    
        "include/" + includeName + ".tail",       
        "./include/" + includeName + ".tail",     
    };
    
    for (const auto& path : searchPaths) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    
    return "";
}


void loadIncludeRecursive(const std::string& includeName, 
                         std::vector<std::string>& allFiles,
                         std::set<std::string>& loaded,
                         const std::string& currentDir) {
    
    if (loaded.count(includeName)) return;
    loaded.insert(includeName);
    
    std::string foundPath = findIncludeFile(includeName, currentDir);
    if (!foundPath.empty()) {
        std::cout << "  Found include: " << includeName << " -> " << foundPath << std::endl;
        allFiles.push_back(foundPath);
        
        
        std::ifstream incFile(foundPath);
        std::string line;
        while (std::getline(incFile, line)) {
            if (line.find("include ") != std::string::npos) {
                size_t start = line.find("include ") + 8;
                size_t end = line.find(';');
                if (end != std::string::npos) {
                    std::string subInclude = line.substr(start, end - start);
                    
                    subInclude.erase(std::remove_if(subInclude.begin(), subInclude.end(), ::isspace), subInclude.end());
                    
                    if (!subInclude.empty()) {
                        
                        std::filesystem::path p(foundPath);
                        std::string newDir = p.parent_path().string();
                        if (newDir.empty()) newDir = ".";
                        
                        loadIncludeRecursive(subInclude, allFiles, loaded, newDir);
                    }
                }
            }
        }
    } else {
        std::cout << "  WARNING: Could not find include: " << includeName << std::endl;
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: tailc <file1.tail> [file2.tail ...] [-o output.tailc]" << std::endl;
        std::cerr << "Compiles Tail source code to Tail bytecode." << std::endl;
        return 1;
    }

    std::vector<std::string> inputFiles;
    std::string outputFile;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-o") {
            if (i + 1 < argc) {
                outputFile = argv[++i];
            } else {
                std::cerr << "Error: -o flag requires output filename" << std::endl;
                return 1;
            }
        } else if (endsWith(arg, ".tail")) {
            inputFiles.push_back(arg);
        } else {
            std::cerr << "Error: Unknown argument or not a .tail file: " << arg << std::endl;
            return 1;
        }
    }

    if (inputFiles.empty()) {
        std::cerr << "Error: No .tail files specified" << std::endl;
        return 1;
    }

    if (outputFile.empty()) {
        std::filesystem::path firstPath(inputFiles[0]);
        outputFile = firstPath.stem().string() + ".tailc";
    }

    std::cout << "Compiling " << inputFiles.size() << " input file(s)..." << std::endl;
    
    try
    {
        
        std::vector<std::string> allSourceFiles;
        std::set<std::string> loadedFiles;
        
        for (const auto& inputFile : inputFiles) {
            std::filesystem::path p(inputFile);
            std::string currentDir = p.parent_path().string();
            if (currentDir.empty()) currentDir = ".";
            
            
            allSourceFiles.push_back(inputFile);
            loadedFiles.insert(p.filename().string());
            
            
            std::ifstream file(inputFile);
            std::string line;
            while (std::getline(file, line)) {
                if (line.find("include ") != std::string::npos) {
                    size_t start = line.find("include ") + 8;
                    size_t end = line.find(';');
                    if (end != std::string::npos) {
                        std::string includeName = line.substr(start, end - start);
                        includeName.erase(std::remove_if(includeName.begin(), includeName.end(), ::isspace), includeName.end());
                        
                        if (!includeName.empty()) {
                            loadIncludeRecursive(includeName, allSourceFiles, loadedFiles, currentDir);
                        }
                    }
                }
            }
        }
        
        std::cout << "Total files to compile: " << allSourceFiles.size() << std::endl;
        
        
        struct FileData {
            std::string path;
            std::vector<std::shared_ptr<Tail::Stmt>> ast;
            bool isMainFile;  
            std::string moduleName;
        };
        
        std::vector<FileData> filesData;
        std::map<std::string, std::vector<std::shared_ptr<Tail::FunctionStmt>>> functionsByModule;
        
        for (const auto& sourceFile : allSourceFiles) {
            std::cout << "  Parsing: " << sourceFile << std::endl;
            
            std::ifstream file(sourceFile);
            if (!file) {
                std::cerr << "Error: Cannot open file '" << sourceFile << "'" << std::endl;
                return 1;
            }
            
            std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            
            Tail::Lexer lexer(source);
            auto tokens = lexer.tokenize();
            
            auto lexerErrors = lexer.getErrors();
            if (!lexerErrors.empty()) {
                std::cerr << "Lexer errors in " << sourceFile << ":" << std::endl;
                for (const auto &err : lexerErrors) {
                    std::cerr << "  " << err << std::endl;
                }
                return 1;
            }
            
            Tail::Parser parser(tokens);
            auto ast = parser.parse();
            
            auto parserErrors = parser.getErrors();
            if (!parserErrors.empty()) {
                std::cerr << "Parser errors in " << sourceFile << ":" << std::endl;
                for (const auto &err : parserErrors) {
                    std::cerr << "  " << err << std::endl;
                }
                return 1;
            }
            
            std::filesystem::path p(sourceFile);
            std::string moduleName = p.stem().string();
            bool isMainFile = std::find(inputFiles.begin(), inputFiles.end(), sourceFile) != inputFiles.end();
            
            filesData.push_back({sourceFile, ast, isMainFile, moduleName});
            
            
            for (const auto &stmt : ast) {
                if (!stmt) continue;
                
                if (auto func = std::dynamic_pointer_cast<Tail::FunctionStmt>(stmt)) {
                    functionsByModule[moduleName].push_back(func);
                }
            }
        }
        
        
        Tail::Compiler compiler;
        std::vector<std::shared_ptr<Tail::Stmt>> allStatements;
        bool hasMain = false;
        
        
        std::cout << "\nCompiling include functions..." << std::endl;
        for (const auto& file : filesData) {
            if (!file.isMainFile) {  
                for (const auto& func : functionsByModule[file.moduleName]) {
                    if (func->name != "Main") {
                        std::cout << "  " << file.moduleName << "_" << func->name << std::endl;
                        compiler.compileFunction(*func, file.moduleName);
                    }
                }
            }
        }
        
        
        std::cout << "\nCompiling auxiliary functions..." << std::endl;
        for (const auto& file : filesData) {
            if (file.isMainFile) {
                for (const auto& func : functionsByModule[file.moduleName]) {
                    if (func->name != "Main") {
                        std::cout << "  " << func->name << " (from " << file.moduleName << ")" << std::endl;
                        compiler.compileFunction(*func, file.moduleName);
                    }
                }
            }
        }
        
        
        std::cout << "\nCompiling Main function..." << std::endl;
        for (const auto& file : filesData) {
            for (const auto& func : functionsByModule[file.moduleName]) {
                if (func->name == "Main") {
                    hasMain = true;
                    std::cout << "  Main (from " << file.moduleName << ")" << std::endl;
                    compiler.compileFunction(*func, "");
                }
            }
        }
        
        if (!hasMain) {
            std::cerr << "Error: No Main() function found" << std::endl;
            return 1;
        }
        
        
        for (const auto& file : filesData) {
            allStatements.insert(allStatements.end(), file.ast.begin(), file.ast.end());
        }
        
        
        std::cout << "\nGenerating final bytecode..." << std::endl;
        Tail::Compiler finalCompiler;
        auto bytecode = finalCompiler.compile(allStatements);
        
        
        std::ofstream out(outputFile, std::ios::binary);
        auto data = bytecode.serialize();
        if (!out)
        {
            std::cerr << "Error: Cannot write to '" << outputFile << "'" << std::endl;
            return 1;
        }
        
        out.write(reinterpret_cast<const char *>(data.data()), data.size());
        out.close();
        
        std::cout << "\nSuccessfully compiled!" << std::endl;
        std::cout << "  Output: " << outputFile << std::endl;
        std::cout << "  Bytecode size: " << data.size() << " bytes" << std::endl;
        std::cout << "  Instructions: " << bytecode.code.size() << std::endl;
        std::cout << "  Constants: " << bytecode.constants.size() << std::endl;
        std::cout << "  Functions: " << bytecode.functions.size() << std::endl;

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Compilation failed: " << e.what() << std::endl;
        return 1;
    }
}