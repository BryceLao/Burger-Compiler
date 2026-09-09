cmake_minimum_required(VERSION 3.20)

project(burgercompiler)

set(CMAKE_CXX_STANDARD 20)

add_executable(
        burger
        src/main.cpp
        src/tokenizer/tokenizer.hpp
        src/parser/parser.hpp
        src/generator/generator.hpp
        src/util/arenaAllocator.hpp
        src/util/dataType.hpp
        src/parser/ast.hpp
        src/parser/parser.cpp
        src/generator/generator.cpp
        src/tokenizer/tokenizer.cpp
        src/tokenizer/tokens.hpp
)