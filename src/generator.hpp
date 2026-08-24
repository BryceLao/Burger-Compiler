#pragma once

#include <unordered_map>
#include <cassert>
#include "parser.hpp"

class Generator{
    public:
        inline explicit Generator(ProgramNode program) :
            m_program(std::move(program)) {}

        void generateTerm(const TermExpressionNode* termExpression) {
            struct TermVisitor {
                Generator* generator;

                void operator()(const IntLiteralTerm* expressionNodeIntLiteral) const {
                    generator->m_output << "    mov rax, " << expressionNodeIntLiteral->intLiteral.value.value() << "\n";
                    generator->push("rax");
                }
                void operator()(const IdentifierTerm* expressionNodeIdentifier) const {
                    if(generator->m_variables.find(expressionNodeIdentifier->identifier.value.value()) == generator->m_variables.end()) {
                        std::cerr << "Variable Does Not Exist" << std::endl;
                        exit(EXIT_FAILURE);
                    }

                    size_t stackLocation = generator->m_variables[expressionNodeIdentifier->identifier.value.value()].stackLocation;
                    generator->push("[rsp + " + std::to_string((generator->m_stackSize - stackLocation - 1) * 8) + "]");
                }
                void operator()(const ParenthesisTerm* parenthesisTerm) const {
                    generator->generateExpression(parenthesisTerm->expression);
                }
                void operator()(const DummyTerm* dummyTerm) const {

                }
            };

            TermVisitor visitor {.generator = this};
            std::visit(visitor, termExpression->variant);
        }

        void generateExpression(const ExpressionNode* expression) {
            struct ExpressionVisitor {
                Generator* generator;

                void operator()(const TermExpressionNode* termExpressionNode) const {
                    generator->generateTerm(termExpressionNode);
                }
                void operator()(const OperationExpressionNode* operatorExpressionNode) const {
                    //Edge-Case: Not operator only has 1 side
                    if(operatorExpressionNode->_operator == TokenType::notOperator) {
                        generator->generateExpression(operatorExpressionNode->right);

                        generator->pop("rax");

                        generator->m_output << "    cmp rax,0\n";
                        generator->m_output << "    sete al\n";
                        generator->m_output << "    movzx rax, al\n";
                    }
                    else {
                        generator->generateExpression(operatorExpressionNode->left);
                        generator->generateExpression(operatorExpressionNode->right);

                        generator->pop("rbx");
                        generator->pop("rax");

                        switch (operatorExpressionNode->_operator) {
                            case (TokenType::addition):
                                generator->m_output << "    add rax,rbx\n";
                                break;
                            case (TokenType::subtraction):
                                generator->m_output << "    sub rax,rbx\n";
                                break;
                            case (TokenType::multiplication):
                                generator->m_output << "    mul rax,rbx\n";
                                break;
                            case (TokenType::division):
                                generator->m_output << "    div rbx\n";
                                break;
                            case (TokenType::modulo):
                                generator->m_output << "    div rbx\n";
                                generator->m_output << "    mov rax, rdx\n";
                                break;
                            case (TokenType::equalTo):
                                generator->m_output << "    cmp rax, rbx\n";
                                generator->m_output << "    sete al\n";
                                generator->m_output << "    movzx rax, al\n";
                                break;
                            case (TokenType::notEqualTo):
                                generator->m_output << "    cmp rax, rbx\n";
                                generator->m_output << "    setne al\n";
                                generator->m_output << "    movzx rax, al\n";
                                break;
                            case (TokenType::lessThan):
                                generator->m_output << "    cmp rax, rbx\n";

                                //Converts into 1 or 0
                                generator->m_output << "    setl al\n";
                                generator->m_output << "    movzx rax, al\n";
                                break;
                            case (TokenType::lessThanOrEqual):
                                generator->m_output << "    cmp rax, rbx\n";

                                //Converts into 1 or 0
                                generator->m_output << "    setle al\n";
                                generator->m_output << "    movzx rax, al\n";
                                break;
                            case (TokenType::greaterThan):
                                generator->m_output << "    cmp rax, rbx\n";

                                //Converts into 1 or 0
                                generator->m_output << "    setg al\n";
                                generator->m_output << "    movzx rax, al\n";
                                break;
                            case (TokenType::greaterThanOrEqual):
                                generator->m_output << "    cmp rax, rbx\n";

                                //Converts into 1 or 0
                                generator->m_output << "    setge al\n";
                                generator->m_output << "    movzx rax, al\n";
                                break;
                            case (TokenType::andOperator):
                                //Converts Non-Zero Integers as True booleans
                                generator->m_output << "    cmp rax, 1\n";
                                generator->m_output << "    setge al\n";
                                generator->m_output << "    movzx rax, al\n";

                                generator->m_output << "    cmp rbx, 1\n";
                                generator->m_output << "    setge bl\n";
                                generator->m_output << "    movzx rbx, bl\n";

                                generator->m_output << "    add rax, rbx\n";

                                generator->m_output << "    cmp rax, 2\n";
                                generator->m_output << "    setge al\n";
                                generator->m_output << "    movzx rax, al\n";

                                break;
                            case (TokenType::orOperator):
                                //Converts Non-Zero Integers as True booleans
                                generator->m_output << "    cmp rax, 1\n";
                                generator->m_output << "    setge al\n";
                                generator->m_output << "    movzx rax, al\n";

                                generator->m_output << "    cmp rbx, 1\n";
                                generator->m_output << "    setge bl\n";
                                generator->m_output << "    movzx rbx, bl\n";

                                generator->m_output << "    add rax, rbx\n";

                                generator->m_output << "    cmp rax, 1\n";
                                generator->m_output << "    setge al\n";
                                generator->m_output << "    movzx rax, al\n";
                                break;
                            default:
                                std::cerr << "Operator Not Supported" << std::endl;
                                exit(EXIT_FAILURE);
                        }
                    }

                    generator->push("rax");

                }
            };

            ExpressionVisitor visitor {.generator = this};
            std::visit(visitor, expression->variant);
        }

