#pragma once

#include <iostream>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "ast.hpp"

#include "../tokenizer/tokenizer.hpp"

#include "../util/arenaAllocator.hpp"
#include "../util/dataType.hpp"

namespace Parser {
    using Token = Tokenizer::Token;
    using TokenType = Tokenizer::TokenType;

    using DataType = Util::DataType;
    using GroupType = Util::GroupType;

    using VarMap = std::unordered_map<std::string, DataType>;

    class Parser {
        public:
            explicit Parser(std::vector<Tokenizer::Token> tokens);
            ProgramNode parseProgram();

        private:
            struct Function {
                DataType returnType;
                std::vector<Variable> arguments;
            };

            // Token Access/Manipulation
            [[nodiscard]] std::optional<Tokenizer::Token> peek(int ahead = 0) const;
            Tokenizer::Token consume();

            Tokenizer::Token expectCharacter(Tokenizer::TokenType type);
            ExpressionNode* expectExpression(VarMap& variables, int minimumPrecedence = 0);
            FunctionCall* parseFunctionCall(VarMap& variables);

            void convertToString(ExpressionNode* expression);

            [[nodiscard]] int lastLine() const;
            [[nodiscard]] bool isTextType(DataType dataType) const;

            DataType tokenToDataType(Tokenizer::TokenType type);
            std::optional<int> getPrecedenceLevel(TokenType type) const;

            [[noreturn]] void throwError(int lineNumber, std::string message);

            template <typename... Args>
            [[nodiscard]] bool tryPeek(int ahead = 0, Args... types) const {
                if(m_Index + ahead >= m_Tokens.size() || m_Index + ahead < 0) return false;

                return ((peek(ahead).value().type == types) || ...);
            }

            template <typename... Args>
            [[nodiscard]] bool tryPeek(Args... types) const {
                if(m_Index >= m_Tokens.size()) return false;

                return tryPeek(0, types...);
            }

            template <typename... Args>
            Tokenizer::Token expectToken(std::string message, Args... args) {
                if(!tryPeek(args...)) throwError(lastLine(), message);
                return consume();
            }

            // Parse Statements
            std::optional<StatementNode*> parseStatement(VarMap& variables, std::string_view curFunction = "");
            std::vector<StatementNode*> parseScope(VarMap & variables, std::string_view curFunction = "");

            StatementNode* parseExit(VarMap& variables);
            StatementNode* parsePrint(VarMap& variables);
            StatementNode* parseVariableDeclaration(VarMap& variables);
            StatementNode* parseReAssignment(VarMap& variables);
            StatementNode* parseScopeStatement(VarMap& variables, std::string_view curFunction);
            StatementNode* parseConditional(VarMap& variables, std::string_view curFunction);
            StatementNode* parseLoop(VarMap& variables, std::string_view curFunction);
            StatementNode* parseFunctionDeclaration(std::string_view curFunction);
            StatementNode* parseReturn(VarMap& variables, std::string_view curFunction);
            StatementNode* parseFunctionCallStatement(VarMap& variables);

            template <typename T>
            StatementNode* makeStatementNode(T* variant, int lineNumber) {
                auto statement = m_ArenaAllocator.allocate<StatementNode>();

                statement->variant = variant;
                statement->lineNumber = lineNumber;

                return statement;
            }

            // Parse Expressions
            std::optional<ExpressionNode*> parseExpression(VarMap& variables, int minimumPrecedence = 0);
            DataType getResultantType(ExpressionNode* left, ExpressionNode* right, Tokenizer::TokenType _operator);

            template <typename T>
            ExpressionNode* makeExpressionNode(T* variant, DataType dataType, int lineNumber) {
                auto expression = m_ArenaAllocator.allocate<ExpressionNode>();

                expression->variant = variant;
                expression->type = dataType;
                expression->lineNumber = lineNumber;

                return expression;
            }

            // Parse Terms
            std::optional<TermExpressionNode*> parseTerm(VarMap& variables, int minimumPrecedence = 0);

            TermExpressionNode* parseLiteral();
            TermExpressionNode* parseIdentifier(VarMap& variables);
            TermExpressionNode* parseTypeCast(VarMap& variables);
            TermExpressionNode* parseReadInput(VarMap& variables);
            TermExpressionNode* parseParenthesisTerm(VarMap& variables);
            TermExpressionNode* parseFunctionCallTerm(VarMap& variables);
            TermExpressionNode* parseUnaryExpression(VarMap& variables, int minimumPrecedence = 0);

            template <typename T>
            TermExpressionNode* makeTermNode(T* variant, DataType dataType, int lineNumber) {
                auto term = m_ArenaAllocator.allocate<TermExpressionNode>();

                term->variant = variant;
                term->type = dataType;
                term->lineNumber = lineNumber;

                return term;
            }

            const std::vector<Tokenizer::Token> m_Tokens;
            int m_Index = 0;

            Util::ArenaAllocator m_ArenaAllocator;

            VarMap m_variables;
            std::unordered_map<std::string , Function> m_functions;

            const int NEGATION_PRECEDENCE_LEVEL = 6;
            const int NOT_OPERATOR_PRECEDENCE_LEVEL = 2;
    };
}
