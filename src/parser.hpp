#pragma once

#include <variant>
#include <unordered_map>

#include "tokenizer.hpp"
#include "arenaAllocator.hpp"
#include "util.hpp"

enum class GroupType {
    Primitive,
    Arrays,
    Strings,
    None
};

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

struct OperationExpressionNode {
    ExpressionNode* left;
    ExpressionNode* right;
    TokenType _operator;
};

struct ParenthesisTerm {
    ExpressionNode* expression;
};

struct DummyTerm {

};

struct TermExpressionNode {
    DataType type;
    bool isNegative;
    int lineNumber;
    std::variant<LiteralTerm*,IdentifierTerm*, IndexedTerm*, PropertyTerm*, ParenthesisTerm*, DummyTerm*> variant;
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

struct PrimitiveDeclarationNode {
    Token identifier;
    DataType type;
    ExpressionNode* expression;
};

struct ArrayDeclarationNode {
    Token identifier;
    DataType type;
    ExpressionNode* size;
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
    std::variant<ExitNode*, PrintNode*, PrimitiveDeclarationNode*, ArrayDeclarationNode*, ReAssignmentNode*, ScopeNode*, ConditionalNode*, LoopNode*> variant;
};

struct ProgramNode {
    std::vector<StatementNode*> statements;
};

class Parser {
    public:
        inline explicit Parser(std::vector<Token> tokens):
            m_Tokens(std::move(tokens)),
            m_ArenaAllocator(1024 * 1024 * 4){};