        void generateStatement(const StatementNode* statement) {
            struct StatementVisitor {
                Generator* generator;

                void operator()(const ExitNode* exitNode) const {
                    generator->generateExpression(exitNode->expression);

                    generator->m_output << "    mov rax, 60\n";
                    generator->pop("rdi");
                    generator->m_output << "    syscall\n";
                }
                void operator()(const PrintNode* printNode) const {
                    generator->generateExpression(printNode->expression);

                    generator->m_output << "    call printInt\n";
                }
                void operator()(const DeclarationNode* declarationNode) const {
                    if(generator->m_variables.contains(declarationNode->identifier.value.value())) {
                        std::cerr << "Redefinition of Identifier" << std::endl;
                        exit(EXIT_FAILURE);
                    }

                    generator->m_variables.insert({declarationNode->identifier.value.value(), Variable {.scopeDepth = generator->m_scopeDepth,
                                                                                                        .stackLocation = generator->m_stackSize}});
                    generator->generateExpression(declarationNode->expression);
                }
                void operator()(const ReAssignmentNode* reAssignmentNode) const {
                    if(!generator->m_variables.contains(reAssignmentNode->identifier.value.value())) {
                        std::cerr << "Undeclared Identifier" << std::endl;
                        exit(EXIT_FAILURE);
                    }

                    generator->generateExpression(reAssignmentNode->expression);
                    generator->pop("rax");

                    size_t stackLocation = generator->m_variables[reAssignmentNode->identifier.value.value()].stackLocation;
                    generator->m_output << "    mov [rsp + " << std::to_string((generator->m_stackSize - stackLocation - 1) * 8) << "], rax\n";
                }
                void operator()(const ScopeNode* scopeNode) const {
                    generator->generateScope(scopeNode->statements);
                }
                void operator()(const ConditionalNode* conditionalNode) {
                    for (int i = 0; i < conditionalNode->statements.size(); ++i) {
                        // If an Else Statement
                        if(i == conditionalNode->condition.size()) {
                            generator->generateScope(conditionalNode->statements[i]);
                        }
                        else {
                            generator->generateExpression(conditionalNode->condition[i]);
                            generator->pop("rax");
                            generator->m_output << "    cmp rax, 1\n";
                            generator->m_output << "    jl a" << generator->m_curLabelCount + i << "\n";

                            generator->generateScope(conditionalNode->statements[i]);

                            generator->m_output << "    jmp a" << generator->m_curLabelCount + conditionalNode->statements.size() - 1 << "\n";

                            generator->m_output << "a" << generator->m_curLabelCount + i << ":\n";
                        }
                    }
                    generator->m_output << "a" << generator->m_curLabelCount + conditionalNode->statements.size() - 1 << ":\n";
                    generator->m_curLabelCount += conditionalNode->statements.size();
                }
                void operator()(const LoopNode* loopNode) {
                    int statementLabel = generator->m_curLabelCount++;
                    int conditionLabel =generator->m_curLabelCount++;

                    //Initial jump to condition label
                    generator->m_output << "    jmp a" << conditionLabel << "\n";

                    //Statements & statement label
                    generator->m_output << "a" << statementLabel << ":\n";
                    generator->generateScope(loopNode->statements);

                    //Condition & condition label
                    generator->m_output << "a" << conditionLabel << ":\n";
                    generator->generateExpression(loopNode->condition);
                    generator->pop("rax");

                    //If condition is true jump back to statement label
                    generator->m_output << "    cmp rax, 1\n";
                    generator->m_output << "    jge a" << statementLabel << "\n";
                }
            };

            StatementVisitor visitor {.generator = this};
            std::visit(visitor, statement->variant);
        }

