cmake_minimum_required(VERSION 3.10)
project(Tail LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directory for executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror -Wpedantic)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# Shared library (lexer, parser, ast, bytecode)
add_library(tail_shared STATIC
    src/shared/lexer.cpp
    src/shared/parser.cpp
    src/shared/ast.cpp
    src/shared/bytecode.cpp
)

# Compiler library
add_library(tail_compiler STATIC
    src/compiler/compiler.cpp
)

# VM library
add_library(tail_vm STATIC
    src/vm/vm.cpp
)

# Compiler executable
add_executable(tailc
    src/tailc.cpp
)

target_link_libraries(tailc
    tail_shared
    tail_compiler
)

# VM executable
add_executable(tail
    src/tail.cpp
)

target_link_libraries(tail
    tail_shared
    tail_vm
)

if(UNIX)
    install(TARGETS tail tailc
        RUNTIME DESTINATION bin
    )
    
    install(DIRECTORY include/
        DESTINATION share/tail/include
    )
    
    # Create shell scripts for convenience
    file(WRITE ${CMAKE_BINARY_DIR}/tail-run.sh "#!/bin/bash\n./tail \"$@\"\n")
    file(WRITE ${CMAKE_BINARY_DIR}/tailc-run.sh "#!/bin/bash\n./tailc \"$@\"\n")
    
    set(SCRIPTS ${CMAKE_BINARY_DIR}/tail-run.sh ${CMAKE_BINARY_DIR}/tailc-run.sh)
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${SCRIPTS}")
endif()

# Create test directory if it doesn't exist
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/tests)
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests)
endif()

# Optional: Add test
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/hello.tail)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/hello.tailc
        COMMAND tailc ${CMAKE_SOURCE_DIR}/tests/hello.tail
        DEPENDS tailc
        COMMENT "Compiling test program"
    )
    
    add_custom_target(test_compile
        DEPENDS ${CMAKE_BINARY_DIR}/hello.tailc
    )
    
    add_test(NAME test_vm
        COMMAND tail ${CMAKE_BINARY_DIR}/hello.tailc
    )
endif()

# Create a package
set(CPACK_PACKAGE_NAME "Tail")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION "Tail Programming Language")
set(CPACK_PACKAGE_VENDOR "Tail Team")
include(CPack)