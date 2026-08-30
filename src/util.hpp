#pragma once

#include <string>

enum class TokenType {
    exit,
    print,
    newKeyWord,
    intLiteral,
    intType,
    boolLiteral,
    boolType,
    charLiteral,
    charType,
    stringType,
    stringLiteral,
    arrayType,
    identifier,
    set,
    assignment,
    semiCol,
    openParenthesis,
    closeParenthesis,
    openCurlyBrace,
    closeCurlyBrace,
    openBracket,
    closeBracket,
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
    whileLoop,
    dot,
    size,
    length
};

enum class DataType {
    Integer,
    Boolean,
    Character,
    IntArray,
    BoolArray,
    CharArray,
    String,
    Empty,
    None
};

enum class GroupType {
    Primitive,
    Arrays,
    Strings,
    None
};

DataType getPrimitiveVariant(DataType dataType) {
    switch(dataType) {
        case DataType::IntArray:
            return DataType::Integer;
        case DataType::BoolArray:
            return DataType::Boolean;
        case DataType::CharArray:
            return DataType::Character;
        default:
            return DataType::None;
    }
}

GroupType getGroupType(DataType dataType) {
    switch(dataType) {
        case DataType::Integer:
        case DataType::Boolean:
        case DataType::Character:
        case DataType::Empty:
            return GroupType::Primitive;
        case DataType::IntArray:
        case DataType::BoolArray:
        case DataType::CharArray:
            return GroupType::Arrays;
        case DataType::String:
            return GroupType::Strings;
        default:
            return GroupType::None;
    }
}

std::string tokenToString(TokenType type) {
    switch(type) {
        case TokenType::exit:
            return "exit";
        case TokenType::print:
            return "print";
        case TokenType::intType:
            return "int";
        case TokenType::boolType:
            return "bool";
        case TokenType::charType:
            return "char";
        case TokenType::stringType:
            return "string";
        case TokenType::newKeyWord:
            return "new";
        case TokenType::arrayType:
            return "Array";
        case TokenType::set:
            return "set";
        case TokenType::assignment:
            return "=";
        case TokenType::semiCol:
            return ";";
        case TokenType::openParenthesis:
            return "(";
        case TokenType::closeParenthesis:
            return ")";
        case TokenType::openCurlyBrace:
            return "{";
        case TokenType::closeCurlyBrace:
            return "}";
        case TokenType::openBracket:
            return "[";
        case TokenType::closeBracket:
            return "]";
        case TokenType::addition:
            return "+";
        case TokenType::subtraction:
            return "-";
        case TokenType::multiplication:
            return "*";
        case TokenType::division:
            return "/";
        case TokenType::modulo:
            return "%";
        case TokenType::lessThan:
            return "<";
        case TokenType::greaterThan:
            return ">";
        case TokenType::lessThanOrEqual:
            return "<=";
        case TokenType::greaterThanOrEqual:
            return "=>";
        case TokenType::equalTo:
            return "==";
        case TokenType::notEqualTo:
            return "!=";
        case TokenType::andOperator:
            return "and";
        case TokenType::orOperator:
            return "or";
        case TokenType::notOperator:
            return "not";
        case TokenType::ifStatement:
            return "if";
        case TokenType::elseIfStatement:
            return "else if";
        case TokenType::elseStatement:
            return "else";
        case TokenType::whileLoop:
            return "while";
        case TokenType::dot:
            return ".";
        case TokenType::size:
            return "size";
        default:
            return "";
    }
}