        [[nodiscard]] std::string generateProgram() {
            m_output << "section .text\nglobal _start\n_start:\n";

            for(const StatementNode* statement : m_program.statements) {
                generateStatement(statement);
            }

            m_output << "    mov rax, 60\n";
            m_output << "    mov rdi, 0\n";
            m_output << "    syscall\n";

            generateUtil();

            return m_output.str();
        }

        void generateUtil() {
            m_output << "\n\n\n ; Util Functions \n\n\n";

            m_output << "printInt:\n"
                        "   mov rbx, 1\n"
                        "   mov rax, 10\n"   // New Line Character
                        "   push rax\n"
                        "   mov rax, [rsp + 16]\n"
                        "   mov rcx, 10\n"
                        "   \n"
                        "   divLoop:\n"
                        "      xor rdx, rdx\n"
                        "      div rcx\n"
                        "      add rbx, 1\n"
                        "      add rdx, '0'\n"
                        "      push rdx\n"
                        "      cmp rax, 0\n"
                        "      jg divLoop\n"
                        "   \n"
                        "   printLoop:\n"
                        "      call printASCII\n"
                        "      pop rax\n"
                        "      sub rbx, 1\n"
                        "      cmp rbx, 0\n"
                        "      jg printLoop\n"
                        "   \n"
                        "   ret\n"
                        "\n"
                        "printASCII:\n"
                        "   mov rax, 1\n"
                        "   mov rdi, 1\n"
                        "   lea rsi, [rsp + 8]\n"
                        "   mov rdx, 1\n"
                        "   syscall\n"
                        "   ret\n";
        }

    private:
        void push(const std::string& reg) {
            m_output << "    push " << reg << "\n";
            m_stackSize++;
        }

        void pop(const std::string& reg) {
            m_output << "    pop " << reg << "\n";
            m_stackSize--;
        }

        void generateScope(const std::vector<StatementNode*> statements) {
            size_t curStackSize = m_stackSize;
            size_t curScopeDepth = m_scopeDepth++;

            for(const StatementNode* statement : statements) {
                generateStatement(statement);
            }

            while(m_stackSize > curStackSize) {
                pop("rax");
            }

            for(auto itr = m_variables.begin(); itr != m_variables.end();) {
                if(itr->second.scopeDepth > curScopeDepth) itr = m_variables.erase(itr);
                else itr++;
            }

            m_scopeDepth--;
        }

        struct Variable{
            int scopeDepth;
            size_t stackLocation;
        };

        const ProgramNode m_program;
        std::stringstream m_output;
        size_t m_stackSize = 0;
        int m_scopeDepth = 0;
        int m_curLabelCount = 0;
        std::unordered_map<std::string, Variable> m_variables;
};