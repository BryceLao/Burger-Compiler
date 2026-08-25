#pragma once

#include <variant>
#include <unordered_map>

#include "tokenizer.hpp"
#include "arenaAllocator.hpp"

enum class DataType {
    Integer,
    Boolean
};

struct LiteralTerm {
    Token literal;
};

struct IdentifierTerm {
    Token identifier;
};

struct ExpressionNode;

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
    std::variant<LiteralTerm*,IdentifierTerm*, ParenthesisTerm*, DummyTerm*> variant;
};

struct ExpressionNode {
    DataType type;
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

        std::optional<TermExpressionNode*> parseTerm() {
            if(tryPeek(TokenType::intLiteral)) {
                auto literalExpression = m_ArenaAllocator.allocate<LiteralTerm>();
                literalExpression->literal = consume();

                auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
                termExpression->variant = literalExpression;
                termExpression->type = DataType::Integer;

                return termExpression;
            }
            else if(tryPeek(TokenType::boolLiteral)) {
                auto literalExpression = m_ArenaAllocator.allocate<LiteralTerm>();
                literalExpression->literal = consume();

                auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
                termExpression->variant = literalExpression;
                termExpression->type = DataType::Boolean;

                return termExpression;
            }
            else if(tryPeek(TokenType::identifier)) {
                auto identifierTerm = m_ArenaAllocator.allocate<IdentifierTerm>();
                identifierTerm->identifier = consume();

                auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
                termExpression->variant = identifierTerm;
                termExpression->type = m_variables[identifierTerm->identifier.value.value()];

                return termExpression;
            }
            else if(tryPeek(TokenType::openParenthesis)) {
                consume();

                auto expression = parseExpression();

                if(!expression.has_value()) {
                    std::cerr << "Invalid Expression" << std::endl;
                    exit(EXIT_FAILURE);
                }

                if(tryPeek(TokenType::closeParenthesis)) consume();
                else {
                    std::cerr << "Expected ')'" << std::endl;
                    exit(EXIT_FAILURE);
                }

                auto parenthesisTerm = m_ArenaAllocator.allocate<ParenthesisTerm>();
                parenthesisTerm->expression = expression.value();

                auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
                termExpression->variant = parenthesisTerm;
                termExpression->type = expression.value()->type;

                return termExpression;
            }
            else if(tryPeek(TokenType::notOperator)) {
                auto dummyTerm = m_ArenaAllocator.allocate<DummyTerm>();

                auto termExpression = m_ArenaAllocator.allocate<TermExpressionNode>();
                termExpression->variant = dummyTerm;

                return termExpression;
            }
            else {
                std::cerr << "Invalid Expression" << std::endl;
                exit(EXIT_FAILURE);
            }
        }

        std::optional<ExpressionNode*> parseExpression(int minimumPrecedence = 0) {
            std::optional<TermExpressionNode*> term = parseTerm();

            if(!term.has_value()) return {};

            ExpressionNode* left = m_ArenaAllocator.allocate<ExpressionNode>();
            left->variant = term.value();
            left->type = term.value()->type;

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

                if(!right.has_value()) {
                    std::cerr << "Invalid Expression" << std::endl;
                    exit(EXIT_FAILURE);
                }

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
                        std::cerr << "Invalid Operator" << std::endl;
                        exit(EXIT_FAILURE);
                }

                auto temp = m_ArenaAllocator.allocate<ExpressionNode>(); //Prevents pointer loop

                temp->variant = left->variant;
                temp->type = left->type;

                operationExpression->left = temp;
                operationExpression->right = right.value();

                left->variant = operationExpression;
                left->type = temp->type;

