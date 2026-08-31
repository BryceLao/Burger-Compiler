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

                        generator->m_output << "    mov rsi, " << (length + 1) * VAR_SIZE << "\n"
                                               "    call allocateMemory\n";

                        generator->m_output << "    mov [rax], " << length << "\n";
                        for (int i = 0; i < length; ++i) {
                            generator->m_output << "    mov [rax + " << (i + 1) * VAR_SIZE << "], " << static_cast<int>(expressionNodeLiteral->literal.value.value()[i]) << "\n";
                        }

                        generator->push("rax");
                    }
                    else {
                        generator->m_output << "    mov rax, " << expressionNodeLiteral->literal.value.value() << "\n";
                        generator->push("rax");
                    }
                }
                void operator()(const IdentifierTerm* expressionNodeIdentifier) const {
                    if(generator->m_variables.find(expressionNodeIdentifier->identifier.value.value()) == generator->m_variables.end())
                        throwError(termExpression->lineNumber, "Error: Use of undeclared variable '" +
                        expressionNodeIdentifier->identifier.value.value() + "'");

                    int stackLocation = generator->m_variables[expressionNodeIdentifier->identifier.value.value()].stackLocation;
                    generator->push("[rsp + " + std::to_string((generator->m_stackSize - stackLocation) * VAR_SIZE) + "]");
                }
                void operator()(const IndexedTerm* indexedTerm) const {
                    generator->generateExpression(indexedTerm->index);
                    generator->pop("rcx");

                    int stackLocation = generator->m_variables[indexedTerm->identifier.value.value()].stackLocation;
                    generator->m_output << "    mov rbx, [rsp + " << std::to_string((generator->m_stackSize - stackLocation) * VAR_SIZE) << "]\n"
                                           "    mov r8, [rbx]\n";

                    generator->m_output << "    cmp rcx, r8\n"
                                           "    jge outOfBounds\n"
                                           "    cmp rcx, 0\n"
                                           "    jl outOfBounds\n";

                    generator->m_output << "    add rcx, 1\n" // Account for header node
                                           "    imul rcx, " << VAR_SIZE << "\n";

                    generator->m_output << "    mov rax, [rbx + rcx]\n";
                    generator->push("rax");
                }
                void operator()(const PropertyTerm* propertyTerm) const {
                    if(propertyTerm->property == TokenType::size || propertyTerm->property == TokenType::length) {
                        int stackLocation = generator->m_variables[propertyTerm->identifier.value.value()].stackLocation;
                        generator->m_output << "    mov rbx, [rsp + " << std::to_string((generator->m_stackSize - stackLocation) * VAR_SIZE) << "]\n";

                        generator->m_output << "    mov rax, [rbx]\n";
                        generator->push("rax");
                    }
                    else throwError(termExpression->lineNumber, "Internal compiler error: Unknown property");
                }
                void operator()(const TypeCastTerm* typeCastTerm) const {
                    if(typeCastTerm->typeCast == TokenType::stoi) {
                        generator->generateExpression(typeCastTerm->expression);
                        generator->pop("r11");

                        generator->m_output << "    call stringToInt\n";

                        generator->push("r8");
                    }
                    else if(typeCastTerm->typeCast == TokenType::toString) {
                        generator->generateExpression(typeCastTerm->expression);
                        generator->pop("r12");

                        switch (typeCastTerm->expression->type) {
                            case DataType::Integer:
                                generator->m_output << "    call intToString\n";
                                break;
                            case DataType::Boolean:
                                generator->m_output << "    mov rsi, 16\n"
                                                       "    call allocateMemory\n"
                                                       "    mov [rax], 1\n"
                                                       "    mov [rax + " << VAR_SIZE << "], r12\n";
                                break;
                            case DataType::Character:
                                generator->m_output << "    mov rsi, 16\n"
                                                       "    call allocateMemory\n"
                                                       "    mov [rax], 1\n"
                                                       "    mov [rax + " << VAR_SIZE << "], r12\n";
                                break;
                            default:
                                throwError(termExpression->lineNumber, "Error: Cannot convert type '" +
                                dataTypeToString(typeCastTerm->expression->type) + "' into 'string'");
                        }

                        generator->push("rax");
                    }
                    else if(typeCastTerm->typeCast == TokenType::boolType){
                        generator->generateExpression(typeCastTerm->expression);
                        generator->pop("rax");
                        generator->boundVariable(termExpression->type);
                        generator->push("rax");
                    }
                }
                void operator()(const ParenthesisTerm* parenthesisTerm) const {
                    generator->generateExpression(parenthesisTerm->expression);
                }
                void operator()(const UnaryOperationExpression* unaryTerm) const {
                    generator->generateExpression(unaryTerm->expression);
                    generator->pop("rax");

                    if(unaryTerm->_operator == TokenType::subtraction) {
                        generator->m_output << "    imul rax, -1\n";
                    }
                    else if(unaryTerm->_operator == TokenType::notOperator) {
                        generator->m_output << "    cmp rax, 0\n"
                                               "    sete al\n"
                                               "    movzx rax, al\n";
                    }
                    else throwError(termExpression->lineNumber, "Internal compiler error: Unknown operator");

                    generator->push("rax");
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
                                                       "    imul rsi, " << VAR_SIZE << "\n"
                                                       "    call allocateMemory\n";

                                generator->m_output << "    mov [rax], r13\n";

                                generator->m_output << "    mov r12, r14\n"
                                                       "    call fillMemory\n";

                                generator->m_output << "    mov r8, rax\n"
                                                       "    mov rbx, [r12]\n"
                                                       "    imul rbx, " << VAR_SIZE << "\n"
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
                                throwError(expression->lineNumber, "Internal compiler error: Unknown operator");
                        }
                    }
                    else if(getGroupType(operatorExpressionNode->left->type) == GroupType::Primitive &&
                            getGroupType(operatorExpressionNode->right->type) == GroupType::Primitive) {
                        generator->generateExpression(operatorExpressionNode->left);
                        generator->generateExpression(operatorExpressionNode->right);

                        generator->pop("rbx");
                        generator->pop("rax");

                        switch (operatorExpressionNode->_operator) {
                            case (TokenType::addition):
                                generator->m_output << "    add rax, rbx\n";
                                break;
                            case (TokenType::subtraction):
                                generator->m_output << "    sub rax, rbx\n";
                                break;
                            case (TokenType::multiplication):
                                generator->m_output << "    cqo\n"
                                                       "    imul rax, rbx\n";
                                break;
                            case (TokenType::division):
                                generator->m_output << "    cqo\n"
                                                       "    idiv rbx\n";
                                break;
                            case (TokenType::modulo):
                                generator->m_output << "    cqo\n"
                                                       "    idiv rbx\n"
                                                       "    mov rax, rdx\n";
                                break;
                            case (TokenType::equalTo):
                                generator->m_output << "    cmp rax, rbx\n"
                                                       "    sete al\n"
                                                       "    movzx rax, al\n";
                                break;
                            case (TokenType::notEqualTo):
                                generator->m_output << "    cmp rax, rbx\n"
                                                       "    setne al\n"
                                                       "    movzx rax, al\n";
                                break;
                            case (TokenType::lessThan):
                                generator->m_output << "    cmp rax, rbx\n"
                                                       "    setl al\n"
                                                       "    movzx rax, al\n";
                                break;
                            case (TokenType::lessThanOrEqual):
                                generator->m_output << "    cmp rax, rbx\n"
                                                       "    setle al\n"
                                                       "    movzx rax, al\n";
                                break;
                            case (TokenType::greaterThan):
                                generator->m_output << "    cmp rax, rbx\n"
                                                       "    setg al\n"
                                                       "    movzx rax, al\n";
                                break;
                            case (TokenType::greaterThanOrEqual):
                                generator->m_output << "    cmp rax, rbx\n"
                                                       "    setge al\n"
                                                       "    movzx rax, al\n";
                                break;
                            case (TokenType::andOperator):
                                //Converts Non-Zero Integers as True booleans
                                generator->m_output << "    cmp rax, 0\n"
                                                       "    setne al\n"
                                                       "    movzx rax, al\n";

                                generator->m_output << "    cmp rbx, 0\n"
                                                       "    setne bl\n"
                                                       "    movzx rbx, bl\n";

                                generator->m_output << "    add rax, rbx\n";

                                generator->m_output << "    cmp rax, 2\n"
                                                       "    setge al\n"
                                                       "    movzx rax, al\n";

                                break;
                            case (TokenType::orOperator):
                                //Converts Non-Zero Integers as True booleans
                                generator->m_output << "    cmp rax, 0\n"
                                                       "    setne al\n"
                                                       "    movzx rax, al\n";

                                generator->m_output << "    cmp rbx, 0\n"
                                                       "    setne bl\n"
                                                       "    movzx rbx, bl\n";

                                generator->m_output << "    add rax, rbx\n";

                                generator->m_output << "    cmp rax, 1\n"
                                                       "    setge al\n"
                                                       "    movzx rax, al\n";
                                break;
                            default:
                                throwError(expression->lineNumber, "Internal compiler error: Unknown operator");
                        }

                        generator->boundVariable(expression->type);
                    }
                    else throwError(expression->lineNumber, "Internal compiler error: Cannot resolve expression");

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
                    if(exitNode->expression->type != DataType::Integer) throwError(statementNode->lineNumber, "Error: Exit code must be an integer");

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
                            generator->m_output << "    call intToString\n"
                                                   "    mov r8, rax\n"
                                                   "    call printString\n";
                            break;
                        case DataType::Character:
                            generator->pop("r12");
                            generator->m_output << "    mov rsi, 16\n"
                                                   "    call allocateMemory\n"
                                                   "    mov [rax], 1\n"
                                                   "    mov [rax + " << VAR_SIZE << "], r12\n"
                                                   "    mov r8, rax\n"
                                                   "    call printString\n";
                            break;
                        case DataType::String:
                            generator->pop("r8");
                            generator->m_output << "    call printString\n";
                            break;
                        default:
                            throwError(statementNode->lineNumber, "Internal compiler error: Unknown data type");
                    }
                }
                void operator()(const DeclarationNode* declarationNode) const {
                    if (generator->m_variables.contains(declarationNode->identifier.value.value()))
                        throwError(statementNode->lineNumber, "Error: Redefinition of variable '" +
                        declarationNode->identifier.value.value() + "'");

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
                                               "    imul rsi, " << VAR_SIZE << "\n"
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

                        generator->m_output << "    add rsi, 1\n"
                                               "    imul rsi, " << VAR_SIZE << "\n";

                        generator->m_output << "    call allocateMemory\n";

                        generator->m_output << "    mov [rax], r12\n";

                        generator->push("rax");
                        generator->m_variables.insert({declarationNode->identifier.value.value(), Variable {.scopeDepth = generator->m_scopeDepth,
                                .stackLocation = generator->m_stackSize,
                                .type = getPrimitiveVariant(declarationNode->type)}});
                    }
                    else throwError(statementNode->lineNumber, "Internal compiler error: Unknown data type");
                }
                void operator()(const ReAssignmentNode* reAssignmentNode) const {
                    if(!generator->m_variables.contains(reAssignmentNode->identifier.value.value()))
                        throwError(statementNode->lineNumber, "Error: Use of undeclared variable '" + reAssignmentNode->identifier.value.value() + "'");

                    if(reAssignmentNode->index.has_value()) {
                        int stackLocation = generator->m_variables[reAssignmentNode->identifier.value.value()].stackLocation;
                        generator->m_output << "    mov rbx, [rsp + "
                                            << std::to_string((generator->m_stackSize - stackLocation) * VAR_SIZE)
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
                                               "    imul r10, " << VAR_SIZE << "\n";

                        generator->m_output << "    mov rbx, [rsp + " << std::to_string((generator->m_stackSize - stackLocation) * VAR_SIZE) << "]\n"
                                               "    mov [rbx + r10], rax\n";
                    }
                    else {
                        generator->generateExpression(reAssignmentNode->expression);
                        generator->pop("rax");
                        generator->boundVariable(
                                generator->m_variables[reAssignmentNode->identifier.value.value()].type);

                        int stackLocation = generator->m_variables[reAssignmentNode->identifier.value.value()].stackLocation;
                        generator->m_output << "    mov [rsp + "
                                            << std::to_string((generator->m_stackSize - stackLocation) * VAR_SIZE)
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
                            generator->m_output << "    cmp rax, 0\n"
                                                   "    je a" << curLabel << "\n";

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
                    generator->m_output << "    cmp rax, 0\n"
                                           "    jne a" << statementLabel << "\n";
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

            m_output << "    mov rax, 60\n"
                        "    mov rdi, 0\n"
                        "    syscall\n";

            m_output << "outOfBounds:\n"
                        "    mov rax, 1\n"
                        "    mov rdi, 1\n"
                        "    mov rsi, oobMsg\n"
                        "    mov rdx, 33\n"
                        "    syscall\n"
                        "    jmp errorExit\n";

            m_output << "invalidArraySize:\n"
                        "    mov rax, 1\n"
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

            m_output << "    mov rax, 60\n"
                        "    mov rdi, 1\n"
                        "    syscall\n";


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

            // r8 = String Address
            // Overwrites: rax, rbx, rdx, rdi, rsi, r12
            // Returns: Nothing
            m_output << "\nprintString:\n"
                        "    mov rbx, [r8]\n"
                        "    mov r12, " << VAR_SIZE << "\n"
                        "    cmp rbx, 0\n"
                        "    jle donePrinting\n"
                        "    printStringLoop:\n"
                        "        lea rsi, [r8 + r12]\n"
                        "        mov rax, 1\n"
                        "        mov rdi, 1\n"
                        "        mov rdx, 1\n"
                        "        syscall\n"
                        "        sub rbx, 1\n"
                        "        add r12, " << VAR_SIZE << "\n"
                        "        cmp rbx, 0\n"
                        "        jg printStringLoop\n"
                        "    donePrinting:\n"
                        "        mov rax, 1\n"
                        "        mov rdi, 1\n"
                        "        mov rsi, newLine\n"
                        "        mov rdx, 1\n"
                        "        syscall\n"
                        "        ret\n";

            // rax = Boolean Value
            // Overwrites: None
            // Returns: rax = Bounded Boolean Value
            m_output << "\nboundBool:\n"
                        "    cmp rax, 0\n"
                        "    setne al\n"
                        "    movzx rax, al\n"
                        "    ret\n";

            // rsi = Memory Size (bytes)
            // Overwrites: rax, rdx, rdi, r8, r9, r10
            // Returns: rax = Memory Address
            m_output << "\nallocateMemory:\n"
                        "    mov rdi, 0\n"
                        "    mov rdx, 0x3\n"
                        "    mov r10, 0x22\n"
                        "    mov r8, -1\n"
                        "    mov r9, 0\n"
                        "    mov rax, 9\n"
                        "    syscall\n"
                        "    ret\n";

            // r12 = Old Address, rax = New Address
            // Overwrites: rbx, rcx, rdx
            // Returns: Nothing
            m_output << "\nfillMemory:\n"
                        "    mov rbx, [r12]\n"
                        "    mov rcx, " << VAR_SIZE << "\n"
                        "\n"
                        "    fillLoop:\n"
                        "        cmp rbx, 0\n"
                        "        jle doneFilling\n"
                        "        mov rdx, [r12 + rcx]\n"
                        "        mov [rax + rcx], rdx\n"
                        "        sub rbx, 1\n"
                        "        add rcx, " << VAR_SIZE << "\n"
                        "        jmp fillLoop\n"
                        "    doneFilling:\n"
                        "        ret\n";

            // r14 = Left String Address, r15 = Right String Address
            // Overwrites: rax, rbx, rcx, r8, r9, r12, r13
            // Returns: rax = Boolean Result
            m_output << "\ncmpStringEq:\n"
                        "    mov r8, [r14]\n"
                        "    mov r9, [r15]\n"
                        "    cmp r8, r9\n"
                        "    jne isNotEqual\n"
                        "    mov rbx, r8\n"
                        "    mov rcx, " << VAR_SIZE << "\n"
                        "    cmpEqLoop:\n"
                        "        cmp rbx, 0\n"
                        "        jle isEqual\n"
                        "        mov r12, [r14 + rcx]\n"
                        "        mov r13, [r15 + rcx]\n"
                        "        cmp r12, r13\n"
                        "        jne isNotEqual\n"
                        "        sub rbx, 1\n"
                        "        add rcx, " << VAR_SIZE << "\n"
                        "        jmp cmpEqLoop\n"
                        "    isEqual:\n"
                        "        mov rax, 1\n"
                        "        ret\n"
                        "    isNotEqual:\n"
                        "        mov rax, 0\n"
                        "        ret\n";

            // r14 = Left String Address, r15 = Right String Address
            // Overwrites: rax, rbx, rcx, r8, r9, r12, r13
            // Returns: rax = Boolean Result
            m_output << "\ncmpStringGt:\n"
                        "    mov r8, [r14]\n"
                        "    mov r9, [r15]\n"
                        "    mov rbx, 0\n"
                        "    mov rcx, " << VAR_SIZE << "\n"
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
                        "        add rcx, " << VAR_SIZE << "\n"
                        "        jmp cmpGtLoop\n"
                        "    greater:\n"
                        "        mov rax, 1\n"
                        "        ret\n"
                        "    lessEqual:\n"
                        "        mov rax, 0\n"
                        "        ret\n";

            // r14 = Left String Address, r15 = Right String Address
            // Overwrites: rax, rbx, rcx, r8, r9, r12, r13
            // Returns: rax = Boolean Result
            m_output << "\ncmpStringLt:\n"
                        "    mov r8, [r14]\n"
                        "    mov r9, [r15]\n"
                        "    mov rbx, 0\n"
                        "    mov rcx, " << VAR_SIZE << "\n"
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
                        "        add rcx, " << VAR_SIZE << "\n"
                        "        jmp cmpLtLoop\n"
                        "    less:\n"
                        "        mov rax, 1\n"
                        "        ret\n"
                        "    greaterEqual:\n"
                        "        mov rax, 0\n"
                        "        ret\n";

            // r11 = String Address
            // Overwrites: rax, rbx, rcx, rdx, r8, r10, r12, r13, r14
            // Returns: r8 = Integer Value
            m_output << "\nstringToInt:\n"
                        "    mov r12, [r11]\n"
                        "    mov r13, " << VAR_SIZE << "\n"
                        "    mov r14, 10\n"
                        "    cmp r12, 0\n"
                        "    je stoiEmptyString\n"
                        "    mov rdx, [r11 + " << VAR_SIZE << "]\n"
                        "    cmp rdx, " << ASCII_MINUS << "\n"
                        "    jne stoiIsPositive\n"
                        "    sub r12, 1\n"
                        "    add r13, " << VAR_SIZE << "\n"
                        "    stoiIsPositive:\n"
                        "    mov r8, 0\n"
                        "    mov rax, 1\n"
                        "    mov rbx, r12\n"
                        "    mov rcx, r13\n"
                        "    stoiCheckLoop:\n"
                        "        cmp rbx, 0\n"
                        "        jle stoiDoneChecking\n"
                        "        mov rdx, [r11 + rcx]\n"
                        "        cmp rdx, " << ASCII_ZERO << "\n"
                        "        jl stoiInvalidString\n"
                        "        cmp rdx, " << ASCII_NINE << "\n"
                        "        jg stoiInvalidString\n"
                        "        sub rbx, 1\n"
                        "        add rcx, " << VAR_SIZE << "\n"
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
                        "        sub r10, " << ASCII_ZERO << "\n"
                        "        imul r10, rax\n"
                        "        add r8, r10\n"
                        "        cqo\n"
                        "        idiv r14\n"
                        "        sub rbx, 1\n"
                        "        add rcx, " << VAR_SIZE << "\n"
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

            // r12 = Integer Value
            // Overwrites: rax, rbx, rcx, rdx, rsi, r14
            // Returns: rax = String Address
            m_output << "\nintToString:\n"
                        "    mov rax, r12\n"
                        "    mov rbx, 0\n"
                        "    mov rcx, 10\n"
                        "    mov r15, 0\n"
                        "    cmp rax, 0\n"
                        "    jne intToStringNotZero\n"
                        "    mov rdx, " << ASCII_ZERO << "\n"
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
                        "        add rdx, " << ASCII_ZERO << "\n"
                        "        push rdx\n"
                        "        add rbx, 1\n"
                        "        jmp intToStringConvertLoop\n"
                        "    intToStringDoneConverting:\n"
                        "        cmp r15, 0\n"
                        "        je intToStringDontAddNegative\n"
                        "        mov rdx, " << ASCII_MINUS << "\n"
                        "        push rdx\n"
                        "        add rbx, 1\n"
                        "        intToStringDontAddNegative:\n"
                        "        mov rsi, rbx\n"
                        "        add rsi, 1\n"
                        "        imul rsi, " << VAR_SIZE << "\n"
                        "        call allocateMemory\n"
                        "        mov rcx, " << VAR_SIZE << "\n"
                        "        mov [rax], rbx\n"
                        "    intToStringFillString:\n"
                        "        cmp rbx, 0\n"
                        "        jle intToStringDoneFilling\n"
                        "        pop rdx\n"
                        "        mov [rax + rcx], rdx\n"
                        "        sub rbx, 1\n"
                        "        add rcx, " << VAR_SIZE << "\n"
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
            if(type == DataType::Boolean) {
                m_output << "    call boundBool\n";
            }
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

        static constexpr int VAR_SIZE = 8;
        static constexpr int ASCII_ZERO = 48;
        static constexpr int ASCII_NINE = 57;
        static constexpr int ASCII_MINUS = 45;
};