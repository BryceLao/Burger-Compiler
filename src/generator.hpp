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
                const TermExpressionNode* termExpression;

                void operator()(const LiteralTerm* expressionNodeLiteral) const {
                    generator->m_output << "    mov rax, " << expressionNodeLiteral->literal.value.value() << "\n";
                    generator->push("rax");

                    if(termExpression->isNegative) generator->convertToNegative();
                }
                void operator()(const IdentifierTerm* expressionNodeIdentifier) const {
                    if(generator->m_variables.find(expressionNodeIdentifier->identifier.value.value()) == generator->m_variables.end()) {
                        std::cerr << "Line " << termExpression->lineNumber << ": Error: Use of undeclared variable '" << expressionNodeIdentifier->identifier.value.value() << "'" << std::endl;
                        exit(EXIT_FAILURE);
                    }

                    int stackLocation = generator->m_variables[expressionNodeIdentifier->identifier.value.value()].stackLocation;
                    generator->push("[rsp + " + std::to_string((generator->m_stackSize - stackLocation) * 8) + "]");

                    if(termExpression->isNegative) generator->convertToNegative();
                }
                void operator()(const IndexedTerm* indexedTerm) const {
                    generator->generateExpression(indexedTerm->index);
                    generator->pop("rcx");

                    int stackLocation = generator->m_variables[indexedTerm->identifier.value.value()].stackLocation;
                    generator->m_output << "    mov rbx, [rsp + " << std::to_string((generator->m_stackSize - stackLocation) * 8) << "]\n";
                    generator->m_output << "    mov r8, [rbx]\n";

                    generator->m_output << "    cmp rcx, r8\n"
                                           "    jg outOfBounds\n"
                                           "    cmp rcx, 0\n"
                                           "    jl outOfBounds\n";

                    generator->m_output << "    add rcx, 1\n" // Account for header node
                                           "    imul rcx, 8\n";

                    generator->m_output << "    mov rax, [rbx + rcx]\n";
                    generator->push("rax");

                    if(termExpression->isNegative) generator->convertToNegative();
                }
                void operator()(const PropertyTerm* propertyTerm) const {
                    if(propertyTerm->property == TokenType::size) {
                        int stackLocation = generator->m_variables[propertyTerm->identifier.value.value()].stackLocation;
                        generator->m_output << "    mov rbx, [rsp + " << std::to_string((generator->m_stackSize - stackLocation) * 8) << "]\n";

                        generator->m_output << "    mov rax, [rbx]\n"
                                               "    add rax, 1\n";
                        generator->push("rax");
                    }
                    else {

                    }
                }
                void operator()(const ParenthesisTerm* parenthesisTerm) const {
                    generator->generateExpression(parenthesisTerm->expression);
                    if(termExpression->isNegative) generator->convertToNegative();
                }
                void operator()(const DummyTerm* dummyTerm) const {

                }
            };

            TermVisitor visitor {.generator = this, .termExpression = termExpression};
            std::visit(visitor, termExpression->variant);
        }

        void generateExpression(const ExpressionNode* expression) {
            struct ExpressionVisitor {
                Generator* generator;
                const ExpressionNode* expression;

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
                                generator->m_output << "    cqo\n";
                                generator->m_output << "    imul rax,rbx\n";
                                break;
                            case (TokenType::division):
                                generator->m_output << "    cqo\n";
                                generator->m_output << "    idiv rbx\n";
                                break;
                            case (TokenType::modulo):
                                generator->m_output << "    cqo\n";
                                generator->m_output << "    idiv rbx\n";
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
                                generator->m_output << "    cmp rax, 0\n";
                                generator->m_output << "    setne al\n";
                                generator->m_output << "    movzx rax, al\n";

                                generator->m_output << "    cmp rbx, 0\n";
                                generator->m_output << "    setne bl\n";
                                generator->m_output << "    movzx rbx, bl\n";

                                generator->m_output << "    add rax, rbx\n";

                                generator->m_output << "    cmp rax, 2\n";
                                generator->m_output << "    setge al\n";
                                generator->m_output << "    movzx rax, al\n";

                                break;
                            case (TokenType::orOperator):
                                //Converts Non-Zero Integers as True booleans
                                generator->m_output << "    cmp rax, 0\n";
                                generator->m_output << "    setne al\n";
                                generator->m_output << "    movzx rax, al\n";

                                generator->m_output << "    cmp rbx, 0\n";
                                generator->m_output << "    setne bl\n";
                                generator->m_output << "    movzx rbx, bl\n";

                                generator->m_output << "    add rax, rbx\n";

                                generator->m_output << "    cmp rax, 1\n";
                                generator->m_output << "    setge al\n";
                                generator->m_output << "    movzx rax, al\n";
                                break;
                            default:
                                std::cerr << "Line " << expression->lineNumber << ": Internal compiler error: Unknown operator" << std::endl;
                                exit(EXIT_FAILURE);
                        }
                    }

                    generator->push("rax");

                }
            };

            ExpressionVisitor visitor {.generator = this, .expression = expression};
            std::visit(visitor, expression->variant);
        }

        void generateStatement(const StatementNode* statement) {
            struct StatementVisitor {
                Generator* generator;
                const StatementNode* statementNode;

                void operator()(const ExitNode* exitNode) const {
                    generator->generateExpression(exitNode->expression);

                    // Exit Code can only be an integer
                    if(exitNode->expression->type != DataType::Integer) {
                        std::cerr << "Line " << statementNode->lineNumber << ": Error: Exit code must be an integer" << std::endl;
                        exit(EXIT_FAILURE);
                    }

                    generator->m_output << "    mov rax, 60\n";
                    generator->pop("rdi");
                    generator->m_output << "    syscall\n";
                }
                void operator()(const PrintNode* printNode) const {
                    generator->generateExpression(printNode->expression);

                    switch(printNode->expression->type) {
                        case DataType::Integer:
                            generator->m_output << "    call printInt\n";
                            break;
                        case DataType::Boolean:
                            generator->m_output << "    call printBool\n";
                            break;
                        case DataType::Character:
                            generator->m_output << "    call printChar\n";
                            break;
                        default:
                            std::cerr << "Line " << statementNode->lineNumber << ": Internal compiler error: Unknown data type"  << std::endl;
                            exit(EXIT_FAILURE);
                    }
                }
                void operator()(const PrimitiveDeclarationNode* declarationNode) const {
                    if(generator->m_variables.contains(declarationNode->identifier.value.value())) {
                        std::cerr << "Line " << statementNode->lineNumber << ": Error: Redefinition of variable '" << declarationNode->identifier.value.value() << "'" << std::endl;
                        exit(EXIT_FAILURE);
                    }

                    generator->generateExpression(declarationNode->expression);
                    generator->pop("rax");

                    generator->boundVariable(declarationNode->type);

                    generator->push("rax");

                    generator->m_variables.insert({declarationNode->identifier.value.value(), Variable {.scopeDepth = generator->m_scopeDepth,
                            .stackLocation = generator->m_stackSize,
                            .type = declarationNode->type}});
                }
                void operator()(const ArrayDeclarationNode* declarationNode) const {
                    if (generator->m_variables.contains(declarationNode->identifier.value.value())) {
                        std::cerr << "Line " << statementNode->lineNumber << ": Error: Redefinition of variable '"
                                  << declarationNode->identifier.value.value() << "'" << std::endl;
                        exit(EXIT_FAILURE);
                    }

                    generator->generateExpression(declarationNode->size);
                    generator->pop("rsi");

                    generator->m_output << "    cmp rsi, 0\n"
                                           "    jle invalidArraySize\n";

                    generator->m_output << "    mov r12, rsi\n"; // Store size

                    generator->m_output << "    add rsi, 1\n"; // Account for header node (size)
                    generator->m_output << "    imul rsi, 8\n";

                    generator->m_output << "    mov rdi, 0\n"
                                           "    mov rdx, 0x3\n"
                                           "    mov r10, 0x22\n"
                                           "    mov r8, -1\n"
                                           "    mov r9, 0\n"
                                           "    mov rax, 9\n"
                                           "    syscall\n";

                    generator->m_output << "    sub r12, 1\n"
                                           "    mov [rax], r12\n";

                    generator->push("rax");
                    generator->m_variables.insert({declarationNode->identifier.value.value(), Variable {.scopeDepth = generator->m_scopeDepth,
                            .stackLocation = generator->m_stackSize,
                            .type = getPrimitiveVariant(declarationNode->type)}});
                }
                void operator()(const ReAssignmentNode* reAssignmentNode) const {
                    if(!generator->m_variables.contains(reAssignmentNode->identifier.value.value())) {
                        std::cerr << "Line " << statementNode->lineNumber << ": Error: Use of undeclared variable '" << reAssignmentNode->identifier.value.value() << "'" << std::endl;
                        exit(EXIT_FAILURE);
                    }

                    if(reAssignmentNode->index.has_value()) {
                        int stackLocation = generator->m_variables[reAssignmentNode->identifier.value.value()].stackLocation;
                        generator->m_output << "    mov rbx, [rsp + "
                                            << std::to_string((generator->m_stackSize - stackLocation) * 8)
                                            << "]\n";
                        generator->m_output << "    mov r8, [rbx]\n";

                        generator->generateExpression(reAssignmentNode->expression);
                        generator->pop("r9");

                        generator->generateExpression(reAssignmentNode->index.value());
                        generator->pop("r10");

                        generator->m_output << "    cmp r10, r8\n"
                                               "    jg outOfBounds\n"
                                               "    cmp r10, 0\n"
                                               "    jl outOfBounds\n";

                        generator->m_output << "    add r10, 1\n" // Account for header node
                                               "    imul r10, 8\n";

                        generator->m_output << "    mov rax, [rsp + " << std::to_string((generator->m_stackSize - stackLocation) * 8) << "]\n";

                        generator->m_output << "    mov [rax + r10], r9\n";
                    }
                    else {
                        generator->generateExpression(reAssignmentNode->expression);
                        generator->pop("rax");
                        generator->boundVariable(
                                generator->m_variables[reAssignmentNode->identifier.value.value()].type);

                        int stackLocation = generator->m_variables[reAssignmentNode->identifier.value.value()].stackLocation;
                        generator->m_output << "    mov [rsp + "
                                            << std::to_string((generator->m_stackSize - stackLocation) * 8)
                                            << "], rax\n";
                    }
                }
                void operator()(const ScopeNode* scopeNode) const {
                    generator->generateScope(scopeNode->statements);
                }
                void operator()(const ConditionalNode* conditionalNode) {
                    int endLabel = generator->m_curLabelCount++;

                    for (int i = 0; i < conditionalNode->statements.size(); ++i) {
                        // If an Else Statement
                        if(i == conditionalNode->condition.size()) {
                            generator->generateScope(conditionalNode->statements[i]);
                        }
                        else {
                            int curLabel = generator->m_curLabelCount++;

                            generator->generateExpression(conditionalNode->condition[i]);
                            generator->pop("rax");
                            generator->m_output << "    cmp rax, 0\n";
                            generator->m_output << "    je a" << curLabel << "\n";

                            generator->generateScope(conditionalNode->statements[i]);

                            generator->m_output << "    jmp a" << endLabel << "\n";

                            generator->m_output << "a" << curLabel << ":\n";
                        }
                    }
                    generator->m_output << "a" << endLabel << ":\n";
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
                    generator->m_output << "    cmp rax, 0\n";
                    generator->m_output << "    jne a" << statementLabel << "\n";
                }
            };

            StatementVisitor visitor {.generator = this, .statementNode = statement};
            std::visit(visitor, statement->variant);
        }

        [[nodiscard]] std::string generateProgram() {
            generateData();

            m_output << "section .text\nglobal _start\n_start:\n";

            for(const StatementNode* statement : m_program.statements) {
                generateStatement(statement);
            }

            m_output << "    mov rax, 1\n"
                        "    mov rdi, 1\n"
                        "    mov rsi, exitMsg\n"
                        "    mov rdx, 32\n"
                        "    syscall\n";

            m_output << "    mov rax, 60\n";
            m_output << "    mov rdi, 0\n";
            m_output << "    syscall\n";

            m_output << "outOfBounds:\n";
            m_output << "    mov rax, 1\n"
                        "    mov rdi, 1\n"
                        "    mov rsi, oobMsg\n"
                        "    mov rdx, 33\n"
                        "    syscall\n"
                        "    jmp errorExit\n";

            m_output << "invalidArraySize:\n";
            m_output << "    mov rax, 1\n"
                        "    mov rdi, 1\n"
                        "    mov rsi, negSizeArray\n"
                        "    mov rdx, 44\n"
                        "    syscall\n"
                        "    jmp errorExit\n";

            m_output << "errorExit:\n"
                        "    mov rax, 1\n"
                        "    mov rdi, 1\n"
                        "    mov rsi, exitMsg\n"
                        "    mov rdx, 32\n"
                        "    syscall\n";

            m_output << "    mov rax, 60\n";
            m_output << "    mov rdi, 1\n";
            m_output << "    syscall\n";


            generateUtil();

            return m_output.str();
        }

        void generateData() {
            m_output << "section .data\n"
                        "    exitMsg db \"Process finished with exit code \"\n"
                        "    oobMsg db \"Error: Array index out of bounds\", 10\n"
                        "    negSizeArray db \"Error: Array size must be greater than zero\", 10\n"
                        "    trueString db \"True\"\n"
                        "    falseString db \"False\"\n"
                        "    newLine db 10\n"
                        "\n";
        }

        void generateUtil() {
            m_output << "\n\n\n ; Util Functions \n\n\n";

            m_output << "printInt:\n"
                        "    mov rbx, 1\n"
                        "    mov rax, 10\n"   // New Line Character
                        "    push rax\n"
                        "    mov rax, [rsp + 16]\n"
                        "    mov rcx, 10\n"
                        "    cmp rax, 0\n"
                        "    jge positiveInt\n"
                        "    imul rax, -1\n"
                        "    mov r8, 1\n" // Flag for if number is negative
                        "    positiveInt:\n"
                        "   \n"
                        "    divLoop:\n"
                        "        xor rdx, rdx\n"
                        "        div rcx\n"
                        "        add rbx, 1\n"
                        "        add rdx, '0'\n"
                        "        push rdx\n"
                        "        cmp rax, 0\n"
                        "        jg divLoop\n"
                        "   \n"
                        "    cmp r8, 1\n"
                        "    jne printLoop\n"
                        "    mov rax, 45\n" // Add negative character to print queue
                        "    push rax\n"
                        "    add rbx, 1\n"
                        "    xor r8, r8\n"
                        "    printLoop:\n"
                        "        call printASCII\n"
                        "        pop rax\n"
                        "        sub rbx, 1\n"
                        "        cmp rbx, 0\n"
                        "        jg printLoop\n"
                        "   \n"
                        "    ret\n"
                        "\n"
                        "printASCII:\n"
                        "    mov rax, 1\n"
                        "    mov rdi, 1\n"
                        "    lea rsi, [rsp + 8]\n"
                        "    mov rdx, 1\n"
                        "    syscall\n"
                        "    ret\n\n";

            m_output << "printBool:\n"
                        "    mov rax, 1\n"
                        "    mov rdi, 1\n"
                        "    mov rsi, [rsp + 8]\n"
                        "    cmp rsi, 0\n"
                        "    je falseStatement\n"
                        "    trueStatement:\n"
                        "        mov rsi, trueString\n"
                        "        mov rdx, 4\n"
                        "        jmp endFunction\n"
                        "    falseStatement:\n"
                        "        mov rsi, falseString\n"
                        "        mov rdx, 5\n"
                        "   endFunction:\n"
                        "        syscall\n"
                        "        mov rax, 1\n"
                        "        mov rdi, 1\n"
                        "        mov rsi, newLine\n"
                        "        mov rdx, 1\n"
                        "        syscall\n"
                        "   \n"
                        "    ret\n"
                        "   \n";

            m_output << "printChar:\n"
                        "    mov rax, 1\n"
                        "    mov rdi, 1\n"
                        "    lea rsi, [rsp + 8]\n"
                        "    mov rdx, 1\n"
                        "    syscall\n"
                        "    mov rax, 1\n"
                        "    mov rdi, 1\n"
                        "    mov rsi, newLine\n"
                        "    mov rdx, 1\n"
                        "    syscall\n"
                        "    ret\n\n";

            m_output << "boundBool:\n"
                        "    cmp rax, 0\n"
                        "    setne al\n"
                        "    movzx rax, al\n"
                        "    ret\n\n";

            m_output << "boundChar:\n"
                        "    mov rbx, 128\n"
                        "    cqo\n"
                        "    idiv rbx\n"
                        "    test rdx, rdx\n"
                        "    jns doneDividing\n"
                        "    add rdx, 128\n"
                        "    doneDividing:\n"
                        "        mov rax, rdx\n"
                        "    ret\n";
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

        void boundVariable(DataType type) {
            switch(type) {
                case DataType::Integer:
                    break;
                case DataType::Boolean:
                    m_output << "   call boundBool\n";
                    break;
                case DataType::Character:
                    m_output << "   call boundChar\n";
                    break;
                default:
                    break;
            }
        }

        void convertToNegative() {
            m_output << "   pop rax\n";
            m_output << "   imul rax, -1\n";
            m_output << "   push rax\n";
        }

        struct Variable{
            int scopeDepth;
            size_t stackLocation;
            DataType type;
        };

        const ProgramNode m_program;
        std::stringstream m_output;
        size_t m_stackSize = 0;
        int m_scopeDepth = 0;
        int m_curLabelCount = 0;
        std::unordered_map<std::string, Variable> m_variables;
};