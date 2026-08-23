#pragma once

#include <string>
#include <vector>

enum class TokenType {
    exit,
    print,
    intLiteral,
    identifier,
    set,
    assignment,
    semiCol,
    openParenthesis,
    closeParenthesis,
    openCurlyBrace,
    closeCurlyBrace,
    addition,
    subtraction,
    multiplication,
    division,
    modulo,
    lessThan,
    greaterThan,
    lessThanOrEqual,
    greaterThanOrEqual,
    equalTo,
    notEqualTo,
    andOperator,
    orOperator,
    notOperator,
    ifStatement,
    elseIfStatement,
    elseStatement,
    whileLoop
};

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
    std::optional<std::string> value = {};
};

class Tokenizer {
    public:
        inline explicit Tokenizer(const std::string& src):
            m_src(std::move(src)) {}

        std::vector<Token> tokenize() {
            std::vector<Token> tokens;
            std::string buffer;

            while(peek().has_value()) {
                char c = peek().value();

                if(std::isalpha(c)) {
                    buffer.push_back(consume());

                    while(peek().has_value() && isalnum(peek().value())) {
                        buffer.push_back(consume());
                    }

                    if(buffer == "exit") {
                        tokens.push_back({.type = TokenType::exit});
                        buffer.clear();
                    }
                    else if(buffer == "print") {
                        tokens.push_back({.type = TokenType::print});
                        buffer.clear();
                    }
                    else if(buffer == "set") {
                        tokens.push_back({.type = TokenType::set});
                        buffer.clear();
                    }
                    else if(buffer == "if") {
                        tokens.push_back({.type = TokenType::ifStatement});
                        buffer.clear();
                    }
                    else if(buffer == "else") {
                        if(tryPeek(' ') && tryPeek('i', 1) && tryPeek('f', 2)) {
                            consume(); consume(); consume();

                            tokens.push_back({.type = TokenType::elseIfStatement});
                            buffer.clear();
                        }
                        else {
                            tokens.push_back({.type = TokenType::elseStatement});
                            buffer.clear();
                        }
                    }
                    else if(buffer == "while") {
                        tokens.push_back({.type = TokenType::whileLoop});
                        buffer.clear();
                    }
                    else if(buffer == "and") {
                        tokens.push_back({.type = TokenType::andOperator});
                        buffer.clear();
                    }
                    else if(buffer == "or") {
                        tokens.push_back({.type = TokenType::orOperator});
                        buffer.clear();
                    }
                    else if(buffer == "not") {
                        tokens.push_back({.type = TokenType::notOperator});
                        buffer.clear();
                    }
                    else {
                        tokens.push_back({.type = TokenType::identifier, .value = buffer});
                        buffer.clear();
                    }
                }
                else if(std::isdigit(c)) {
                    buffer.push_back(consume());

                    while(peek().has_value() && std::isdigit(peek().value())) {
                        buffer.push_back(consume());
                    }

                    tokens.push_back({.type = TokenType::intLiteral, .value = buffer});
                    buffer.clear();
                }
                else if(std::isspace(c)) {
                    consume();
                }
                else if(c == '=') {
                    consume();

                    if(tryPeek('=')) {
                        tokens.push_back({.type = TokenType::equalTo});
                        consume();
                    }
                    else tokens.push_back({.type = TokenType::assignment});
                }
                else if(c == '<') {
                    consume();

                    if(tryPeek('=')) {
                        tokens.push_back({.type = TokenType::lessThanOrEqual});
                        consume();
                    }
                    else tokens.push_back({.type = TokenType::lessThan});
                }
                else if(c == '>') {
                    consume();

                    if(tryPeek('=')) {
                        tokens.push_back({.type = TokenType::greaterThanOrEqual});
                        consume();
                    }
                    else tokens.push_back({.type = TokenType::greaterThan});
                }
                else if(c == '!' && tryPeek('=', 1)) {
                    consume();
                    consume();

                    tokens.push_back({.type = TokenType::notEqualTo});
                }
                else {
                    switch(c) {
                        case '(':
                            tokens.push_back({.type = TokenType::openParenthesis});
                            consume();
                            break;
                        case ')':
                            tokens.push_back({.type = TokenType::closeParenthesis});
                            consume();
                            break;
                        case ';':
                            tokens.push_back({.type = TokenType::semiCol});
                            consume();
                            break;
                        case '+':
                            tokens.push_back({.type = TokenType::addition});
                            consume();
                            break;
                        case '-':
                            tokens.push_back({.type = TokenType::subtraction});
                            consume();
                            break;
                        case '*':
                            tokens.push_back({.type = TokenType::multiplication});
                            consume();
                            break;
                        case '/':
                            tokens.push_back({.type = TokenType::division});
                            consume();
                            break;
                        case '%':
                            tokens.push_back({.type = TokenType::modulo});
                            consume();
                            break;
                        case '{':
                            tokens.push_back({.type = TokenType::openCurlyBrace});
                            consume();
                            break;
                        case '}':
                            tokens.push_back({.type = TokenType::closeCurlyBrace});
                            consume();
                            break;
                        default:
                            std::cerr << "Invalid a" << std::endl;
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