        std::optional<TermExpressionNode*> parseTerm() {
            if(tryPeek(TokenType::intLiteral)) {
                auto literalExpression = m_ArenaAllocator.allocate<LiteralTerm>();
                literalExpression->literal = consume();

                auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
                termExpression->variant = literalExpression;
                termExpression->type = DataType::Integer;
                termExpression->lineNumber = peek(-1).value().lineNumber;

                return termExpression;
            }
            else if(tryPeek(TokenType::boolLiteral)) {
                auto literalExpression = m_ArenaAllocator.allocate<LiteralTerm>();
                literalExpression->literal = consume();

                auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
                termExpression->variant = literalExpression;
                termExpression->type = DataType::Boolean;
                termExpression->lineNumber = peek(-1).value().lineNumber;

                return termExpression;
            }
            else if(tryPeek(TokenType::charLiteral)) {
                auto literalExpression = m_ArenaAllocator.allocate<LiteralTerm>();
                literalExpression->literal = consume();

                auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
                termExpression->variant = literalExpression;
                termExpression->type = DataType::Character;
                termExpression->lineNumber = peek(-1).value().lineNumber;

                return termExpression;
            }
            else if(tryPeek(TokenType::identifier)) {
                Token term = consume();

                if(m_variables.find(term.value.value()) != m_variables.end()) {
                    if(tryPeek(TokenType::openBracket))  {
                        if(getGroupType(m_variables[term.value.value()]) == GroupType::Primitive) {
                            std::cerr << "Line " << peek().value().lineNumber << ": Error: Cannot index non-array variable '" << term.value.value() << "'"  << std::endl;
                            exit(EXIT_FAILURE);
                        }

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
                        termExpression->type = getPrimitiveVariant(m_variables[term.value.value()]);
                        termExpression->lineNumber = peek(-1).value().lineNumber;

                        return termExpression;
                    }
                    else if(tryPeek(TokenType::dot)) {
                        consume();

                        if(tryPeek(TokenType::size)) {
                            consume();

                            if(!tryPeek(TokenType::openParenthesis)) expectedCharacterError(peek(-1).value().lineNumber, '(');
                            consume();

                            if(!tryPeek(TokenType::closeParenthesis)) expectedCharacterError(peek(-1).value().lineNumber, ')');
                            consume();

                            auto propertyTerm = m_ArenaAllocator.allocate<PropertyTerm>();
                            propertyTerm->identifier = term;
                            propertyTerm->property = TokenType::size;

                            auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
                            termExpression->variant = propertyTerm;
                            termExpression->type = DataType::Integer;
                            termExpression->lineNumber = term.lineNumber;

                            return termExpression;
                        }
                        else {
                            std::cerr << "Line " << peek(-1).value().lineNumber << ": Error: Expected a property after '.'"  << std::endl;
                            exit(EXIT_FAILURE);
                        }
                    }
                    else {
                        auto identifierTerm = m_ArenaAllocator.allocate<IdentifierTerm>();
                        identifierTerm->identifier = term;

                        auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
                        termExpression->variant = identifierTerm;
                        termExpression->type = m_variables[term.value.value()];
                        termExpression->lineNumber = peek(-1).value().lineNumber;

                        return termExpression;
                    }
                }
                else {
                    std::cerr << "Line " << peek(-1).value().lineNumber << ": Error: Use of undeclared variable '" << term.value.value() << "'"  << std::endl;
                    exit(EXIT_FAILURE);
                }
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
            else if(tryPeek(TokenType::notOperator)) {
                auto dummyTerm = m_ArenaAllocator.allocate<DummyTerm>();

                auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
                termExpression->variant = dummyTerm;
                termExpression->lineNumber = peek().value().lineNumber;

                return termExpression;
            }
            else {
                std::cerr << "Line " << peek(-1).value().lineNumber << ": Error: Expected a term after '" << tokenToString(peek(-1).value().type) << "'"  << std::endl;
                exit(EXIT_FAILURE);
            }
        }

        std::optional<ExpressionNode*> parseExpression(int minimumPrecedence = 0) {
            // Non-Term section
            bool isNegative = tryPeek(TokenType::subtraction);
            DataType dataType = DataType::Empty;

            if(isNegative) consume();

            // Type casting
            if(tryPeek(TokenType::intType) && tryPeek(TokenType::openParenthesis, 1)) {
                consume(); consume();
                dataType = DataType::Integer;
            }
            else if(tryPeek(TokenType::boolType) && tryPeek(TokenType::openParenthesis, 1)) {
                consume(); consume();
                dataType = DataType::Boolean;
            }
            else if(tryPeek(TokenType::charType) && tryPeek(TokenType::openParenthesis, 1)) {
                consume(); consume();
                dataType = DataType::Character;
            }

            std::optional<TermExpressionNode*> term = parseTerm();

            if(!term.has_value()) return {};
            term.value()->isNegative = isNegative;

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

                //Don't increment if it is a unary operator
                if(operatorToken.type == TokenType::notOperator) right = parseExpression(precedenceLevel.value());
                else right = parseExpression(precedenceLevel.value() + 1);

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
                    case TokenType::notOperator:
                        operationExpression->_operator = TokenType::notOperator;
                        left->type = right.value()->type;
                        break;
                    case TokenType::andOperator:
                        operationExpression->_operator = TokenType::andOperator;
                        break;
                    case TokenType::orOperator:
                        operationExpression->_operator = TokenType::orOperator;
                        break;
                    default:
                        if(operatorToken.value.has_value())
                            std::cerr << "Line " << operatorToken.lineNumber << ": Error: Unexpected operator '" << operatorToken.value.value() << "'" << std::endl;
                        else if(tokenToString(operatorToken.type) != "")
                            std::cerr << "Line " << operatorToken.lineNumber << ": Error: Unexpected operator '" << tokenToString(operatorToken.type) << "'" << std::endl;
                        else std::cerr << "Line " << operatorToken.lineNumber << ": Error: Expected an operator" << std::endl;


                        exit(EXIT_FAILURE);
                }

                if(getGroupType(left->type) == GroupType::Primitive &&
                        getGroupType(right.value()->type) == GroupType::Primitive) {
                    auto temp = m_ArenaAllocator.allocate<ExpressionNode>(); //Prevents pointer loop

                    temp->variant = left->variant;

                    operationExpression->left = temp;
                    operationExpression->right = right.value();

                    left->variant = operationExpression;

                    auto _operator = operationExpression->_operator;
                    if (_operator == TokenType::addition || _operator == TokenType::subtraction ||
                        _operator == TokenType::multiplication
                        || _operator == TokenType::division || _operator == TokenType::modulo) {
                        left->type = DataType::Integer;
                    } else if (_operator == TokenType::equalTo || _operator == TokenType::notEqualTo ||
                               _operator == TokenType::lessThan
                               || _operator == TokenType::lessThanOrEqual || _operator == TokenType::greaterThan ||
                               _operator == TokenType::greaterThanOrEqual
                               || _operator == TokenType::notOperator || _operator == TokenType::andOperator ||
                               _operator == TokenType::orOperator) {
                        left->type = DataType::Boolean;
                    }
                }
                else {
                    std::cerr << "Line " << peek(-1).value().lineNumber << ": Error: Operator '" << tokenToString(operationExpression->_operator) <<
                                "' cannot be applied to types '" << dataTypeToString(left->type) << "' and '"
                                << dataTypeToString(right.value()->type) << "'" << std::endl;
                    exit(EXIT_FAILURE);
                }
            }

