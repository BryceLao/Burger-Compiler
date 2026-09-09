#pragma once

#include <variant>

#include "../tokenizer/tokenizer.hpp"

namespace Parser {
    using Token = Tokenizer::Token;
    using TokenType = Tokenizer::TokenType;

    struct ExpressionNode;

    struct StatementNode;

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

    struct Argument {
        Token identifier;
        ExpressionNode* expression;
    };

    struct FunctionCall {
        Token identifier;
        std::vector<Argument> arguments;
    };

    struct UnaryExpressionTerm {
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
        Util::DataType type;
        int lineNumber;
        std::variant<LiteralTerm*, IdentifierTerm*, IndexedTerm*, PropertyTerm*, InputTerm*, TypeCastTerm*, UnaryExpressionTerm*, ParenthesisTerm*, FunctionCall*> variant;
    };

    struct ExpressionNode {
        Util::DataType type;
        int lineNumber;
        std::variant<TermExpressionNode*, OperationExpressionNode*> variant;
    };

    struct ExitNode {
        ExpressionNode* expression;
    };

    struct PrintNode {
        ExpressionNode* expression;
    };

    struct DeclarationNode {
        Token identifier;
        Util::DataType type;
        ExpressionNode* expression;
    };

    struct ReAssignmentNode {
        Token identifier;
        ExpressionNode* expression;
        std::optional<ExpressionNode*> index;
    };

    struct ScopeNode {
        std::vector<StatementNode*> statements = {};
    };

    struct ConditionalNode {
        std::vector<ExpressionNode*> conditions = {};
        std::vector<std::vector<StatementNode*>> statements = {};
    };

    struct LoopNode {
        ExpressionNode* condition;
        std::vector<StatementNode*> statements = {};
    };

    struct Variable {
        Token identifier;
        Util::DataType dataType;
    };

    struct FunctionNode {
        Util::DataType returnType;
        Token identifier;
        std::vector<Variable> arguments = {};
        std::vector<StatementNode*> statements = {};
    };

    struct ReturnNode {
        std::string functionName;
        std::optional<ExpressionNode*> expression;
    };

    struct StatementNode {
        int lineNumber;
        std::variant<ExitNode*, PrintNode*, DeclarationNode*, ReAssignmentNode*, ScopeNode*, ConditionalNode*, LoopNode*, FunctionNode*, ReturnNode*, FunctionCall*> variant;
    };

    struct ProgramNode {
        std::vector<StatementNode*> statements = {};
    };
}
