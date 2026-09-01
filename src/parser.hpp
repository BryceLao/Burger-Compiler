#pragma once

#include <variant>
#include <unordered_map>

#include "tokenizer.hpp"
#include "arenaAllocator.hpp"
#include "util.hpp"

struct ExpressionNode;

struct LiteralTerm {
    Token literal;
};

struct IdentifierTerm {
    Token identifier;
};

struct IndexedTerm {
    Token identifier;
    ExpressionNode* index;
};

struct PropertyTerm {
    Token identifier;
    TokenType property;
};

struct InputTerm {
    TokenType readType;
    std::optional<ExpressionNode*> inputMessage;
};

struct TypeCastTerm {
    TokenType typeCast;
    ExpressionNode* expression;
};

struct UnaryOperationExpression {
    TokenType _operator;
    ExpressionNode* expression;
};

struct OperationExpressionNode {
    ExpressionNode* left;
    ExpressionNode* right;
    TokenType _operator;
};

struct ParenthesisTerm {
    ExpressionNode* expression;
};

struct TermExpressionNode {
    DataType type;
    int lineNumber;
    std::variant<LiteralTerm*,IdentifierTerm*, IndexedTerm*, PropertyTerm*, InputTerm*, TypeCastTerm*, UnaryOperationExpression*, ParenthesisTerm*> variant;
};

struct ExpressionNode {
    DataType type;
    int lineNumber;
    std::variant<TermExpressionNode*, OperationExpressionNode*> variant;
};

struct StatementNode;

struct ExitNode {
    ExpressionNode* expression;
};

struct PrintNode {
    ExpressionNode* expression;
};

struct DeclarationNode {
    Token identifier;
    DataType type;
    ExpressionNode* expression;
};

struct ReAssignmentNode {
    Token identifier;
    ExpressionNode* expression;
    std::optional<ExpressionNode*> index;
};

struct ScopeNode {
    std::vector<StatementNode*> statements;
};

struct ConditionalNode {
    std::vector<ExpressionNode*> condition = {};
    std::vector<std::vector<StatementNode*>> statements = {};
};

struct LoopNode {
    ExpressionNode* condition;
    std::vector<StatementNode*> statements = {};
};

struct StatementNode{
    int lineNumber;
    std::variant<ExitNode*, PrintNode*, DeclarationNode*, ReAssignmentNode*, ScopeNode*, ConditionalNode*, LoopNode*> variant;
};

struct ProgramNode {
    std::vector<StatementNode*> statements;
};

class Parser {
    public:
        inline explicit Parser(std::vector<Token> tokens):
            m_Tokens(std::move(tokens)),
            m_ArenaAllocator(1024 * 1024 * 4){};