                if(left->type != right.value()->type) {
                    std::cerr << "DataType Mismatch" << std::endl;
                    exit(EXIT_FAILURE);
                }
            }

            return left;
        }

        std::optional<std::vector<StatementNode*>> parseScope() {
            std::vector<StatementNode*> statements;

            while(peek().has_value() && peek().value().type != TokenType::closeCurlyBrace) {
                statements.push_back(parseStatement().value());
            }

            if(!peek().has_value() || peek().value().type != TokenType::closeCurlyBrace) {
                std::cerr << "Expected '}'" << std::endl;
                exit(EXIT_FAILURE);
            }

            consume();

            return statements;
        }

        std::optional<StatementNode*> parseStatement() {
            if(tryPeek(TokenType::exit) && tryPeek(TokenType::openParenthesis, 1)) {
                consume(); consume();

                auto exitNode = m_ArenaAllocator.allocate<ExitNode>();

                if(auto expressionNode = parseExpression()) {
                    exitNode->expression = expressionNode.value();
                }
                else {
                    std::cerr << "Expected Expression" << std::endl;
                    exit(EXIT_FAILURE);
                }

                if(tryPeek(TokenType::closeParenthesis)) {
                    consume();
                }
                else {
                    std::cerr << "Expected ')'" << std::endl;
                    exit(EXIT_FAILURE);
                }

                if(tryPeek(TokenType::semiCol)) {
                    consume();
                }
                else {
                    std::cerr << "Expected ';'" << std::endl;
                    exit(EXIT_FAILURE);
                }

                auto statementNode = m_ArenaAllocator.allocate<StatementNode>();
                statementNode->variant = exitNode;

                return statementNode;
            }
            else if(tryPeek(TokenType::print) && tryPeek(TokenType::openParenthesis, 1)) {
                consume(); consume();

                auto printNode = m_ArenaAllocator.allocate<PrintNode>();

                if(auto expressionNode = parseExpression()) {
                    printNode->expression = expressionNode.value();
                }
                else {
                    std::cerr << "Expected Expression" << std::endl;
                    exit(EXIT_FAILURE);
                }

                if(tryPeek(TokenType::closeParenthesis)) {
                    consume();
                }
                else {
                    std::cerr << "Expected ')'" << std::endl;
                    exit(EXIT_FAILURE);
                }

                if(tryPeek(TokenType::semiCol)) {
                    consume();
                }
                else {
                    std::cerr << "Expected ';'" << std::endl;
                    exit(EXIT_FAILURE);
                }

                auto statementNode = m_ArenaAllocator.allocate<StatementNode>();
                statementNode->variant = printNode;

                return statementNode;
            }
            else if(tryPeek(TokenType::set) && tryPeek(TokenType::identifier, 2) && tryPeek(TokenType::assignment, 3)) {
                consume();

                auto declarationNode = m_ArenaAllocator.allocate<DeclarationNode>();

                switch(peek().value().type) {
                    case TokenType::intType:
                        declarationNode->type = DataType::Integer;
                        consume();
                        break;
                    case TokenType::boolType:
                        declarationNode->type = DataType::Boolean;
                        consume();
                        break;
                    default:
                        std::cerr << "Expected DataType Identifier" << std::endl;
                        exit(EXIT_FAILURE);
                }

                declarationNode->identifier = consume();

                consume();

                if(auto expressionNode = parseExpression()) {
                    if(declarationNode->type != expressionNode.value()->type) {
                        std::cerr << "Variable DataType and Expression DataType Do Not Match" << std::endl;
                        exit(EXIT_FAILURE);
                    }

                    declarationNode->expression = expressionNode.value();
                }
                else {
                    std::cerr << "Variable Not Set To A Value" << std::endl;
                    exit(EXIT_FAILURE);
                }

                if(tryPeek(TokenType::semiCol)) {
                    consume();

                    auto statementNode = m_ArenaAllocator.allocate<StatementNode>();
                    statementNode->variant = declarationNode;

                    m_variables[declarationNode->identifier.value.value()] = declarationNode->type;

                    return statementNode;
                }
                else {
                    std::cerr << "Expected ';'" << std::endl;
                    exit(EXIT_FAILURE);
                }
            }
            else if(tryPeek(TokenType::identifier)) {
                auto reAssignmentNode = m_ArenaAllocator.allocate<ReAssignmentNode>();
                reAssignmentNode->identifier = consume();

                if(tryPeek(TokenType::assignment)) {
                    consume();

                    if(auto expression = parseExpression()) {
                        if(tryPeek(TokenType::semiCol)) {
                            consume();

                            reAssignmentNode->expression = expression.value();

                            auto statementNode = m_ArenaAllocator.allocate<StatementNode>();
                            statementNode->variant = reAssignmentNode;

                            return statementNode;
                        }
                        else {
                            std::cerr << "Expected ';'" << std::endl;
                            exit(EXIT_FAILURE);
                        }
                    }
                    else {
                        std::cerr << "Expected Expression" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                }
                else {
                    std::cerr << "Expected Assignment Operator" << std::endl;
                    exit(EXIT_FAILURE);
                }
            }
            else if(tryPeek(TokenType::openCurlyBrace)) {
                consume();

                auto scopeNode = m_ArenaAllocator.allocate<ScopeNode>();
                scopeNode->statements = parseScope().value();

                auto statementNode = m_ArenaAllocator.allocate<StatementNode>();
                statementNode->variant = scopeNode;

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
                                else {
                                    std::cerr << "Expected '{'" << std::endl;
                                    exit(EXIT_FAILURE);
                                }
                            }
                            else {
                                std::cerr << "Expected Expression" << std::endl;
                                exit(EXIT_FAILURE);
                            }
                        }

                        if(tryPeek(TokenType::elseStatement)) {
                            consume();

                            if(tryPeek(TokenType::openCurlyBrace)) {
                                consume();

                                conditionalNode->statements.push_back(parseScope().value());
                            }
                            else {
                                std::cerr << "Expected '{'" << std::endl;
                                exit(EXIT_FAILURE);
                            }
                        }

                        auto statementNode = m_ArenaAllocator.allocate<StatementNode>();
                        statementNode->variant = conditionalNode;

                        return statementNode;
                    }
                    else {
                        std::cerr << "Expected '{'" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                }
                else {
                    std::cerr << "Expected Expression" << std::endl;
                    exit(EXIT_FAILURE);
                }
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

                        return statementNode;
                    }
                    else {
                        std::cerr << "Expected '{'" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                }
                else {
                    std::cerr << "Expected Expression" << std::endl;
                    exit(EXIT_FAILURE);

                }
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
                    std::cerr << "Couldn't Parse Statement" << std::endl;
                    exit(EXIT_FAILURE);
                }
            }

            return program;
        }

    private:
        const std::vector<Token> m_Tokens;
        size_t m_Index = 0;
        ArenaAllocator m_ArenaAllocator;
        std::unordered_map<std::string, DataType> m_variables;

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
};