#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "tokens.hpp"
#include "../util/dataType.hpp"

namespace Tokenizer {
    class Tokenizer {
        public:
            explicit Tokenizer(const std::string& src);
            std::vector<Token> tokenize();

        private:
            // Tokenizer functions
            Token readWord();
            Token readNumber();
            Token readCharLiteral();
            Token readStringLiteral();
            Token readSymbol();
            void skipLine();

            // Read functions
            [[nodiscard]] std::optional<char> peek(int ahead = 0) const;
            [[nodiscard]] bool tryPeek(char c, int ahead = 0) const;

            char consume();
            char consumeCharacter();

            // Miscellaneous
            [[nodiscard]] Token makeToken(TokenType type, std::string value = "") const;
            [[noreturn]] void throwError(int lineNumber, std::string message);

            const std::string m_src;
            int m_Index = 0;

            int m_curLine = 1;
    };
}