        std::optional<TermExpressionNode*> parseTerm(int minimumPrecedence = 0) {
            if(tryPeek(TokenType::intLiteral) || tryPeek(TokenType::boolLiteral) || tryPeek(TokenType::charLiteral) ||
                    tryPeek(TokenType::stringLiteral)) {
                auto literalExpression = m_ArenaAllocator.allocate<LiteralTerm>();
                literalExpression->literal = consume();

                auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
                termExpression->variant = literalExpression;
                termExpression->lineNumber = peek(-1).value().lineNumber;

                switch(literalExpression->literal.type) {
                    case TokenType::intLiteral:
                        termExpression->type = DataType::Integer;
                        break;
                    case TokenType::boolLiteral:
                        termExpression->type = DataType::Boolean;
                        break;
                    case TokenType::charLiteral:
                        termExpression->type = DataType::Character;
                        break;
                    case TokenType::stringLiteral:
                        termExpression->type = DataType::String;
                        break;
                    default:
                        throwError(literalExpression->literal.lineNumber, "Internal compiler error: Unknown data type");
                }

                return termExpression;
            }
            else if(tryPeek(TokenType::identifier)) {
                Token term = consume();

                if(m_variables.find(term.value.value()) != m_variables.end()) {
                    auto dataType = m_variables[term.value.value()];

                    if(tryPeek(TokenType::openBracket))  {
                        if(getGroupType(dataType) == GroupType::Primitive)
                            throwError(peek().value().lineNumber, "Error: Cannot index primitive variable '" + term.value.value() + "'");


                        auto indexedTerm = m_ArenaAllocator.allocate<IndexedTerm>();
                        consume();

                        auto expression = parseExpression();
                        if(!expression.has_value()) expectedExpressionError(peek(-1).value().lineNumber);

                        if(!tryPeek(TokenType::closeBracket)) expectedCharacterError(peek(-1).value().lineNumber, ']');
                        consume();

                        indexedTerm->identifier = term;
                        indexedTerm->index = expression.value();

                        auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
                        termExpression->variant = indexedTerm;
                        termExpression->lineNumber = peek(-1).value().lineNumber;

                        if(getGroupType(dataType) == GroupType::Arrays) termExpression->type = getPrimitiveVariant(dataType);
                        else if(dataType == DataType::String) termExpression->type = DataType::Character;

                        return termExpression;
                    }
                    else if(tryPeek(TokenType::dot)) {
                        consume();

                        auto property = peek();

                        if(!peek().has_value()) throwError(peek(-1).value().lineNumber, "Error: Expected a property after '.'");


                        if((tryPeek(TokenType::size) && getGroupType(dataType) != GroupType::Arrays) ||
                           (tryPeek(TokenType::length) && dataType != DataType::String))
                            throwError(peek().value().lineNumber,"Error: Property '" + tokenToString(peek().value().type) +
                            "' does not exist on type '" + dataTypeToString(dataType) + "'");

                        consume();

                        if(!tryPeek(TokenType::openParenthesis)) expectedCharacterError(peek(-1).value().lineNumber, '(');
                        consume();

                        if(!tryPeek(TokenType::closeParenthesis)) expectedCharacterError(peek(-1).value().lineNumber, ')');
                        consume();

                        auto propertyTerm = m_ArenaAllocator.allocate<PropertyTerm>();
                        propertyTerm->identifier = term;


                        auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
                        termExpression->variant = propertyTerm;
                        termExpression->lineNumber = term.lineNumber;

                        switch(property.value().type) {
                            case TokenType::size:
                                propertyTerm->property = TokenType::size;
                                termExpression->type = DataType::Integer;
                                break;
                            case TokenType::length:
                                propertyTerm->property = TokenType::length;
                                termExpression->type = DataType::Integer;
                                break;
                            default:
                                throwError(peek(-1).value().lineNumber, "Error: Property '" + tokenToString(property.value().type) +
                                "' does not exist on type '" + dataTypeToString(dataType) + "'");
                        }

                        return termExpression;
                    }
                    else {
                        auto identifierTerm = m_ArenaAllocator.allocate<IdentifierTerm>();
                        identifierTerm->identifier = term;

                        auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
                        termExpression->variant = identifierTerm;
                        termExpression->type = dataType;
                        termExpression->lineNumber = peek(-1).value().lineNumber;

                        return termExpression;
                    }
                }
                else throwError(peek(-1).value().lineNumber, "Error: Use of undeclared variable '" + term.value.value() + "'");
            }
            else if(tryPeek(TokenType::toString) || tryPeek(TokenType::stoi) ||
                    tryPeek(TokenType::intType) || tryPeek(TokenType::boolType) || tryPeek(TokenType::charType)) {
                auto typeCastTerm = m_ArenaAllocator.allocate<TypeCastTerm>();
                typeCastTerm->typeCast = consume().type;

                if(!tryPeek(TokenType::openParenthesis)) expectedCharacterError(peek(-1).value().lineNumber, '(');
                consume();

                if(auto expression = parseExpression()) typeCastTerm->expression = expression.value();
                else expectedExpressionError(peek(-1).value().lineNumber);

                if(!tryPeek(TokenType::closeParenthesis)) expectedCharacterError(peek(-1).value().lineNumber, ')');
                consume();

                GroupType expectedDataType;
                DataType newDataType;

                switch(typeCastTerm->typeCast) {
                    case TokenType::intType:
                        expectedDataType = GroupType::Primitive;
                        newDataType = DataType::Integer;
                        break;
                    case TokenType::boolType:
                        expectedDataType = GroupType::Primitive;
                        newDataType = DataType::Boolean;
                        break;
                    case TokenType::charType:
                        expectedDataType = GroupType::Primitive;
                        newDataType = DataType::Character;
                        break;
                    case TokenType::toString:
                        expectedDataType = GroupType::Primitive;
                        newDataType = DataType::String;
                        break;
                    case TokenType::stoi:
                        expectedDataType = GroupType::Strings;
                        newDataType = DataType::Integer;
                        break;
                    default:
                        throwError(peek(-1).value().lineNumber, "Internal compiler error: Unknown type cast '" +
                                tokenToString(typeCastTerm->typeCast) + "'");
                }

                if(expectedDataType != getGroupType(typeCastTerm->expression->type))
                    throwError(peek(-1).value().lineNumber, "Error: Type cast '" + tokenToString(typeCastTerm->typeCast) +
                    "' cannot be used with type '" + dataTypeToString(typeCastTerm->expression->type) + "'");

                auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
                termExpression->variant = typeCastTerm;
                termExpression->type = newDataType;
                termExpression->lineNumber = peek(-1).value().lineNumber;

                return termExpression;
            }
            else if(tryPeek(TokenType::readInt) || tryPeek(TokenType::readBool) || tryPeek(TokenType::readChar) ||
                    tryPeek(TokenType::readNext) || tryPeek(TokenType::readLine)) {
                auto inputTerm = m_ArenaAllocator.allocate<InputTerm>();
                inputTerm->readType = consume().type;

                if(tryPeek(TokenType::openParenthesis)) consume();
                else expectedCharacterError(peek(-1).value().lineNumber, '(');

                auto expression = parseExpression();

                if(expression.has_value() && expression.value()->type != DataType::String)
                    throwError(expression.value()->lineNumber, "Error: Argument of '" + tokenToString(inputTerm->readType) + "' must be type 'string'");
                inputTerm->inputMessage = expression;

                if(tryPeek(TokenType::closeParenthesis)) consume();
                else expectedCharacterError(peek(-1).value().lineNumber, ')');

                auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
                termExpression->variant = inputTerm;
                termExpression->lineNumber = peek(-1).value().lineNumber;

                switch(inputTerm->readType) {
                    case TokenType::readInt:
                        termExpression->type = DataType::Integer;
                        break;
                    case TokenType::readBool:
                        termExpression->type = DataType::Boolean;
                        break;
                    case TokenType::readChar:
                        termExpression->type = DataType::Character;
                        break;
                    case TokenType::readNext:
                    case TokenType::readLine:
                        termExpression->type = DataType::String;
                        break;
                    default:
                        throwError(termExpression->lineNumber, "Internal compiler error: Unknown method");
                }

                return termExpression;
            }
            else if(tryPeek(TokenType::openParenthesis)) {
                consume();

                auto expression = parseExpression();

                if(!expression.has_value()) expectedExpressionError(peek(-1).value().lineNumber);

                if(tryPeek(TokenType::closeParenthesis)) consume();
                else expectedCharacterError(peek(-1).value().lineNumber, ')');

                auto parenthesisTerm = m_ArenaAllocator.allocate<ParenthesisTerm>();
                parenthesisTerm->expression = expression.value();

                auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
                termExpression->variant = parenthesisTerm;
                termExpression->type = expression.value()->type;
                termExpression->lineNumber = peek(-1).value().lineNumber;

                return termExpression;
            }
            else {
                auto unaryExpression = m_ArenaAllocator.allocate<UnaryOperationExpression>();
                Token _operator;
                int precedenceLevel;

                if(tryPeek(TokenType::subtraction)) precedenceLevel = HIGHEST_PRECEDENCE_LEVEL;
                else if(tryPeek(TokenType::notOperator)) precedenceLevel = std::max(minimumPrecedence, NOT_OPERATOR_PRECEDENCE_LEVEL);
                else return {};

                _operator = consume();
                unaryExpression->_operator = _operator.type;

                if(auto expression = parseExpression(precedenceLevel)) unaryExpression->expression = expression.value();
                else expectedExpressionError(_operator.lineNumber);

                if(getGroupType(unaryExpression->expression->type) != GroupType::Primitive)
                    throwError(_operator.lineNumber, "Error: Unary operator '" + tokenToString(_operator.type) + "' cannot be applied to type '" +
                            dataTypeToString(unaryExpression->expression->type) + "'");

                auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
                termExpression->variant = unaryExpression;
                termExpression->lineNumber = peek().value().lineNumber;

                switch(_operator.type) {
                    case TokenType::subtraction:
                        termExpression->type = unaryExpression->expression->type;
                        break;
                    case TokenType::notOperator:
                        termExpression->type = DataType::Boolean;
                        break;
                    default:
                        throwError(termExpression->lineNumber, "Internal compiler error: Unknown operator");
                }

                return termExpression;
            }
        }

