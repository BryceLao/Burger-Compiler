#include "tokenizer.hpp"

namespace Tokenizer {
    Tokenizer::Tokenizer(const std::string &src)
            : m_src(src + "\n") {}

    std::vector<Token> Tokenizer::tokenize() {
        std::vector<Token> tokens;

        while(peek().has_value()) {
            char curChar = peek().value();

            if(std::isspace(curChar)) consume();
            else if(std::isalpha(curChar)) tokens.push_back(readWord());
            else if(std::isdigit(curChar)) tokens.push_back(readNumber());
            else if(curChar == '"') tokens.push_back(readStringLiteral());
            else if(curChar == '\'') tokens.push_back(readCharLiteral());
            else if(curChar == '/' && tryPeek('/', 1)) skipLine();
            else tokens.push_back(readSymbol());
        }

        return tokens;
    }

    Token Tokenizer::readWord() {
        std::string word;

        while(peek().has_value() && std::isalnum(peek().value())) {
            word.push_back(consume());
        }

        if(word == "True") return makeToken(TokenType::boolLiteral, "1");
        if(word == "False") return makeToken(TokenType::boolLiteral, "0");

        if(word == "else" && tryPeek(' ', 0) && tryPeek('i', 1) && tryPeek('f', 2)) {
            consume(); consume(); consume();
            return makeToken(TokenType::elseIfStatement);
        }

        if(KEYWORDS.find(word) != KEYWORDS.end()) return makeToken(KEYWORDS[word]);
        return makeToken(TokenType::identifier, word);
    }

    Token Tokenizer::readNumber() {
        std::string number;

        while(peek().has_value() && std::isdigit(peek().value())) {
            number.push_back(consume());
        }

        return makeToken(TokenType::intLiteral, number);
    }

    Token Tokenizer::readStringLiteral() {
        std::string stringLiteral;
        consume();

        while(peek().has_value() && peek().value() != '"') {
            stringLiteral.push_back(consumeCharacter());
        }

        if(!peek().has_value()) throwError(m_curLine, "Error: Expected \" but found none");
        consume();

        return makeToken(TokenType::stringLiteral, stringLiteral);
    }

    Token Tokenizer::readCharLiteral() {
        char charLiteral;
        consume();

        if(!peek().has_value()) throwError(m_curLine, "Error: Expected ' but found none");
        charLiteral = consumeCharacter();

        if(!tryPeek('\'')) throwError(m_curLine, "Error: Character literal must hold exactly 1 character");
        consume();

        return makeToken(TokenType::charLiteral, std::to_string(static_cast<int>(charLiteral)));
    }

    Token Tokenizer::readSymbol() {
        char curChar = consume();

        switch(curChar) {
            case '=':
                if(tryPeek('=')) {
                    consume();
                    return makeToken(TokenType::equalTo);
                }
                return makeToken(TokenType::assignment);
            case '<':
                if(tryPeek('=')) {
                    consume();
                    return makeToken(TokenType::lessThanOrEqual);
                }
                return makeToken(TokenType::lessThan);
            case '>':
                if(tryPeek('=')) {
                    consume();
                    return makeToken(TokenType::greaterThanOrEqual);
                }
                return makeToken(TokenType::greaterThan);
            case '!':
                if(tryPeek('=')) {
                    consume();
                    return makeToken(TokenType::notEqualTo);
                }
                throwError(m_curLine, "Unexpected character '!'");
            case '+': return makeToken(TokenType::addition);
            case '-': return makeToken(TokenType::subtraction);
            case '*': return makeToken(TokenType::multiplication);
            case '/': return makeToken(TokenType::division);
            case '%': return makeToken(TokenType::modulo);

            case '(': return makeToken(TokenType::openParenthesis);
            case ')': return makeToken(TokenType::closeParenthesis);
            case '{': return makeToken(TokenType::openCurlyBrace);
            case '}': return makeToken(TokenType::closeCurlyBrace);
            case '[': return makeToken(TokenType::openBracket);
            case ']': return makeToken(TokenType::closeBracket);

            case '.': return makeToken(TokenType::dot);
            case ',': return makeToken(TokenType::comma);
            case ';': return makeToken(TokenType::semiColon);

            default: throwError(m_curLine, "Unexpected character '" + std::string(1, curChar) + "'");
        }
    }

    void Tokenizer::skipLine() {
        consume(); consume();

        while(peek().has_value() && !tryPeek('\n') || !tryPeek('\r')) {
            consume();
        }
    }

    Token Tokenizer::makeToken(TokenType type, std::string value) const {
        return {.type = type, .lineNumber = m_curLine, .value = std::move(value)};
    }

    std::optional<char> Tokenizer::peek(int ahead) const {
        if(m_Index + ahead >= m_src.length()) return {};
        else return m_src.at(m_Index + ahead);
    }

    bool Tokenizer::tryPeek(char c, int ahead) const {
        if(m_Index + ahead >= m_src.length()) return false;
        return c == peek(ahead).value();
    }

    char Tokenizer::consume() {
        if(peek().value() == '\n') ++m_curLine;
        return m_src.at(m_Index++);
    }

    char Tokenizer::consumeCharacter() {
        char curChar = consume();

        if(curChar == '\\') {
            char escapeCode = consume();
            switch(escapeCode) {
                case 'n': return '\n';
                case 't': return '\t';
                case 'r': return '\r';
                case 'b': return '\b';
                case 'f': return '\f';
                case 'v': return '\v';
                case 'a': return '\a';
                case '\\': return '\\';
                case '\'': return '\'';
                case '"': return '\"';
                default:
                    throwError(m_curLine, "Error: Unknown escape sequence '\\" + std::string(1, escapeCode));
            }
        }
        else return curChar;
    }

    void Tokenizer::throwError(int lineNumber, std::string message) {
        std::cerr << "Line " << lineNumber << ": " << message << std::endl;
        exit(EXIT_FAILURE);
    }
}