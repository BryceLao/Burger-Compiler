cmake_minimum_required(VERSION 3.20)

project(burgercompiler)

set(CMAKE_CXX_STANDARD 20)

add_executable(burger src/main.cpp
        src/tokenizer.hpp
        src/parser.hpp
        src/generator.hpp
        src/arenaAllocator.hpp)