        std::optional<ExpressionNode*> parseExpression(int minimumPrecedence = 0) {
            std::optional<TermExpressionNode*> term = parseTerm(minimumPrecedence);

            if(!term.has_value()) return {};

            ExpressionNode* left = m_ArenaAllocator.allocate<ExpressionNode>();
            left->variant = term.value();
            left->type = term.value()->type;
            left->lineNumber = peek(-1).value().lineNumber;

            while(true) {
                if(!peek().has_value()) break;
                std::optional<Token> curToken = peek().value();

                std::optional<int> precedenceLevel = getPrecedenceLevel(curToken->type);
                if(!precedenceLevel.has_value() || precedenceLevel.value() < minimumPrecedence) break;

                Token operatorToken = consume();

                std::optional<ExpressionNode*> right;

                right = parseExpression(precedenceLevel.value() + 1);
                if(!right.has_value()) expectedExpressionError(peek(-1).value().lineNumber);

                auto operationExpression = m_ArenaAllocator.allocate<OperationExpressionNode>();

                switch(operatorToken.type) {
                    case TokenType::addition:
                        operationExpression->_operator = TokenType::addition;
                        break;
                    case TokenType::subtraction:
                        operationExpression->_operator = TokenType::subtraction;
                        break;
                    case TokenType::multiplication:
                        operationExpression->_operator = TokenType::multiplication;
                        break;
                    case TokenType::division:
                        operationExpression->_operator = TokenType::division;
                        break;
                    case TokenType::modulo:
                        operationExpression->_operator = TokenType::modulo;
                        break;
                    case TokenType::equalTo:
                        operationExpression->_operator = TokenType::equalTo;
                        break;
                    case TokenType::notEqualTo:
                        operationExpression->_operator = TokenType::notEqualTo;
                        break;
                    case TokenType::lessThan:
                        operationExpression->_operator = TokenType::lessThan;
                        break;
                    case TokenType::lessThanOrEqual:
                        operationExpression->_operator = TokenType::lessThanOrEqual;
                        break;
                    case TokenType::greaterThan:
                        operationExpression->_operator = TokenType::greaterThan;
                        break;
                    case TokenType::greaterThanOrEqual:
                        operationExpression->_operator = TokenType::greaterThanOrEqual;
                        break;
                    case TokenType::andOperator:
                        operationExpression->_operator = TokenType::andOperator;
                        break;
                    case TokenType::orOperator:
                        operationExpression->_operator = TokenType::orOperator;
                        break;
                    default:
                        throwError(operatorToken.lineNumber, "Error: Unexpected operator '" + tokenToString(operatorToken.type) + "'");
                }

                if(getGroupType(left->type) == GroupType::Primitive &&
                        getGroupType(right.value()->type) == GroupType::Primitive) {
                    auto temp = m_ArenaAllocator.allocate<ExpressionNode>(); //Prevents pointer loop

                    temp->variant = left->variant;
                    temp->type = left->type;

                    operationExpression->left = temp;
                    operationExpression->right = right.value();

                    left->variant = operationExpression;

                    auto _operator = operationExpression->_operator;
                    if (_operator == TokenType::addition || _operator == TokenType::subtraction ||
                        _operator == TokenType::multiplication
                        || _operator == TokenType::division || _operator == TokenType::modulo) {
                        left->type = DataType::Integer;
                    }
                    else if (_operator == TokenType::equalTo || _operator == TokenType::notEqualTo ||
                               _operator == TokenType::lessThan
                               || _operator == TokenType::lessThanOrEqual || _operator == TokenType::greaterThan ||
                               _operator == TokenType::greaterThanOrEqual
                               || _operator == TokenType::notOperator || _operator == TokenType::andOperator ||
                               _operator == TokenType::orOperator) {
                        left->type = DataType::Boolean;
                    }
                }
                else if(isTextType(left->type) && isTextType(right.value()->type)) {
                    if(left->type == DataType::Character) {
                        auto newLeft = parseCharAsString(left);
                        left = newLeft;
                    }
                    if(right.value()->type == DataType::Character) {
                        auto newRight = parseCharAsString(right.value());
                        right = newRight;
                    }

                    auto temp = m_ArenaAllocator.allocate<ExpressionNode>(); //Prevents pointer loop

                    temp->variant = left->variant;
                    temp->type = left->type;

                    operationExpression->left = temp;
                    operationExpression->right = right.value();

                    left->variant = operationExpression;

                    auto _operator = operationExpression->_operator;
                    if (_operator == TokenType::addition) left->type = DataType::String;
                    else if (_operator == TokenType::equalTo || _operator == TokenType::notEqualTo ||
                               _operator == TokenType::lessThan
                               || _operator == TokenType::lessThanOrEqual || _operator == TokenType::greaterThan ||
                               _operator == TokenType::greaterThanOrEqual) {
                        left->type = DataType::Boolean;
                    }
                    else throwError(peek(-1).value().lineNumber, "Error: Operator '" +
                    tokenToString(operationExpression->_operator) + "' cannot be applied to types 'string' and 'string'");
                }
                else {
                    throwError(peek(-1).value().lineNumber, "Error: Operator '" + tokenToString(operationExpression->_operator) +
                    "' cannot be applied to types '" + dataTypeToString(left->type) + "' and '" + dataTypeToString(right.value()->type) + "'");
                }
            }

            return left;
        }

