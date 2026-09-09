#include <optional>

namespace Tokenizer {
    enum class TokenType;

    struct Token {
        TokenType type;
        int lineNumber;
        std::optional<std::string> value = {};
    };

    enum class TokenType {
        // Statements and keywords
        exit,
        print,
        set,
        newKeyword,
        ifStatement,
        elseIfStatement,
        elseStatement,
        whileLoop,
        define,
        returnCall,
        call,

        // Data types
        intType,
        boolType,
        charType,
        stringType,
        arrayType,
        voidType,

        // Literals and Identifier
        intLiteral,
        boolLiteral,
        charLiteral,
        stringLiteral,
        identifier,

        // Built-In functions
        size,
        length,
        toString,
        stoi,
        readInt,
        readBool,
        readChar,
        readNext,
        readLine,

        // Operators
        assignment,
        equalTo,
        notEqualTo,
        lessThan,
        lessThanOrEqual,
        greaterThan,
        greaterThanOrEqual,
        addition,
        subtraction,
        multiplication,
        division,
        modulo,
        andOperator,
        orOperator,
        notOperator,

        // Miscellaneous syntax
        openParenthesis,
        closeParenthesis,
        openCurlyBrace,
        closeCurlyBrace,
        openBracket,
        closeBracket,
        semiColon,
        dot,
        comma
    };

    static std::unordered_map<std::string, TokenType> KEYWORDS = {
            {"exit", TokenType::exit},
            {"print", TokenType::print},
            {"set", TokenType::set},
            {"new", TokenType::newKeyword},
            {"if", TokenType::ifStatement},
            {"else", TokenType::elseStatement},
            {"while", TokenType::whileLoop},
            {"define", TokenType::define},
            {"return", TokenType::returnCall},
            {"call", TokenType::call},

            {"int", TokenType::intType},
            {"bool", TokenType::boolType},
            {"char", TokenType::charType},
            {"string", TokenType::stringType},
            {"Array", TokenType::arrayType},
            {"void", TokenType::voidType},

            {"size", TokenType::size},
            {"length", TokenType::length},
            {"toString", TokenType::toString},
            {"stoi", TokenType::stoi},
            {"readInt", TokenType::readInt},
            {"readBool", TokenType::readBool},
            {"readChar", TokenType::readChar},
            {"readNext", TokenType::readNext},
            {"readLine", TokenType::readLine},

            {"and", TokenType::andOperator},
            {"or", TokenType::orOperator},
            {"not", TokenType::notOperator},
    };

    static std::unordered_map<TokenType, std::string> tokenStrings = {
            {TokenType::exit, "exit"},
            {TokenType::print, "print"},
            {TokenType::intType, "int"},
            {TokenType::boolType, "bool"},
            {TokenType::charType, "char"},
            {TokenType::stringType, "string"},
            {TokenType::newKeyword, "new"},
            {TokenType::arrayType, "Array"},
            {TokenType::set, "set"},
            {TokenType::assignment, "="},
            {TokenType::semiColon, ";"},
            {TokenType::openParenthesis, "("},
            {TokenType::closeParenthesis, ")"},
            {TokenType::openCurlyBrace, "{"},
            {TokenType::closeCurlyBrace, "}"},
            {TokenType::openBracket, "["},
            {TokenType::closeBracket, "]"},
            {TokenType::addition, "+"},
            {TokenType::subtraction, "-"},
            {TokenType::multiplication, "*"},
            {TokenType::division, "/"},
            {TokenType::modulo, "%"},
            {TokenType::lessThan, "<"},
            {TokenType::greaterThan, ">"},
            {TokenType::lessThanOrEqual, "<="},
            {TokenType::greaterThanOrEqual, ">="},
            {TokenType::equalTo, "=="},
            {TokenType::notEqualTo, "!="},
            {TokenType::andOperator, "and"},
            {TokenType::orOperator, "or"},
            {TokenType::notOperator, "not"},
            {TokenType::ifStatement, "if"},
            {TokenType::elseIfStatement, "else if"},
            {TokenType::elseStatement, "else"},
            {TokenType::whileLoop, "while"},
            {TokenType::dot, "."},
            {TokenType::size, "size"},
            {TokenType::length, "size"},
            {TokenType::toString, "toString()"},
            {TokenType::stoi, "stoi()"},
            {TokenType::readInt, "readInt()"},
            {TokenType::readBool, "readBool()"},
            {TokenType::readChar, "readChar()"},
            {TokenType::readNext, "readNext()"},
            {TokenType::readLine, "readLine()"},
            {TokenType::call, "call"},
            {TokenType::returnCall, "return"},
            {TokenType::define, "define"},
            {TokenType::comma, ","},
            {TokenType::voidType, "void"},
    };

    inline std::string tokenToString(TokenType type) {
        if (tokenStrings.find(type) == tokenStrings.end()) return "";
        return tokenStrings[type];
    }
}