#pragma once

#include <string>
#include <vector>
#include "util.hpp"

std::optional<int> getPrecedenceLevel(TokenType type) {
    switch (type) {
        case TokenType::orOperator:
            return 0;
        case TokenType::andOperator:
            return 1;
        case TokenType::notOperator:
            return 2;
        case TokenType::equalTo:
        case TokenType::notEqualTo:
        case TokenType::lessThan:
        case TokenType::lessThanOrEqual:
        case TokenType::greaterThan:
        case TokenType::greaterThanOrEqual:
            return 3;
        case TokenType::addition:
        case TokenType::subtraction:
            return 4;
        case TokenType::multiplication:
        case TokenType::division:
        case TokenType::modulo:
            return 5;

        default:
            return {};
    }
}

struct Token {
    TokenType type;
    int lineNumber;
    std::optional<std::string> value = {};
};

class Tokenizer {
    public:
        inline explicit Tokenizer(const std::string& src):
            m_src(src + "\n") {}

        std::vector<Token> tokenize() {
            std::vector<Token> tokens;
            std::string buffer;
            int curLineNumber = 1;

            while(peek().has_value()) {
                char c = peek().value();

                if(std::isalpha(c)) {
                    buffer.push_back(consume());

                    while(peek().has_value() && isalnum(peek().value())) {
                        buffer.push_back(consume());
                    }

                    if(buffer == "exit") {
                        tokens.push_back({.type = TokenType::exit, .lineNumber = curLineNumber});
                        buffer.clear();
                    }
                    else if(buffer == "print") {
                        tokens.push_back({.type = TokenType::print, .lineNumber = curLineNumber});
                        buffer.clear();
                    }
                    else if(buffer == "set") {
                        tokens.push_back({.type = TokenType::set, .lineNumber = curLineNumber});
                        buffer.clear();
                    }
                    else if(buffer == "int") {
                        tokens.push_back({.type = TokenType::intType, .lineNumber = curLineNumber});
                        buffer.clear();
                    }
                    else if(buffer == "bool") {
                        tokens.push_back({.type = TokenType::boolType, .lineNumber = curLineNumber});
                        buffer.clear();
                    }
                    else if(buffer == "char") {
                        tokens.push_back({.type = TokenType::charType, .lineNumber = curLineNumber});
                        buffer.clear();
                    }
                    else if(buffer == "True") {
                        tokens.push_back({.type = TokenType::boolLiteral, .lineNumber = curLineNumber, .value = "1"});
                        buffer.clear();
                    }
                    else if(buffer == "False") {
                        tokens.push_back({.type = TokenType::boolLiteral, .lineNumber = curLineNumber, .value = "0"});
                        buffer.clear();
                    }
                    else if(buffer == "if") {
                        tokens.push_back({.type = TokenType::ifStatement, .lineNumber = curLineNumber});
                        buffer.clear();
                    }
                    else if(buffer == "else") {
                        if(tryPeek(' ') && tryPeek('i', 1) && tryPeek('f', 2)) {
                            consume(); consume(); consume();

                            tokens.push_back({.type = TokenType::elseIfStatement, .lineNumber = curLineNumber});
                            buffer.clear();
                        }
                        else {
                            tokens.push_back({.type = TokenType::elseStatement, .lineNumber = curLineNumber});
                            buffer.clear();
                        }
                    }
                    else if(buffer == "while") {
                        tokens.push_back({.type = TokenType::whileLoop, .lineNumber = curLineNumber});
                        buffer.clear();
                    }
                    else if(buffer == "and") {
                        tokens.push_back({.type = TokenType::andOperator, .lineNumber = curLineNumber});
                        buffer.clear();
                    }
                    else if(buffer == "or") {
                        tokens.push_back({.type = TokenType::orOperator, .lineNumber = curLineNumber});
                        buffer.clear();
                    }
                    else if(buffer == "not") {
                        tokens.push_back({.type = TokenType::notOperator, .lineNumber = curLineNumber});
                        buffer.clear();
                    }
                    else if(buffer == "new") {
                        tokens.push_back({.type = TokenType::newKeyWord, .lineNumber = curLineNumber});
                        buffer.clear();
                    }
                    else if(buffer == "Array") {
                        tokens.push_back({.type = TokenType::arrayType, .lineNumber = curLineNumber});
                        buffer.clear();
                    }
                    else if(buffer == "size") {
                        tokens.push_back({.type = TokenType::size, .lineNumber = curLineNumber});
                        buffer.clear();
                    }
                    else {
                        tokens.push_back({.type = TokenType::identifier, .lineNumber = curLineNumber, .value = buffer});
                        buffer.clear();
                    }
                }
                else if(std::isdigit(c)) {
                    buffer.push_back(consume());

                    while(peek().has_value() && std::isdigit(peek().value())) {
                        buffer.push_back(consume());
                    }

                    tokens.push_back({.type = TokenType::intLiteral, .lineNumber = curLineNumber, .value = buffer});
                    buffer.clear();
                }
                else if(std::isspace(c)) {
                    if(c == '\n') ++curLineNumber;

                    consume();
                }
                else if(c == '=') {
                    consume();

                    if(tryPeek('=')) {
                        tokens.push_back({.type = TokenType::equalTo, .lineNumber = curLineNumber});
                        consume();
                    }
                    else tokens.push_back({.type = TokenType::assignment, .lineNumber = curLineNumber});
                }
                else if(c == '<') {
                    consume();

                    if(tryPeek('=')) {
                        tokens.push_back({.type = TokenType::lessThanOrEqual, .lineNumber = curLineNumber});
                        consume();
                    }
                    else tokens.push_back({.type = TokenType::lessThan, .lineNumber = curLineNumber});
                }
                else if(c == '>') {
                    consume();

                    if(tryPeek('=')) {
                        tokens.push_back({.type = TokenType::greaterThanOrEqual, .lineNumber = curLineNumber});
                        consume();
                    }
                    else tokens.push_back({.type = TokenType::greaterThan, .lineNumber = curLineNumber});
                }
                else if(c == '!' && tryPeek('=', 1)) {
                    consume();
                    consume();

                    tokens.push_back({.type = TokenType::notEqualTo, .lineNumber = curLineNumber});
                }
                else if(c == '\'') {
                    if(!tryPeek('\'',2)) {
                        std::cerr << "Line " << curLineNumber << ": Error: Expected ' but found " << peek(2).value() << std::endl;
                        exit(EXIT_FAILURE);
                    }

                    consume();
                    tokens.push_back({.type = TokenType::charLiteral, .lineNumber = curLineNumber, .value = std::to_string(static_cast<int>(peek().value()))});
                    consume(); consume();
                }
                else if(c == '/') {
                    consume();

                    if(tryPeek('/'))  {
                        consume();

                        while(!tryPeek('\n') && !tryPeek('\r')) {
                            consume();
                        }
                    }
                    else tokens.push_back({.type = TokenType::division, .lineNumber = curLineNumber});
                }
                else {
                    switch(c) {
                        case '(':
                            tokens.push_back({.type = TokenType::openParenthesis, .lineNumber = curLineNumber});
                            consume();
                            break;
                        case ')':
                            tokens.push_back({.type = TokenType::closeParenthesis, .lineNumber = curLineNumber});
                            consume();
                            break;
                        case ';':
                            tokens.push_back({.type = TokenType::semiCol, .lineNumber = curLineNumber});
                            consume();
                            break;
                        case '+':
                            tokens.push_back({.type = TokenType::addition, .lineNumber = curLineNumber});
                            consume();
                            break;
                        case '-':
                            tokens.push_back({.type = TokenType::subtraction, .lineNumber = curLineNumber});
                            consume();
                            break;
                        case '*':
                            tokens.push_back({.type = TokenType::multiplication, .lineNumber = curLineNumber});
                            consume();
                            break;
                        case '%':
                            tokens.push_back({.type = TokenType::modulo, .lineNumber = curLineNumber});
                            consume();
                            break;
                        case '{':
                            tokens.push_back({.type = TokenType::openCurlyBrace, .lineNumber = curLineNumber});
                            consume();
                            break;
                        case '}':
                            tokens.push_back({.type = TokenType::closeCurlyBrace, .lineNumber = curLineNumber});
                            consume();
                            break;
                        case '[':
                            tokens.push_back({.type = TokenType::openBracket, .lineNumber = curLineNumber});
                            consume();
                            break;
                        case ']':
                            tokens.push_back({.type = TokenType::closeBracket, .lineNumber = curLineNumber});
                            consume();
                            break;
                        case '.':
                            tokens.push_back({.type = TokenType::dot, .lineNumber = curLineNumber});
                            consume();
                            break;
                        default:
                            std::cerr << "Line " << curLineNumber << ": Error: Unexpected character '" << c << "'" << std::endl;
                            exit(EXIT_FAILURE);
                    }
                }
            }

            m_Index = 0;

            return tokens;
        }
    private:
        const std::string m_src;
        int m_Index = 0;

        inline bool tryPeek(char c, int ahead = 0) {
            if(m_Index + ahead >= m_src.length()) return false;

            return c == peek(ahead).value();
        }

        [[nodiscard]] inline std::optional<char> peek(int ahead = 0) const {
            if(m_Index + ahead >= m_src.length()) return {};
            else return m_src.at(m_Index + ahead);
        }

        inline char consume() {
            return m_src.at(m_Index++);
        }
};