        std::optional<std::vector<StatementNode*>> parseScope() {
            std::vector<StatementNode*> statements;

            while(peek().has_value() && peek().value().type != TokenType::closeCurlyBrace) {
                statements.push_back(parseStatement().value());
            }

            if(!tryPeek(TokenType::closeCurlyBrace)) expectedCharacterError(peek(-1).value().lineNumber, '}');

            consume();

            return statements;
        }

        std::optional<StatementNode*> parseStatement() {
            if(tryPeek(TokenType::exit)) {
                consume();

                if(!tryPeek(TokenType::openParenthesis)) expectedCharacterError(peek(-1).value().lineNumber, '(');
                consume();

                auto exitNode = m_ArenaAllocator.allocate<ExitNode>();

                if(auto expressionNode = parseExpression()) {
                    exitNode->expression = expressionNode.value();
                }
                else expectedExpressionError(peek(-1).value().lineNumber);

                if(tryPeek(TokenType::closeParenthesis)) {
                    consume();
                }
                else expectedCharacterError(peek(-1).value().lineNumber, ')');

                if(tryPeek(TokenType::semiCol)) {
                    consume();
                }
                else expectedCharacterError(peek(-1).value().lineNumber, ';');

                auto statementNode = m_ArenaAllocator.allocate<StatementNode>();
                statementNode->variant = exitNode;
                statementNode->lineNumber = peek(-1).value().lineNumber;

                return statementNode;
            }
            else if(tryPeek(TokenType::print)) {
                consume();

                if(!tryPeek(TokenType::openParenthesis)) expectedCharacterError(peek(-1).value().lineNumber, '(');
                consume();

                auto printNode = m_ArenaAllocator.allocate<PrintNode>();

                if(auto expressionNode = parseExpression()) {
                    printNode->expression = expressionNode.value();
                }
                else expectedExpressionError(peek(-1).value().lineNumber);

                if(tryPeek(TokenType::closeParenthesis)) {
                    consume();
                }
                else expectedCharacterError(peek(-1).value().lineNumber, ')');

                if(tryPeek(TokenType::semiCol)) {
                    consume();
                }
                else expectedCharacterError(peek(-1).value().lineNumber, ';');

                if(getGroupType(printNode->expression->type) == GroupType::Arrays)
                    throwError(peek(-1).value().lineNumber, "Error: Unable to print expression of type '" +
                    dataTypeToString(printNode->expression->type)  + "'");


                auto statementNode = m_ArenaAllocator.allocate<StatementNode>();
                statementNode->variant = printNode;
                statementNode->lineNumber = peek(-1).value().lineNumber;

                return statementNode;
            }
            else if(tryPeek(TokenType::set)) {
                consume();

                if(!peek().has_value()) throwError(peek(-1).value().lineNumber, "Error: Expected a data type after 'set'");

                auto dataType = peek().value().type; consume();

                if(tryPeek(TokenType::openBracket)) {
                    auto declarationNode = m_ArenaAllocator.allocate<DeclarationNode>();
                    consume();

                    if(!tryPeek(TokenType::closeBracket)) expectedCharacterError(peek(-1).value().lineNumber, ']');
                    consume();

                    switch(dataType) {
                        case TokenType::intType:
                            declarationNode->type = DataType::IntArray;
                            break;
                        case TokenType::boolType:
                            declarationNode->type = DataType::BoolArray;
                            break;
                        case TokenType::charType:
                            declarationNode->type = DataType::CharArray;
                            break;
                        default:
                            throwError(peek(-1).value().lineNumber, "Error: Expected a data type after 'set'");
                    }

                    if(!tryPeek(TokenType::identifier)) throwError(peek(-1).value().lineNumber, "Error: Expected variable identifier");
                    declarationNode->identifier = consume();

                    if(!tryPeek(TokenType::assignment)) expectedCharacterError(peek(-1).value().lineNumber, '=');
                    consume();

                    if(!tryPeek(TokenType::newKeyWord)) throwError(peek(-1).value().lineNumber, "Error: expected 'new' before array allocation");
                    consume();

                    if(!tryPeek(TokenType::arrayType)) throwError(peek(-1).value().lineNumber, "Error: expected 'Array' after 'new'");
                    consume();

                    if(!tryPeek(TokenType::openBracket)) expectedCharacterError(peek(-1).value().lineNumber, '[');
                    consume();

                    if(auto expressionNode = parseExpression()) {
                        declarationNode->expression = expressionNode.value();
                    }
                    else expectedExpressionError(peek(-1).value().lineNumber);

                    if(!tryPeek(TokenType::closeBracket)) expectedCharacterError(peek(-1).value().lineNumber, ']');
                    consume();

                    if(tryPeek(TokenType::semiCol)) {
                        consume();

                        auto statementNode = m_ArenaAllocator.allocate<StatementNode>();
                        statementNode->variant = declarationNode;
                        statementNode->lineNumber = peek(-1).value().lineNumber;

                        m_variables[declarationNode->identifier.value.value()] = declarationNode->type;

                        return statementNode;
                    }
                    else expectedCharacterError(peek(-1).value().lineNumber, ';');
                }
                else if(dataType == TokenType::stringType) {
                    // Special handling if a string - not an array, not a primitive

                    auto declarationNode = m_ArenaAllocator.allocate<DeclarationNode>();
                    declarationNode->type = DataType::String;

                    if(!tryPeek(TokenType::identifier)) throwError(peek(-1).value().lineNumber, "Error: Expected variable identifier");
                    declarationNode->identifier = consume();

                    if(!tryPeek(TokenType::assignment)) expectedCharacterError(peek(-1).value().lineNumber, '=');
                    consume();

                    if(auto expressionNode = parseExpression()) declarationNode->expression = expressionNode.value();
                    else expectedExpressionError(peek(-1).value().lineNumber);

                    if(tryPeek(TokenType::semiCol)) {
                        consume();

                        auto statementNode = m_ArenaAllocator.allocate<StatementNode>();
                        statementNode->variant = declarationNode;
                        statementNode->lineNumber = peek(-1).value().lineNumber;

                        m_variables[declarationNode->identifier.value.value()] = DataType::String;

                        return statementNode;
                    }
                    else expectedCharacterError(peek(-1).value().lineNumber, ';');
                }
                else {
                    auto declarationNode = m_ArenaAllocator.allocate<DeclarationNode>();

                    switch(dataType) {
                        case TokenType::intType:
                            declarationNode->type = DataType::Integer;
                            break;
                        case TokenType::boolType:
                            declarationNode->type = DataType::Boolean;
                            break;
                        case TokenType::charType:
                            declarationNode->type = DataType::Character;
                            break;
                        default:
                            throwError(peek(-1).value().lineNumber, "Error: Expected a data type after 'set'");
                    }

                    if(!tryPeek(TokenType::identifier)) throwError(peek(-1).value().lineNumber, "Error: Expected variable identifier");
                    declarationNode->identifier = consume();

                    if(!tryPeek(TokenType::assignment)) expectedCharacterError(peek(-1).value().lineNumber, '=');
                    consume();

                    if(auto expressionNode = parseExpression()) {
                        declarationNode->expression = expressionNode.value();
                    }
                    else expectedExpressionError(peek(-1).value().lineNumber);

                    if(tryPeek(TokenType::semiCol)) {
                        consume();

                        auto statementNode = m_ArenaAllocator.allocate<StatementNode>();
                        statementNode->variant = declarationNode;
                        statementNode->lineNumber = peek(-1).value().lineNumber;

                        m_variables[declarationNode->identifier.value.value()] = declarationNode->type;

                        return statementNode;
                    }
                    else expectedCharacterError(peek(-1).value().lineNumber, ';');
                }
            }
            else if(tryPeek(TokenType::identifier)) {
                auto reAssignmentNode = m_ArenaAllocator.allocate<ReAssignmentNode>();
                reAssignmentNode->identifier = consume();

                bool isPrimitive = false;

                if(tryPeek(TokenType::openBracket)) {
                    consume();

                    auto indexExpression = parseExpression();
                    if(!indexExpression.has_value()) expectedExpressionError(peek(-1).value().lineNumber);
                    reAssignmentNode->index = indexExpression.value();

                    if(!tryPeek(TokenType::closeBracket)) expectedCharacterError(peek(-1).value().lineNumber, ']');
                    consume();

                    isPrimitive = true;
                }

                if(tryPeek(TokenType::assignment)) {
                    consume();

                    if(auto expression = parseExpression()) {
                        if(tryPeek(TokenType::semiCol)) {
                            consume();

                            reAssignmentNode->expression = expression.value();

                            auto statementNode = m_ArenaAllocator.allocate<StatementNode>();
                            statementNode->variant = reAssignmentNode;
                            statementNode->lineNumber = peek(-1).value().lineNumber;

                            if(getGroupType(m_variables[reAssignmentNode->identifier.value.value()]) == GroupType::Arrays && !isPrimitive)
                                throwError(statementNode->lineNumber, "Error: Cannot assign type '" +
                                dataTypeToString(reAssignmentNode->expression->type) + "' to type '" +
                                dataTypeToString(m_variables[reAssignmentNode->identifier.value.value()]) + "'");

                            return statementNode;
                        }
                        else expectedCharacterError(peek(-1).value().lineNumber, ';');
                    }
                    else expectedExpressionError(peek(-1).value().lineNumber);
                }
                else expectedCharacterError(peek(-1).value().lineNumber, '=');
            }
            else if(tryPeek(TokenType::openCurlyBrace)) {
                consume();

                auto scopeNode = m_ArenaAllocator.allocate<ScopeNode>();
                scopeNode->statements = parseScope().value();

                auto statementNode = m_ArenaAllocator.allocate<StatementNode>();
                statementNode->variant = scopeNode;
                statementNode->lineNumber = peek(-1).value().lineNumber;

                return statementNode;
            }
            else if(tryPeek(TokenType::ifStatement)) {
                consume();

                auto conditionalNode = m_ArenaAllocator.allocate<ConditionalNode>();

                if(auto initialCondition = parseExpression()) {
                    conditionalNode->condition.push_back(initialCondition.value());

                    if(tryPeek(TokenType::openCurlyBrace)) {
                        consume();

                        conditionalNode->statements.push_back(parseScope().value());

                        while(tryPeek(TokenType::elseIfStatement)) {
                            consume();

                            if(auto condition = parseExpression()) {
                                conditionalNode->condition.push_back(condition.value());

                                if(tryPeek(TokenType::openCurlyBrace)) {
                                    consume();

                                    conditionalNode->statements.push_back(parseScope().value());
                                }
                                else expectedCharacterError(peek(-1).value().lineNumber, '{');
                            }
                            else expectedExpressionError(peek(-1).value().lineNumber);
                        }

                        if(tryPeek(TokenType::elseStatement)) {
                            consume();

                            if(tryPeek(TokenType::openCurlyBrace)) {
                                consume();

                                conditionalNode->statements.push_back(parseScope().value());
                            }
                            else expectedCharacterError(peek(-1).value().lineNumber, '{');
                        }

                        auto statementNode = m_ArenaAllocator.allocate<StatementNode>();
                        statementNode->variant = conditionalNode;
                        statementNode->lineNumber = peek(-1).value().lineNumber;

                        return statementNode;
                    }
                    else expectedCharacterError(peek(-1).value().lineNumber, '{');
                }
                else expectedExpressionError(peek(-1).value().lineNumber);
            }
            else if(tryPeek(TokenType::whileLoop)) {
                consume();

                auto loopNode = m_ArenaAllocator.allocate<LoopNode>();

                if(auto condition = parseExpression()) {
                    loopNode->condition = condition.value();

                    if(tryPeek(TokenType::openCurlyBrace)) {
                        consume();

                        loopNode->statements = parseScope().value();

                        auto statementNode = m_ArenaAllocator.allocate<StatementNode>();
                        statementNode->variant = loopNode;
                        statementNode->lineNumber = peek(-1).value().lineNumber;

                        return statementNode;
                    }
                    else expectedCharacterError(peek(-1).value().lineNumber, '{');
                }
                else expectedExpressionError(peek(-1).value().lineNumber);
            }
            else return {};
        }

