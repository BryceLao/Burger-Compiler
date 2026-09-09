#pragma once

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <variant>

#include "../parser/ast.hpp"
#include "../tokenizer/tokenizer.hpp"

namespace Generator {
    using TokenType = Tokenizer::TokenType;

    using DataType = Util::DataType;
    using GroupType = Util::GroupType;

    class Generator {
        public:
            explicit Generator(Parser::ProgramNode program);
            std::string generateProgram();

        private:
            struct Variable;
            using VarMap = std::unordered_map<std::string, Variable>;

            struct Function {
                int argumentCount;
                VarMap variables;
            };

            struct Variable {
                int scopeDepth;
                int stackLocation;
                DataType type;
            };

            // Assembly Helpers
            void push(const std::string& reg);
            void pop(const std::string& reg);

            // Generate Statements
            void generateProgramStatements();
            void generateStatement(Parser::StatementNode* statement, VarMap& variables);
            void generateScope(std::vector<Parser::StatementNode*> statements, VarMap& variables);

            void generateExit(const Parser::ExitNode* exitNode, VarMap& variables);
            void generatePrint(const Parser::PrintNode* printNode, VarMap& variables);
            void generateVariableDeclaration(const Parser::DeclarationNode* declarationNode, VarMap& variables, int lineNumber);
            void generateReAssignment(const Parser::ReAssignmentNode* reAssignmentNode, VarMap& variables, int lineNumber);
            void generateScopeStatement(const Parser::ScopeNode* scopeNode, VarMap& variables);
            void generateConditional(const Parser::ConditionalNode* conditionalNode, VarMap& variables);
            void generateLoop(const Parser::LoopNode* loopNode, VarMap& variables);
            void generateFunctionDeclaration(Parser::FunctionNode* functionNode);
            void generateReturn(const Parser::ReturnNode* returnNode, VarMap& variables);

            // Generate Expressions
            void generateExpression(const Parser::ExpressionNode* expression, VarMap& variables);

            void generateTermExpression(Parser::TermExpressionNode* termExpression, VarMap& variables);
            void generateOperationExpression(const Parser::OperationExpressionNode* operationExpression, VarMap& variables, DataType resultantType);

            void generateOperation(GroupType groupType, TokenType _operator, DataType resultantType);

            // Generate Terms
            void generateTerm(Parser::TermExpressionNode* term, VarMap& variables);

            void generateLiteral(const Parser::LiteralTerm* literalTerm);
            void generateIdentifier(const Parser::IdentifierTerm* identifierTerm, VarMap& variables, int lineNumber);
            void generateIndexedTerm(const Parser::IndexedTerm* indexedTerm, VarMap& variables, int lineNumber);
            void generateProperty(const Parser::PropertyTerm* propertyTerm, VarMap& variables, int lineNumber);
            void generateTypeCast(const Parser::TypeCastTerm* typeCastTerm, VarMap& variables);
            void generateReadInput(const Parser::InputTerm* inputTerm, VarMap& variables);
            void generateParenthesisTerm(const Parser::ParenthesisTerm* parenthesisTerm, VarMap& variables);
            void generateFunctionCall(const Parser::FunctionCall* functionCallTerm, VarMap& variables);
            void generateUnaryExpression(const Parser::UnaryExpressionTerm* unaryTerm, VarMap& variables);

            // Generation Functions
            void generateData();
            void generateEntryPoint();

            void generateProgramExit();
            void generateErrorHandlers();

            void generateUserFunctions();

            void generateUtil();
            void generateInputFunctions();
            void generateStringFunctions();
            void generateTypeConversionFunctions();

            [[noreturn]] void throwError(int lineNumber, std::string message);

            const Parser::ProgramNode m_program;
            std::stringstream m_output;

            int m_stackSize = 0;
            int m_scopeDepth = 0;
            int m_curLabelCount = 0;

            VarMap m_variables;
            std::unordered_map<std::string, Function> m_functions;
            std::vector<Parser::FunctionNode*> m_functionNodes;

            const int VAR_SIZE = 8;
            const int ASCII_ZERO = 48;
            const int ASCII_ONE = 49;
            const int ASCII_NINE = 57;
            const int ASCII_MINUS = 45;
            const int ASCII_TAB = 9;
            const int ASCII_NEWLINE = 10;
            const int ASCII_VERTICAL_TAB = 11;
            const int ASCII_FORM_FEED = 12;
            const int ASCII_CARRIAGE_RETURN = 13;
            const int ASCII_SPACE = 32;
    };
}
