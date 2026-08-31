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
                    if(expressionNodeLiteral->literal.type == TokenType::stringLiteral) {
                        int length = expressionNodeLiteral->literal.value.value().length();

                        generator->m_output << "    mov rsi, " << (length + 1) * 8 << "\n"
                                            << "    call allocateMemory\n";

                        generator->m_output << "    mov [rax], " << length << "\n";
                        for (int i = 0; i < length; ++i) {
                            generator->m_output << "    mov [rax + " << (i + 1) * 8 << "], " << static_cast<int>(expressionNodeLiteral->literal.value.value()[i]) << "\n";
                        }

                        generator->push("rax");

                        if (termExpression->isNegative) {
                            std::cerr << "Line " << termExpression->lineNumber << ": Error: Unary '-' cannot be applied to type 'string'" << std::endl;
                            exit(EXIT_FAILURE);
                        }
                    }
                    else {
                        generator->m_output << "    mov rax, " << expressionNodeLiteral->literal.value.value() << "\n";
                        generator->push("rax");

                        if (termExpression->isNegative) generator->convertToNegative();
                    }
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
                                           "    jge outOfBounds\n"
                                           "    cmp rcx, 0\n"
                                           "    jl outOfBounds\n";

                    generator->m_output << "    add rcx, 1\n" // Account for header node
                                           "    imul rcx, 8\n";

                    generator->m_output << "    mov rax, [rbx + rcx]\n";
                    generator->push("rax");

                    if(termExpression->isNegative) generator->convertToNegative();
                }
                void operator()(const PropertyTerm* propertyTerm) const {
                    if(propertyTerm->property == TokenType::size || propertyTerm->property == TokenType::length) {
                        int stackLocation = generator->m_variables[propertyTerm->identifier.value.value()].stackLocation;
                        generator->m_output << "    mov rbx, [rsp + " << std::to_string((generator->m_stackSize - stackLocation) * 8) << "]\n";

                        generator->m_output << "    mov rax, [rbx]\n";
                        generator->push("rax");

                        if(termExpression->isNegative) generator->convertToNegative();
                    }
                    else {
                        std::cerr << "Line " << termExpression->lineNumber << ": Internal compiler error: Unknown property" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                }
                void operator()(const MethodTerm* methodTerm) const {
                    if(methodTerm->method.type == TokenType::stoi) {
                        generator->generateExpression(methodTerm->expression);
                        generator->pop("r11");

                        generator->m_output << "    call stringToInt\n";

                        generator->push("r8");
                    }
                    else if(methodTerm->method.type == TokenType::toString) {
                        generator->generateExpression(methodTerm->expression);
                        generator->pop("r12");

                        switch (methodTerm->expression->type) {
                            case DataType::Integer:
                                generator->m_output << "    call intToString\n";
                                break;
                            case DataType::Boolean:
                                generator->m_output << "    mov rsi, 16\n"
                                                       "    call allocateMemory\n"
                                                       "    mov [rax], 1\n"
                                                       "    mov [rax + 8], r12\n";
                                break;
                            case DataType::Character:
                                generator->m_output << "    mov rsi, 16\n"
                                                       "    call allocateMemory\n"
                                                       "    mov [rax], 1\n"
                                                       "    mov [rax + 8], r12\n";
                                break;
                            default:
                                std::cerr << "Line " << termExpression->lineNumber << ": Error: Cannot convert type '"
                                          << dataTypeToString(methodTerm->expression->type) << "' into 'string'" << std::endl;
                                exit(EXIT_FAILURE);
                        }

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
                    // DummyTerm - Not meant to be processed
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
                    if(operatorExpressionNode->left->type == DataType::String && operatorExpressionNode->right->type == DataType::String) {
                        generator->generateExpression(operatorExpressionNode->left);
                        generator->generateExpression(operatorExpressionNode->right);

                        generator->pop("r15"); // Right
                        generator->pop("r14"); // Left

                        switch (operatorExpressionNode->_operator) {
                            case TokenType::addition:
                                generator->m_output << "    mov rbx, [r14]\n"
                                                       "    mov r13, [r15]\n"
                                                       "    add r13, rbx\n";

                                generator->m_output << "    mov rsi, r13\n"
                                                       "    add rsi, 1\n"
                                                       "    imul rsi, 8\n"
                                                       "    call allocateMemory\n";

                                generator->m_output << "    mov [rax], r13\n";

                                generator->m_output << "    mov r12, r14\n"
                                                       "    call fillMemory\n";

                                generator->m_output << "    mov r8, rax\n"
                                                       "    mov rbx, [r12]\n"
                                                       "    imul rbx, 8\n"
                                                       "    add rax, rbx\n"
                                                       "    mov r12, r15\n"
                                                       "    call fillMemory\n"
                                                       "    mov rax, r8\n";
                                break;
                            case TokenType::equalTo:
                                generator->m_output << "    call cmpStringEq\n";
                                break;
                            case TokenType::notEqualTo:
                                generator->m_output << "    call cmpStringEq\n"
                                                       "    xor rax, 1\n";
                                break;
                            case TokenType::greaterThan:
                                generator->m_output << "    call cmpStringGt\n";
                                break;
                            case TokenType::lessThanOrEqual:
                                generator->m_output << "    call cmpStringGt\n"
                                                       "    xor rax, 1\n";
                                break;
                            case TokenType::lessThan:
                                generator->m_output << "    call cmpStringLt\n";
                                break;
                            case TokenType::greaterThanOrEqual:
                                generator->m_output << "    call cmpStringLt\n"
                                                       "    xor rax, 1\n";
                                break;
                            default:
                                std::cerr << "Line " << expression->lineNumber
                                          << ": Internal compiler error: Unknown operator" << std::endl;
                                exit(EXIT_FAILURE);
                        }
                    }
                    else if(getGroupType(operatorExpressionNode->left->type) == GroupType::Primitive &&
                            getGroupType(operatorExpressionNode->right->type) == GroupType::Primitive){
                        //Edge-Case: Not operator only has 1 side
                        if (operatorExpressionNode->_operator == TokenType::notOperator) {
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
                                    std::cerr << "Line " << expression->lineNumber
                                              << ": Internal compiler error: Unknown operator" << std::endl;
                                    exit(EXIT_FAILURE);
                            }

                            generator->boundVariable(expression->type);
                        }
                    }
                    else {
                        std::cerr << "Line " << expression->lineNumber
                                  << ": Internal compiler error: Cannot resolve expression" << std::endl;
                        exit(EXIT_FAILURE);
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
                        case DataType::Boolean:
                            generator->pop("r12");
                            generator->m_output << "    call intToString\n";
                            generator->push("rax");
                            generator->m_output << "    call printString\n";
                            generator->pop("rax");
                            break;
                        case DataType::Character:
                            generator->pop("r12");
                            generator->m_output << "    mov rsi, 16\n"
                                                   "    call allocateMemory\n"
                                                   "    mov [rax], 1\n"
                                                   "    mov [rax + 8], r12\n";
                            generator->push("rax");
                            generator->m_output << "    call printString\n";
                            generator->pop("rax");
                            break;
                        case DataType::String:
                            generator->m_output << "    call printString\n";
                            break;
                        default:
                            std::cerr << "Line " << statementNode->lineNumber << ": Internal compiler error: Unknown data type"  << std::endl;
                            exit(EXIT_FAILURE);
                    }
                }
                void operator()(const DeclarationNode* declarationNode) const {
                    if (generator->m_variables.contains(declarationNode->identifier.value.value())) {
                        std::cerr << "Line " << statementNode->lineNumber << ": Error: Redefinition of variable '"
                                  << declarationNode->identifier.value.value() << "'" << std::endl;
                        exit(EXIT_FAILURE);
                    }

                    if(getGroupType(declarationNode->type) == GroupType::Primitive) {
                        generator->generateExpression(declarationNode->expression);
                        generator->pop("rax");

                        generator->boundVariable(declarationNode->type);

                        generator->push("rax");

                        generator->m_variables.insert({declarationNode->identifier.value.value(),
                                                       Variable{.scopeDepth = generator->m_scopeDepth,
                                                               .stackLocation = generator->m_stackSize,
                                                               .type = declarationNode->type}});
                    }
                    else if(getGroupType(declarationNode->type) == GroupType::Strings) {
                        generator->generateExpression(declarationNode->expression);
                        generator->pop("r12");

                        generator->m_output << "    mov rsi, [r12]\n"
                                               "    add rsi, 1\n"
                                               "    imul rsi, 8\n"
                                               "    call allocateMemory\n";

                        generator->m_output << "    mov rdx, [r12]\n"
                                               "    mov [rax], rdx\n"
                                               "    call fillMemory\n";

                        generator->push("rax");
                        generator->m_variables.insert({declarationNode->identifier.value.value(), Variable {.scopeDepth = generator->m_scopeDepth,
                                .stackLocation = generator->m_stackSize,
                                .type = DataType::String}});
                    }
                    else if(getGroupType(declarationNode->type) == GroupType::Arrays) {
                        generator->generateExpression(declarationNode->expression);
                        generator->pop("rsi");

                        generator->m_output << "    cmp rsi, 0\n"
                                               "    jle invalidArraySize\n";

                        generator->m_output << "    mov r12, rsi\n"; // Store size

                        generator->m_output << "    add rsi, 1\n"; // Account for header node (size)
                        generator->m_output << "    imul rsi, 8\n";

                        generator->m_output << "    call allocateMemory\n";

                        generator->m_output << "    mov [rax], r12\n";

                        generator->push("rax");
                        generator->m_variables.insert({declarationNode->identifier.value.value(), Variable {.scopeDepth = generator->m_scopeDepth,
                                .stackLocation = generator->m_stackSize,
                                .type = getPrimitiveVariant(declarationNode->type)}});
                    }
                    else {
                        std::cerr << "Line " << statementNode->lineNumber << ": Internal compiler error: Unknown data type" << std::endl;
                        exit(EXIT_FAILURE);
                    }
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

                        generator->generateExpression(reAssignmentNode->index.value());

                        generator->generateExpression(reAssignmentNode->expression);
                        generator->pop("rax");
                        generator->boundVariable(getPrimitiveVariant(generator->m_variables[reAssignmentNode->identifier.value.value()].type));
                        generator->pop("r10");

                        generator->m_output << "    cmp r10, r8\n"
                                               "    jge outOfBounds\n"
                                               "    cmp r10, 0\n"
                                               "    jl outOfBounds\n";

                        generator->m_output << "    add r10, 1\n" // Account for header node
                                               "    imul r10, 8\n";

                        generator->m_output << "    mov rbx, [rsp + " << std::to_string((generator->m_stackSize - stackLocation) * 8) << "]\n";
                        generator->m_output << "    mov [rbx + r10], rax\n";
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
                        "    invalidStoiArg db \"Error: Invalid integer string: string must contain only digits (0-9), with an optional leading '-'\", 10\n"
                        "    newLine db 10\n"
                        "\n";
        }

        void generateUtil() {
            m_output << "\n\n\n ; Util Functions \n\n\n";

            // The address to the string must be stored at the top of the stack before calling
            m_output << "\nprintString:\n"
                        "    mov r8, [rsp + 8]\n"
                        "    mov rbx, [r8]\n"
                        "    mov r12, 8\n"
                        "    cmp rbx, 0\n"
                        "    jle donePrinting\n"
                        "    printStringLoop:\n"
                        "        lea rsi, [r8 + r12]\n"
                        "        mov rax, 1\n"
                        "        mov rdi, 1\n"
                        "        mov rdx, 1\n"
                        "        syscall\n"
                        "        sub rbx, 1\n"
                        "        add r12, 8\n"
                        "        cmp rbx, 0\n"
                        "        jg printStringLoop\n"
                        "    donePrinting:\n"
                        "        mov rax, 1\n"
                        "        mov rdi, 1\n"
                        "        mov rsi, newLine\n"
                        "        mov rdx, 1\n"
                        "        syscall\n"
                        "        ret\n";

            // Boolean must be stored in rax before calling
            // Returns the boolean in rax
            m_output << "\nboundBool:\n"
                        "    cmp rax, 0\n"
                        "    setne al\n"
                        "    movzx rax, al\n"
                        "    ret\n";

            // Char must be stored in rax before calling
            // Returns the char in rax
            m_output << "\nboundChar:\n"
                        "    mov rbx, 128\n"
                        "    cqo\n"
                        "    idiv rbx\n"
                        "    test rdx, rdx\n"
                        "    jns doneDividing\n"
                        "    add rdx, 128\n"
                        "    doneDividing:\n"
                        "        mov rax, rdx\n"
                        "    ret\n";

            // Allocation size must be stored in rsi before calling (in bytes)
            m_output << "\nallocateMemory:\n"
                        "    mov rdi, 0\n"
                        "    mov rdx, 0x3\n"
                        "    mov r10, 0x22\n"
                        "    mov r8, -1\n"
                        "    mov r9, 0\n"
                        "    mov rax, 9\n"
                        "    syscall\n"
                        "    ret\n";

            // Old address must be stored in r12, and new address in rax before calling
            m_output << "\nfillMemory:\n"
                        "    mov rbx, [r12]\n"
                        "    mov rcx, 8\n"
                        "\n"
                        "    fillLoop:\n"
                        "        cmp rbx, 0\n"
                        "        jle doneFilling\n"
                        "        mov rdx, [r12 + rcx]\n"
                        "        mov [rax + rcx], rdx\n"
                        "        sub rbx, 1\n"
                        "        add rcx, 8\n"
                        "        jmp fillLoop\n"
                        "    doneFilling:\n"
                        "        ret\n";

            // Left string address must be stored in r14, and right string address in r15 before calling
            // Returns the boolean in rax
            m_output << "\ncmpStringEq:\n"
                        "    mov r8, [r14]\n"
                        "    mov r9, [r15]\n"
                        "    cmp r8, r9\n"
                        "    jne isNotEqual\n"
                        "    mov rbx, r8\n"
                        "    mov rcx, 8\n"
                        "    cmpEqLoop:\n"
                        "        cmp rbx, 0\n"
                        "        jle isEqual\n"
                        "        mov r12, [r14 + rcx]\n"
                        "        mov r13, [r15 + rcx]\n"
                        "        cmp r12, r13\n"
                        "        jne isNotEqual\n"
                        "        sub rbx, 1\n"
                        "        add rcx, 8\n"
                        "        jmp cmpEqLoop\n"
                        "    isEqual:\n"
                        "        mov rax, 1\n"
                        "        ret\n"
                        "    isNotEqual:\n"
                        "        mov rax, 0\n"
                        "        ret\n";

            // Left string address must be stored in r14, and right string address in r15 before calling
            // Returns the boolean in rax
            m_output << "\ncmpStringGt:\n"
                        "    mov r8, [r14]\n"
                        "    mov r9, [r15]\n"
                        "    mov rbx, 0\n"
                        "    mov rcx, 8\n"
                        "    cmpGtLoop:"
                        "        cmp rbx, r8\n"
                        "        jge lessEqual\n"
                        "        cmp rbx, r9\n"
                        "        jge greater\n"
                        "        mov r12, [r14 + rcx]\n"
                        "        mov r13, [r15 + rcx]\n"
                        "        cmp r12, r13\n"
                        "        jg greater\n"
                        "        jl lessEqual\n"
                        "        add rbx, 1\n"
                        "        add rcx, 8\n"
                        "        jmp cmpGtLoop\n"
                        "    greater:\n"
                        "        mov rax, 1\n"
                        "        ret\n"
                        "    lessEqual:\n"
                        "        mov rax, 0\n"
                        "        ret\n";

            // Left string address must be stored in r14, and right string address in r15 before calling
            // Returns the boolean in rax
            m_output << "\ncmpStringLt:\n"
                        "    mov r8, [r14]\n"
                        "    mov r9, [r15]\n"
                        "    mov rbx, 0\n"
                        "    mov rcx, 8\n"
                        "    cmpLtLoop:"
                        "        cmp rbx, r9\n"
                        "        jge greaterEqual\n"
                        "        cmp rbx, r8\n"
                        "        jge less\n"
                        "        mov r12, [r14 + rcx]\n"
                        "        mov r13, [r15 + rcx]\n"
                        "        cmp r12, r13\n"
                        "        jl less\n"
                        "        jg greaterEqual\n"
                        "        add rbx, 1\n"
                        "        add rcx, 8\n"
                        "        jmp cmpLtLoop\n"
                        "    less:\n"
                        "        mov rax, 1\n"
                        "        ret\n"
                        "    greaterEqual:\n"
                        "        mov rax, 0\n"
                        "        ret\n";

            // String address must be stored in r11 before calling
            // Returns the integer in r8
            m_output << "\nstringToInt:\n"
                        "    mov r12, [r11]\n"
                        "    mov r13, 8\n"
                        "    mov r14, 10\n"
                        "    cmp r12, 0\n"
                        "    je stoiEmptyString\n"
                        "    mov rdx, [r11 + 8]\n"
                        "    cmp rdx, 45\n"
                        "    jne stoiIsPositive\n"
                        "    sub r12, 1\n"
                        "    add r13, 8\n"
                        "    stoiIsPositive:\n"
                        "    mov r8, 0\n"
                        "    mov rax, 1\n"
                        "    mov rbx, r12\n"
                        "    mov rcx, r13\n"
                        "    stoiCheckLoop:\n"
                        "        cmp rbx, 0\n"
                        "        jle stoiDoneChecking\n"
                        "        mov rdx, [r11 + rcx]\n"
                        "        cmp rdx, 48\n"
                        "        jl stoiInvalidString\n"
                        "        cmp rdx, 57\n"
                        "        jg stoiInvalidString\n"
                        "        sub rbx, 1\n"
                        "        add rcx, 8\n"
                        "        imul rax, 10\n"
                        "        jmp stoiCheckLoop\n"
                        "    stoiDoneChecking:\n"
                        "        mov rbx, r12\n"
                        "        mov rcx, r13\n"
                        "        cqo\n"
                        "        idiv r14\n"
                        "    stoiConvertLoop:\n"
                        "        cmp rbx, 0\n"
                        "        jle stoiDoneConverting\n"
                        "        mov r10, [r11 + rcx]\n"
                        "        sub r10, 48\n"
                        "        imul r10, rax\n"
                        "        add r8, r10\n"
                        "        cqo\n"
                        "        idiv r14\n"
                        "        sub rbx, 1\n"
                        "        add rcx, 8\n"
                        "        jmp stoiConvertLoop\n"
                        "    stoiDoneConverting:\n"
                        "    cmp r13, 16\n"
                        "    jne stoiDontTurnNegative\n"
                        "    imul r8, -1\n"
                        "    stoiDontTurnNegative:\n"
                        "        ret\n"
                        "    stoiEmptyString:\n"
                        "        mov r8, 0\n"
                        "        ret\n"
                        "    stoiInvalidString:\n"
                        "        mov rax, 1\n"
                        "        mov rdi, 1\n"
                        "        mov rsi, invalidStoiArg\n"
                        "        mov rdx, 99\n"
                        "        syscall\n"
                        "        mov rax, 1\n"
                        "        mov rdi, 1\n"
                        "        mov rsi, exitMsg\n"
                        "        mov rdx, 32\n"
                        "        syscall\n"
                        "        mov rax, 60\n"
                        "        mov rdi, 1\n"
                        "        syscall\n";

            // Integer value must be stored in r12 before calling
            // Returns the string address in rax
            m_output << "\nintToString:\n"
                        "    mov rax, r12\n"
                        "    mov rbx, 0\n"
                        "    mov rcx, 10\n"
                        "    mov r15, 0\n"
                        "    cmp rax, 0\n"
                        "    jne intToStringNotZero\n"
                        "    mov rdx, 48\n"
                        "    push rdx\n"
                        "    mov rbx, 1\n"
                        "    jmp intToStringDoneConverting\n"
                        "    intToStringNotZero:\n"
                        "    cmp rax, 0\n"
                        "    jge intToStringIsPositive\n"
                        "    mov r15, 1\n"
                        "    imul rax, -1\n"
                        "    intToStringIsPositive:\n"
                        "    intToStringConvertLoop:\n"
                        "        cmp rax, 0\n"
                        "        jle intToStringDoneConverting\n"
                        "        cqo\n"
                        "        idiv rcx\n"
                        "        add rdx, 48\n"
                        "        push rdx\n"
                        "        add rbx, 1\n"
                        "        jmp intToStringConvertLoop\n"
                        "    intToStringDoneConverting:\n"
                        "        cmp r15, 0\n"
                        "        je intToStringDontAddNegative\n"
                        "        mov rdx, 45\n"
                        "        push rdx\n"
                        "        add rbx, 1\n"
                        "        intToStringDontAddNegative:\n"
                        "        mov rsi, rbx\n"
                        "        add rsi, 1\n"
                        "        imul rsi, 8\n"
                        "        call allocateMemory\n"
                        "        mov rcx, 8\n"
                        "        mov [rax], rbx\n"
                        "    intToStringFillString:\n"
                        "        cmp rbx, 0\n"
                        "        jle intToStringDoneFilling\n"
                        "        pop rdx\n"
                        "        mov [rax + rcx], rdx\n"
                        "        sub rbx, 1\n"
                        "        add rcx, 8\n"
                        "        jmp intToStringFillString\n"
                        "    intToStringDoneFilling:\n"
                        "        ret\n";
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
                    m_output << "    call boundBool\n";
                    break;
                case DataType::Character:
                    m_output << "    call boundChar\n";
                    break;
                default:
                    break;
            }
        }

        void convertToNegative() {
            m_output << "    pop rax\n";
            m_output << "    imul rax, -1\n";
            m_output << "    push rax\n";
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