       std::optional<ProgramNode> parseProgram() {
            ProgramNode program;

            while(peek().has_value()) {
                if(auto statement = parseStatement()) program.statements.push_back(statement.value());
                else throwError(peek().value().lineNumber, "Error: Couldn't parse statement");
            }

            return program;
       }

    private:
        [[nodiscard]] inline bool tryPeek(TokenType type, int ahead = 0) const {
            if(m_Index + ahead >= m_Tokens.size()) return false;

            return type == peek(ahead).value().type;
        }

        [[nodiscard]] inline std::optional<Token> peek(int ahead = 0) const {
            if(m_Index + ahead >= m_Tokens.size()) return {};
            else return m_Tokens.at(m_Index + ahead);
        }

        inline Token consume() {
            return m_Tokens.at(m_Index++);
        }

        void expectedCharacterError(int lineNumber, char expectedCharacter) {
            if(peek().has_value())
                throwError(lineNumber, "Error: Expected '" + std::string(1, expectedCharacter) +
                "' but found " + tokenToString(peek().value().type));
            else throwError(lineNumber, "Error: Expected '" + std::string(1, expectedCharacter) + "' but found none");
        }

        void expectedExpressionError(int lineNumber) {
            throwError(lineNumber, "Error: Expected expression after '" + tokenToString(peek(-1).value().type) + "'");
        }

        ExpressionNode* parseCharAsString(ExpressionNode* charExpression) {
            auto methodTerm = m_ArenaAllocator.allocate<TypeCastTerm>();
            methodTerm->typeCast = TokenType::toString;
            methodTerm->expression = charExpression;

            auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
            termExpression->variant = methodTerm;
            termExpression->type = DataType::String;
            termExpression->lineNumber = charExpression->lineNumber;

            auto expression = m_ArenaAllocator.allocate<ExpressionNode>();
            expression->variant = termExpression;
            expression->type = DataType::String;
            expression->lineNumber = charExpression->lineNumber;

            return expression;
        }

        bool isTextType(DataType dataType) {
            return dataType == DataType::String || dataType == DataType::Character;
        }

        const std::vector<Token> m_Tokens;
        size_t m_Index = 0;
        ArenaAllocator m_ArenaAllocator;
        std::unordered_map<std::string, DataType> m_variables;

        static constexpr int HIGHEST_PRECEDENCE_LEVEL = 6;
        static constexpr int NOT_OPERATOR_PRECEDENCE_LEVEL = 2;
};