            // Check if type-casted
            if(dataType != DataType::Empty)  {
                left->type = dataType;

                if(tryPeek(TokenType::closeParenthesis)) consume();
                else expectedCharacterError(peek(-1).value().lineNumber, ')');
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

                auto statementNode = m_ArenaAllocator.allocate<StatementNode>();
                statementNode->variant = printNode;
                statementNode->lineNumber = peek(-1).value().lineNumber;

                return statementNode;
            }
            else if(tryPeek(TokenType::set)) {
                consume();

                if(!peek().has_value()) {
                    std::cerr << "Line " << peek(-1).value().lineNumber << ": Error: Expected a data type after 'set'" << std::endl;
                    exit(EXIT_FAILURE);
                }

                auto dataType = peek().value().type; consume();

                if(tryPeek(TokenType::openBracket)) {
                    auto declarationNode = m_ArenaAllocator.allocate<ArrayDeclarationNode>();
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
                            std::cerr << "Line " << peek(-1).value().lineNumber << ": Error: Expected a data type after 'set'" << std::endl;
                            exit(EXIT_FAILURE);
                    }

                    if(!tryPeek(TokenType::identifier)) {
                        std::cerr << "Line " << peek(-1).value().lineNumber << ": Error: Expected variable identifier" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                    declarationNode->identifier = consume();

                    if(!tryPeek(TokenType::assignment)) expectedCharacterError(peek(-1).value().lineNumber, '=');
                    consume();

                    if(!tryPeek(TokenType::newKeyWord)) {
                        std::cerr << "Line " << peek(-1).value().lineNumber << ": Error: expected 'new' before array allocation" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                    consume();

                    if(!tryPeek(TokenType::arrayType)) {
                        std::cerr << "Line " << peek(-1).value().lineNumber << ": Error: expected 'Array' after 'new'" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                    consume();

                    if(!tryPeek(TokenType::openBracket)) expectedCharacterError(peek(-1).value().lineNumber, '[');
                    consume();

                    if(auto expressionNode = parseExpression()) {
                        declarationNode->size = expressionNode.value();
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
                else {
                    auto declarationNode = m_ArenaAllocator.allocate<PrimitiveDeclarationNode>();

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
                            std::cerr << "Line " << peek(-1).value().lineNumber << ": Error: Expected a data type after 'set'" << std::endl;
                            exit(EXIT_FAILURE);
                    }

                    if(!tryPeek(TokenType::identifier)) {
                        std::cerr << "Line " << peek(-1).value().lineNumber << ": Error: Expected variable identifier" << std::endl;
                        exit(EXIT_FAILURE);
                    }
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

                            if((getGroupType(m_variables[reAssignmentNode->identifier.value.value()]) != GroupType::Primitive && !isPrimitive) ||
                               getGroupType(reAssignmentNode->expression->type) != GroupType::Primitive) {
                                std::cerr << "Line " << statementNode->lineNumber << ": Error: Cannot assign type '" << dataTypeToString(reAssignmentNode->expression->type)
                                          << "' to type '" << dataTypeToString(m_variables[reAssignmentNode->identifier.value.value()]) << "'" << std::endl;
                                exit(EXIT_FAILURE);
                            }

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

                if(auto condition = parseExpression()) {
                    conditionalNode->condition.push_back(condition.value());

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
                if(auto statement = parseStatement()) {
                   program.statements.push_back(statement.value());
                }
                else {
                    std::cerr << "Line " << peek().value().lineNumber << ": Error: Couldn't parse statement" << std::endl;
                    exit(EXIT_FAILURE);
                }
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
                default:
                    return GroupType::None;
            }
        }

        void expectedCharacterError(int lineNumber, char expectedCharacter) {
            if(peek().has_value()) {
                std::cerr << "Line " << lineNumber << ": Error: Expected '" << expectedCharacter << "' but found " << tokenToString(
                        peek().value().type) << std::endl;
            }
            else {
                std::cerr << "Line " << lineNumber << ": Error: Expected '" << expectedCharacter << "' but found none"<< std::endl;
            }

            exit(EXIT_FAILURE);
        }

        void expectedExpressionError(int lineNumber) {
            std::cerr << "Line " << lineNumber << ": Error: Expected expression after '" << tokenToString(peek(-1).value().type) << "'" << std::endl;
            exit(EXIT_FAILURE);
        }

        std::string dataTypeToString(DataType dataType) {
            switch(dataType) {
                case DataType::Integer:
                    return "int";
                case DataType::Boolean:
                    return "bool";
                case DataType::Character:
                    return "char";
                case DataType::IntArray:
                    return "int[]";
                case DataType::BoolArray:
                    return "bool[]";
                case DataType::CharArray:
                    return "char[]";
                case DataType::Empty:
                case DataType::None:
                    return "";
            }
        }

        const std::vector<Token> m_Tokens;
        size_t m_Index = 0;
        ArenaAllocator m_ArenaAllocator;
        std::unordered_map<std::string, DataType> m_variables;
};