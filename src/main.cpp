#include <iostream>
#include <fstream>
#include <sstream>
#include <optional>
#include <vector>

#include "tokenizer/tokenizer.hpp"
#include "parser/parser.hpp"
#include "generator/generator.hpp"

int main(int argc, char* argv[]) {

    if(argc != 2) {
        std::cerr << "Incorrect amount of arguments passed" << std::endl;
        std::cerr << "burger <input.bger>" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string contents;
    {
        std::stringstream contentStream;
        std::fstream input(argv[1], std::ios::in);
        contentStream << input.rdbuf();
        contents = contentStream.str();
    }

    Tokenizer::Tokenizer tokenizer(contents);
    std::vector<Tokenizer::Token> tokens = tokenizer.tokenize();

    Parser::Parser parser(std::move(tokens));

    Parser::ProgramNode program = parser.parseProgram();

    Generator::Generator generator(program);
    {
        std::fstream file("out.asm", std::ios::out);
        file << generator.generateProgram();
    }

    system("nasm -felf64 out.asm");
    system("ld -o out out.o");

    return EXIT_SUCCESS;
}