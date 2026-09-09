#include "generator.hpp"

namespace Generator {
    Generator::Generator(Parser::ProgramNode program)
        : m_program(std::move(program)) {}

    std::string Generator::generateProgram() {
        m_output << "DEFAULT REL\n";

        generateData();
        generateEntryPoint();

        generateProgramStatements();
        generateProgramExit();

        generateErrorHandlers();

        generateUserFunctions();

        generateUtil();
        generateInputFunctions();
        generateStringFunctions();
        generateTypeConversionFunctions();

        return m_output.str();
    }

    void Generator::generateProgramStatements() {
        for(Parser::StatementNode* statement : m_program.statements) {
            generateStatement(statement, m_variables);
        }
    }

    void Generator::generateStatement(Parser::StatementNode* statement, VarMap &variables) {
        struct StatementVisitor {
            Generator* generator;
            VarMap& variables;
            Parser::StatementNode* statement;

            void operator()(const Parser::ExitNode* exitNode) const {
                generator->generateExit(exitNode, variables);
            }
            void operator()(const Parser::PrintNode* printNode) const {
                generator->generatePrint(printNode, variables);
            }
            void operator()(const Parser::DeclarationNode* declarationNode) const {
                generator->generateVariableDeclaration(declarationNode, variables, statement->lineNumber);
            }
            void operator()(const Parser::ReAssignmentNode* reAssignmentNode) const {
                generator->generateReAssignment(reAssignmentNode, variables, statement->lineNumber);
            }
            void operator()(const Parser::ScopeNode* scopeNode) const {
                generator->generateScopeStatement(scopeNode, variables);
            }
            void operator()(const Parser::ConditionalNode* conditionalNode) const {
                generator->generateConditional(conditionalNode, variables);
            }
            void operator()(const Parser::LoopNode* loopNode) const {
                generator->generateLoop(loopNode, variables);
            }
            void operator()(Parser::FunctionNode* functionNode) const {
                generator->generateFunctionDeclaration(functionNode);
            }
            void operator()(const Parser::ReturnNode* returnNode) const {
                generator->generateReturn(returnNode, variables);
            }
            void operator()(const Parser::FunctionCall* functionCall) const {
                generator->generateFunctionCall(functionCall, variables);
            }
        };

        StatementVisitor visitor {.generator = this, .variables = variables, .statement = statement};
        std::visit(visitor, statement->variant);
    }

    void Generator::generateExpression(const Parser::ExpressionNode *expression, VarMap &variables) {
        struct ExpressionVisitor {
            Generator* generator;
            const Parser::ExpressionNode* expression;
            VarMap& variables;

            void operator()(Parser::TermExpressionNode* termExpression) const {
                generator->generateTermExpression(termExpression, variables);
            }
            void operator()(const Parser::OperationExpressionNode* operationExpression) const {
                generator->generateOperationExpression(operationExpression, variables, expression->type);
            }
        };

        ExpressionVisitor visitor {.generator = this, .expression = expression, .variables = variables};
        std::visit(visitor, expression->variant);
    }

    void Generator::generateTerm(Parser::TermExpressionNode *term, VarMap &variables) {
        struct TermVisitor {
            Generator* generator;
            VarMap& variables;
            Parser::TermExpressionNode* term;

            void operator()(const Parser::LiteralTerm* literalTerm) const {
                generator->generateLiteral(literalTerm);
            }
            void operator()(const Parser::IdentifierTerm* identifierTerm) const {
                generator->generateIdentifier(identifierTerm, variables, term->lineNumber);
            }
            void operator()(const Parser::IndexedTerm* indexedTerm) const {
                generator->generateIndexedTerm(indexedTerm, variables, term->lineNumber);
            }
            void operator()(const Parser::PropertyTerm* propertyTerm) const {
                generator->generateProperty(propertyTerm, variables, term->lineNumber);
            }
            void operator()(const Parser::TypeCastTerm* typeCastTerm) const {
                generator->generateTypeCast(typeCastTerm, variables);
            }
            void operator()(const Parser::InputTerm* inputTerm) const {
                generator->generateReadInput(inputTerm, variables);
            }
            void operator()(const Parser::ParenthesisTerm* parenthesisTerm) const {
                generator->generateParenthesisTerm(parenthesisTerm, variables);
            }
            void operator()(const Parser::FunctionCall* functionCall) const {
                generator->generateFunctionCall(functionCall, variables);
                generator->push("rax");
            }
            void operator()(const Parser::UnaryExpressionTerm* unaryTerm) const {
                generator->generateUnaryExpression(unaryTerm, variables);
            }
        };

        TermVisitor visitor {.generator = this, .variables = variables, .term = term};
        std::visit(visitor, term->variant);
    }

    void Generator::generateExit(const Parser::ExitNode* exitNode, VarMap &variables) {
        generateExpression(exitNode->expression, variables);

        m_output << "    mov rax, 60\n";
        pop("rdi");

        m_output << "    syscall\n";
    }

    void Generator::generatePrint(const Parser::PrintNode *printNode, VarMap &variables) {
        generateExpression(printNode->expression, variables);

        switch(printNode->expression->type) {
            case DataType::Integer:
            case DataType::Boolean:
                pop("r12");
                m_output << "    call intToString\n"
                            "    mov r8, rax\n"
                            "    call printString\n";
                break;
            case DataType::Character:
                pop("r12");
                m_output << "    mov rsi, 16\n"
                            "    call allocateMemory\n"
                            "    mov [rax], 1\n"
                            "    mov [rax + " << VAR_SIZE << "], r12\n"
                            "    mov r8, rax\n"
                            "    call printString\n";
                break;
            case DataType::String:
                pop("r8");
                m_output << "    call printString\n";
                break;
        }

        m_output << "    call printNewLine\n";
    }

    void Generator::generateVariableDeclaration(const Parser::DeclarationNode *declarationNode, VarMap &variables, int lineNumber) {
        if(variables.find(declarationNode->identifier.value.value()) != variables.end())
            throwError(lineNumber, "Error: Redefinition of variable '" + declarationNode->identifier.value.value() + "'");

        generateExpression(declarationNode->expression, variables);

        if(getGroupType(declarationNode->type) == GroupType::Primitive) {
            pop("rax");

            if(declarationNode->type == DataType::Boolean) m_output << "    call boundBool\n";
        }
        else if(getGroupType(declarationNode->type) == GroupType::Strings) {
            pop("r12");

            m_output << "    mov rsi, [r12]\n"
                        "    add rsi, 1\n"
                        "    imul rsi, " << VAR_SIZE << "\n"
                        "    call allocateMemory\n";

            m_output << "    mov rdx, [r12]\n"
                        "    mov [rax], rdx\n"
                        "    call fillMemory\n";
        }
        else if(getGroupType(declarationNode->type) == GroupType::Arrays) {
            pop("rsi");

            m_output << "    cmp rsi, 0\n"
                        "    jle invalidArraySize\n";

            m_output << "    mov r12, rsi\n";

            m_output << "    add rsi, 1\n"
                        "    imul rsi, " << VAR_SIZE << "\n";

            m_output << "    call allocateMemory\n";

            m_output << "    mov [rax], r12\n";
        }

        push("rax");
        variables.insert({declarationNode->identifier.value.value(),
                          Variable {.scopeDepth = m_scopeDepth, .stackLocation = m_stackSize, .type = declarationNode->type}});
    }

    void Generator::generateReAssignment(const Parser::ReAssignmentNode *reAssignmentNode, VarMap &variables, int lineNumber) {
        if(variables.find(reAssignmentNode->identifier.value.value()) == variables.end())
            throwError(lineNumber, "Error: Use of undeclared variable '" + reAssignmentNode->identifier.value.value() + "'");

        int stackLocation = variables[reAssignmentNode->identifier.value.value()].stackLocation;
        generateExpression(reAssignmentNode->expression, variables);

        if(reAssignmentNode->index.has_value()) {
            generateExpression(reAssignmentNode->index.value(), variables);

            m_output << "    mov rbx, [rsp + " << std::to_string((m_stackSize - stackLocation) * VAR_SIZE) << "]\n"
                        "    mov r8, [rbx]\n";

            pop("r10");
            pop("rax");
            if(getPrimitiveVariant(variables[reAssignmentNode->identifier.value.value()].type) == DataType::Boolean)
                m_output << "    call boundBool\n";

            m_output << "    cmp r10, r8\n"
                        "    jge outOfBounds\n"
                        "    cmp r10, 0\n"
                        "    jl outOfBounds\n";

            m_output << "    add r10, 1\n"
                        "    imul r10, " << VAR_SIZE << "\n";

            m_output << "    mov rbx, [rsp + " << std::to_string((m_stackSize - stackLocation) * VAR_SIZE) << "]\n"
                        "    mov [rbx + r10], rax\n";
        }
        else {
            pop("rax");
            if(variables[reAssignmentNode->identifier.value.value()].type == DataType::Boolean)
                m_output << "    call boundBool\n";

            m_output << "    mov [rsp + " << std::to_string((m_stackSize - stackLocation) * VAR_SIZE) << "], rax\n";
        }
    }

    void Generator::generateScopeStatement(const Parser::ScopeNode *scopeNode, Generator::VarMap &variables) {
        generateScope(scopeNode->statements, variables);
    }

    void Generator::generateConditional(const Parser::ConditionalNode *conditionalNode, Generator::VarMap &variables) {
        int endLabel = m_curLabelCount++;

        for (int i = 0; i < conditionalNode->statements.size(); ++i) {
            // If an Else statement is present
            if(i == conditionalNode->conditions.size()) {
                generateScope(conditionalNode->statements[i], variables);
            }
            else {
                int curLabel = m_curLabelCount++;

                generateExpression(conditionalNode->conditions[i], variables);
                pop("rax");

                m_output << "    cmp rax, 0\n"
                            "    je b" << curLabel << "\n";

                generateScope(conditionalNode->statements[i], variables);

                m_output << "    jmp b" << endLabel << "\n";

                m_output << "b" << curLabel << ":\n";
            }
        }

        m_output << "b" << endLabel << ":\n";
    }

    void Generator::generateLoop(const Parser::LoopNode *loopNode, Generator::VarMap &variables) {
        int statementLabel = m_curLabelCount++;
        int conditionLabel = m_curLabelCount++;

        m_output << "    jmp b" << conditionLabel << "\n";

        m_output << "b" << statementLabel << ":\n";
        generateScope(loopNode->statements, variables);

        m_output << "b" << conditionLabel << ":\n";
        generateExpression(loopNode->condition, variables);

        pop("rax");
        m_output << "    cmp rax, 0\n"
                    "    jne b" << statementLabel << "\n";
    }

    void Generator::generateFunctionDeclaration(Parser::FunctionNode* functionNode) {
        m_functionNodes.push_back(functionNode);
    }

    void Generator::generateReturn(const Parser::ReturnNode *returnNode, Generator::VarMap &variables) {
        generateExpression(returnNode->expression.value(), variables);
        pop("rcx");

        m_output << "    mov rbx, [stackPointerIndex]\n"
                    "    sub rbx, 1\n"
                    "    mov rsp, [stackPointers + rbx * " << VAR_SIZE << "]\n"
                    "    mov [stackPointerIndex], rbx\n";

        m_output << "    mov rax, rcx\n"
                    "    ret\n";
    }

    void Generator::generateFunctionCall(const Parser::FunctionCall* functionCall, Generator::VarMap &variables) {
        int curOffset = 0;

        m_output << "    mov rsi, " << functionCall->arguments.size() * VAR_SIZE << "\n"
                                                                                    "    call allocateMemory\n"
                                                                                    "    mov rbp, rax\n";

        for (auto parameter : functionCall->arguments) {
            generateExpression(parameter.expression, variables);
            pop("rax");

            m_output << "    mov [rbp + " << curOffset << "], rax\n";
            curOffset += VAR_SIZE;
        }

        m_output << "    mov rax, [stackPointerIndex]\n"
                    "    mov [stackPointers + rax * " << VAR_SIZE << "], rsp\n"
                                                                     "    sub [stackPointers + rax * " << VAR_SIZE << "], " << VAR_SIZE << "\n"
                                                                                                                                           "    add rax, 1\n"
                                                                                                                                           "    mov [stackPointerIndex], rax\n";

        m_output << "    call func_" << functionCall->identifier.value.value() << "\n";
    }

    void Generator::generateScope(std::vector<Parser::StatementNode *> statements, Generator::VarMap &variables) {
        int curStackSize = m_stackSize;
        int curScopeDepth = m_scopeDepth++;

        for(Parser::StatementNode* statement : statements) {
            generateStatement(statement, variables);
        }

        while(m_stackSize > curStackSize) {
            pop("rax");
        }

        for(auto itr = variables.begin(); itr != variables.end();) {
            if(itr->second.scopeDepth > curScopeDepth) itr = variables.erase(itr);
            else itr++;
        }

        m_scopeDepth--;
    }

    void Generator::generateTermExpression(Parser::TermExpressionNode *termExpression, Generator::VarMap &variables) {
        generateTerm(termExpression, variables);
    }

    void Generator::generateOperationExpression(const Parser::OperationExpressionNode *operationExpression, Generator::VarMap &variables, DataType resultantType) {
        generateExpression(operationExpression->left, variables);
        generateExpression(operationExpression->right, variables);

        switch(getGroupType(operationExpression->left->type)) {
            case GroupType::Primitive:
                pop("rbx");
                pop("rax");
                break;
            case GroupType::Strings:
                pop("r15");
                pop("r14");
                break;
        }

        generateOperation(getGroupType(operationExpression->left->type), operationExpression->_operator, resultantType);
        push("rax");
    }

    void Generator::generateLiteral(const Parser::LiteralTerm *literalTerm) {
        if(literalTerm->literal.type == TokenType::stringLiteral) {
            int length = literalTerm->literal.value.value().length();

            m_output << "    mov rsi, " << (length + 1) * VAR_SIZE << "\n"
                        "    call allocateMemory\n";

            m_output << "    mov [rax], " << length << "\n";
            for (int i = 0; i < length; ++i) {
                m_output << "    mov [rax + " << (i + 1) * VAR_SIZE << "], " << static_cast<int>(literalTerm->literal.value.value()[i]) << "\n";
            }
        }
        else {
            m_output << "    mov rax, " << literalTerm->literal.value.value() << "\n";
        }

        push("rax");
    }

    void Generator::generateIdentifier(const Parser::IdentifierTerm *identifierTerm, Generator::VarMap &variables, int lineNumber) {
        if(variables.find(identifierTerm->identifier.value.value()) == variables.end())
            throwError(lineNumber, "Error: Use of undeclared variable '" + identifierTerm->identifier.value.value() + "'");

        int stackLocation = variables[identifierTerm->identifier.value.value()].stackLocation;
        push("[rsp + " + std::to_string((m_stackSize - stackLocation) * VAR_SIZE) + "]");
    }

    void Generator::generateIndexedTerm(const Parser::IndexedTerm *indexedTerm, Generator::VarMap &variables, int lineNumber) {
        if(variables.find(indexedTerm->identifier.value.value()) == variables.end())
            throwError(lineNumber, "Error: Use of undeclared variable '" + indexedTerm->identifier.value.value() + "'");

        int stackLocation = variables[indexedTerm->identifier.value.value()].stackLocation;

        generateExpression(indexedTerm->index, variables);
        pop("rcx");


        m_output << "    mov rbx, [rsp + " << std::to_string((m_stackSize - stackLocation) * VAR_SIZE) << "]\n"
                    "    mov r8, [rbx]\n";

        m_output << "    cmp rcx, r8\n"
                    "    jge outOfBounds\n"
                    "    cmp rcx, 0\n"
                    "    jl outOfBounds\n";

        m_output << "    add rcx, 1\n"
                    "    imul rcx, " << VAR_SIZE << "\n";

        m_output << "    mov rax, [rbx + rcx]\n";
        push("rax");
    }

    void Generator::generateProperty(const Parser::PropertyTerm *propertyTerm, Generator::VarMap &variables, int lineNumber) {
        if(variables.find(propertyTerm->identifier.value.value()) == variables.end())
            throwError(lineNumber, "Error: Use of undeclared variable '" + propertyTerm->identifier.value.value() + "'");

        if(propertyTerm->property == TokenType::size || propertyTerm->property == TokenType::length) {
            int stackLocation = variables[propertyTerm->identifier.value.value()].stackLocation;
            m_output << "    mov rbx, [rsp + " << std::to_string((m_stackSize - stackLocation) * VAR_SIZE) << "]\n";

            m_output << "    mov rax, [rbx]\n";
            push("rax");
        }
    }

    void Generator::generateTypeCast(const Parser::TypeCastTerm *typeCastTerm, Generator::VarMap &variables) {
        generateExpression(typeCastTerm->expression, variables);

        switch(typeCastTerm->typeCast) {
            case TokenType::stoi:
                pop("r11");
                m_output << "    call stringToInt\n";
                push("r8");
                break;
            case TokenType::toString:
                pop("r12");
                switch(typeCastTerm->expression->type) {
                    case DataType::Integer:
                        m_output << "    call intToString\n";
                        break;
                    case DataType::Boolean:
                        m_output << "    mov rsi, 16\n"
                                    "    call allocateMemory\n"
                                    "    mov [rax], 1\n"
                                    "    add r12, " << ASCII_ZERO << "\n"
                                    "    mov [rax + " << VAR_SIZE << "], r12\n";
                        break;
                    case DataType::Character:
                        m_output << "    mov rsi, 16\n"
                                    "    call allocateMemory\n"
                                    "    mov [rax], 1\n"
                                    "    mov [rax + " << VAR_SIZE << "], r12\n";
                        break;
                }
                push("rax");
                break;
            case TokenType::boolType:
                pop("rax");
                m_output << "    call boundBool\n";
                push("rax");
                break;
            case TokenType::intType:
            case TokenType::charType:
                break;
        }
    }

    void Generator::generateReadInput(const Parser::InputTerm *inputTerm, Generator::VarMap &variables) {
        if(inputTerm->inputMessage.has_value()) {
            generateExpression(inputTerm->inputMessage.value(), variables);
            pop("r8");
            m_output << "    call printString\n";
        }

        switch(inputTerm->readType) {
            case TokenType::readInt:
                m_output << "    call readInt\n";
                break;
            case TokenType::readBool:
                m_output << "    call readInt\n"
                            "    call boundBool\n";
                break;
            case TokenType::readChar:
                m_output << "    call readChar\n";
                break;
            case TokenType::readNext:
                m_output << "    call readNext\n";
                break;
            case TokenType::readLine:
                m_output << "    call readLine\n";
                break;
        }

        push("rax");
    }

    void Generator::generateParenthesisTerm(const Parser::ParenthesisTerm *parenthesisTerm, Generator::VarMap &variables) {
        generateExpression(parenthesisTerm->expression, variables);
    }

    void Generator::generateUnaryExpression(const Parser::UnaryExpressionTerm *unaryTerm, Generator::VarMap &variables) {
        generateExpression(unaryTerm->expression, variables);
        pop("rax");

        switch(unaryTerm->_operator) {
            case TokenType::subtraction:
                m_output << "    imul rax, -1\n";
                break;
            case TokenType::notOperator:
                m_output << "    cmp rax, 0\n"
                            "    sete al\n"
                            "    movzx rax, al\n";
                break;
        }

        push("rax");
    }

    void Generator::generateOperation(GroupType groupType, TokenType _operator, DataType resultantType) {
        switch(groupType) {
            case GroupType::Primitive:
                switch (_operator) {
                    case (TokenType::addition):
                        m_output << "    add rax, rbx\n";
                        break;
                    case (TokenType::subtraction):
                        m_output << "    sub rax, rbx\n";
                        break;
                    case (TokenType::multiplication):
                        m_output << "    cqo\n"
                                    "    imul rax, rbx\n";
                        break;
                    case (TokenType::division):
                        m_output << "    cqo\n"
                                    "    idiv rbx\n";
                        break;
                    case (TokenType::modulo):
                        m_output << "    cqo\n"
                                    "    idiv rbx\n"
                                    "    mov rax, rdx\n";
                        break;
                    case (TokenType::equalTo):
                        m_output << "    cmp rax, rbx\n"
                                    "    sete al\n"
                                    "    movzx rax, al\n";
                        break;
                    case (TokenType::notEqualTo):
                        m_output << "    cmp rax, rbx\n"
                                    "    setne al\n"
                                    "    movzx rax, al\n";
                        break;
                    case (TokenType::lessThan):
                        m_output << "    cmp rax, rbx\n"
                                    "    setl al\n"
                                    "    movzx rax, al\n";
                        break;
                    case (TokenType::lessThanOrEqual):
                        m_output << "    cmp rax, rbx\n"
                                    "    setle al\n"
                                    "    movzx rax, al\n";
                        break;
                    case (TokenType::greaterThan):
                        m_output << "    cmp rax, rbx\n"
                                    "    setg al\n"
                                    "    movzx rax, al\n";
                        break;
                    case (TokenType::greaterThanOrEqual):
                        m_output << "    cmp rax, rbx\n"
                                    "    setge al\n"
                                    "    movzx rax, al\n";
                        break;
                    case (TokenType::andOperator):
                        m_output << "    cmp rax, 0\n"
                                    "    setne al\n"
                                    "    movzx rax, al\n";

                        m_output << "    cmp rbx, 0\n"
                                    "    setne bl\n"
                                    "    movzx rbx, bl\n";

                        m_output << "    add rax, rbx\n";

                        m_output << "    cmp rax, 2\n"
                                    "    setge al\n"
                                    "    movzx rax, al\n";

                        break;
                    case (TokenType::orOperator):
                        m_output << "    cmp rax, 0\n"
                                    "    setne al\n"
                                    "    movzx rax, al\n";

                        m_output << "    cmp rbx, 0\n"
                                    "    setne bl\n"
                                    "    movzx rbx, bl\n";

                        m_output << "    add rax, rbx\n";

                        m_output << "    cmp rax, 1\n"
                                    "    setge al\n"
                                    "    movzx rax, al\n";
                        break;
                }
                break;
            case GroupType::Strings:
                switch (_operator) {
                    case TokenType::addition:
                        m_output << "    mov rbx, [r14]\n"
                                    "    mov r13, [r15]\n"
                                    "    add r13, rbx\n";

                        m_output << "    mov rsi, r13\n"
                                    "    add rsi, 1\n"
                                    "    imul rsi, " << VAR_SIZE << "\n"
                                                                    "    call allocateMemory\n";

                        m_output << "    mov [rax], r13\n";

                        m_output << "    mov r12, r14\n"
                                    "    call fillMemory\n";

                        m_output << "    mov r8, rax\n"
                                    "    mov rbx, [r12]\n"
                                    "    imul rbx, " << VAR_SIZE << "\n"
                                                                    "    add rax, rbx\n"
                                                                    "    mov r12, r15\n"
                                                                    "    call fillMemory\n"
                                                                    "    mov rax, r8\n";
                        break;
                    case TokenType::equalTo:
                        m_output << "    call cmpStringEq\n";
                        break;
                    case TokenType::notEqualTo:
                        m_output << "    call cmpStringEq\n"
                                    "    xor rax, 1\n";
                        break;
                    case TokenType::greaterThan:
                        m_output << "    call cmpStringGt\n";
                        break;
                    case TokenType::lessThanOrEqual:
                        m_output << "    call cmpStringGt\n"
                                    "    xor rax, 1\n";
                        break;
                    case TokenType::lessThan:
                        m_output << "    call cmpStringLt\n";
                        break;
                    case TokenType::greaterThanOrEqual:
                        m_output << "    call cmpStringLt\n"
                                    "    xor rax, 1\n";
                        break;
                }
                break;
        }

        if(resultantType == DataType::Boolean) m_output << "    call boundBool\n";
    }

    void Generator::generateData() {
        m_output << "section .bss\n"
                    "    readBuffer resb 1\n"
                    "    unReadFlag resb 1\n"
                    "    stackPointers resq 256\n"
                    "    stackPointerIndex resb 1\n";

        m_output << "\nsection .data\n"
                    "    exitMsg db \"Process finished with exit code \"\n"
                    "    oobMsg db \"Error: Array index out of bounds\", " << ASCII_NEWLINE << "\n"
                    "    negSizeArray db \"Error: Array size must be greater than zero\", " << ASCII_NEWLINE << "\n"
                    "    invalidStoiArg db \"Error: Invalid integer string: string must contain only digits (0-9), with an optional leading '-'\", " << ASCII_NEWLINE << "\n"
                    "    endOfFileMsg db \"Error: Unexpected end of input while reading\", " << ASCII_NEWLINE << "\n"
                    "    missingReturnMsg db \"Error: Missing return statement\", " << ASCII_NEWLINE << "\n"
                    "    newLine db 10\n";
    }

    void Generator::generateEntryPoint() {
        m_output << "section .text\n"
                    "global _start\n"
                    "_start:\n"
                    "    mov [unReadFlag], 0\n";
    }

    void Generator::generateProgramExit() {
        m_output << "    mov rax, 1\n"
                    "    mov rdi, 1\n"
                    "    mov rsi, exitMsg\n"
                    "    mov rdx, 32\n"
                    "    syscall\n";

        m_output << "    mov rax, 60\n"
                    "    mov rdi, 0\n"
                    "    syscall\n";
    }

    void Generator::generateErrorHandlers() {
        m_output << "outOfBounds:\n"
                    "    mov rax, 1\n"
                    "    mov rdi, 1\n"
                    "    mov rsi, oobMsg\n"
                    "    mov rdx, 33\n"
                    "    syscall\n"
                    "    call exitError\n";

        m_output << "invalidArraySize:\n"
                    "    mov rax, 1\n"
                    "    mov rdi, 1\n"
                    "    mov rsi, negSizeArray\n"
                    "    mov rdx, 44\n"
                    "    syscall\n"
                    "    call exitError\n";

        m_output << "missingReturn:\n"
                    "    mov rax, 1\n"
                    "    mov rdi, 1\n"
                    "    mov rsi, missingReturnMsg\n"
                    "    mov rdx, 32\n"
                    "    syscall\n"
                    "    call exitError\n";
    }

    void Generator::generateUserFunctions() {
        m_output << "\n\n\n ; User Functions \n\n\n";

        for(auto functionNode : m_functionNodes) {
            int curOffset = 0;
            auto variables = m_functions[functionNode->identifier.value.value()].variables;

            m_scopeDepth = 0;
            m_stackSize = 1;

            m_output << "\nfunc_" << functionNode->identifier.value.value() << ":\n";

            for(auto argument : functionNode->arguments) {
                m_output << "    mov rax, [rbp + " << curOffset << "]\n";
                push("rax");

                variables.insert({argument.identifier.value.value(),
                                  Variable {.scopeDepth = m_scopeDepth, .stackLocation = m_stackSize, .type = argument.dataType}});
                curOffset += VAR_SIZE;
            }

            generateScope(functionNode->statements, variables);

            if(functionNode->returnType == DataType::Void) m_output << "    ret\n";
            else m_output << "    jmp missingReturn\n";
        }
    }

    void Generator::generateUtil() {
        m_output << "\n\n\n ; Util Functions \n\n\n";

        m_output << "\nexitError:\n"
                    "    mov rax, 1\n"
                    "    mov rdi, 1\n"
                    "    mov rsi, exitMsg\n"
                    "    mov rdx, 32\n"
                    "    syscall\n"
                    "    mov rax, 60\n"
                    "    mov rdi, 1\n"
                    "    syscall\n";

        // Fixes boolean term to between 0 and 1
        // rax = Boolean Value
        // Overwrites: None
        // Returns: rax = Bounded Boolean Value
        m_output << "\nboundBool:\n"
                    "    cmp rax, 0\n"
                    "    setne al\n"
                    "    movzx rax, al\n"
                    "    ret\n";

        // Checks if rax contains the ascii value of a whitespace character
        // rax = Character Value
        // Overwrites: rbx
        // Returns: rbx = Boolean Result
        m_output << "\nisWhiteSpace:\n"
                    "    cmp rax, " << ASCII_TAB << "\n"
                    "    je whiteSpaceFound\n"
                    "    cmp rax, " << ASCII_NEWLINE << "\n"
                    "    je whiteSpaceFound\n"
                    "    cmp rax, " << ASCII_VERTICAL_TAB << "\n"
                    "    je whiteSpaceFound\n"
                    "    cmp rax, " << ASCII_FORM_FEED << "\n"
                    "    je whiteSpaceFound\n"
                    "    cmp rax, " << ASCII_CARRIAGE_RETURN << "\n"
                    "    je whiteSpaceFound\n"
                    "    cmp rax, " << ASCII_SPACE << "\n"
                    "    je whiteSpaceFound\n"
                    "    mov rbx, 0\n"
                    "    ret\n"
                    "    whiteSpaceFound:\n"
                    "        mov rbx, 1\n"
                    "        ret\n";
    }

    void Generator::generateInputFunctions() {
        // Reads the next character entered
        // Overwrites: rax, rdx, rdi, rsi, unReadFlag, readBuffer
        // Returns: rax = Character Value
        m_output << "\nreadRawChar:\n"
                    "    cmp [unReadFlag], 1\n"
                    "    jne readRaw\n"
                    "    movzx rax, byte [readBuffer]\n"
                    "    mov [unReadFlag], 0\n"
                    "    ret\n"
                    "    readRaw:\n"
                    "        mov rax, 0\n"
                    "        mov rdi, 0\n"
                    "        mov rsi, readBuffer\n"
                    "        mov rdx, 1\n"
                    "        syscall\n"
                    "        cmp rax, 0\n"
                    "        je endOfFile\n"
                    "        movzx rax, byte [readBuffer]\n"
                    "        ret\n"
                    "    endOfFile:\n"
                    "        mov rax, 1\n"
                    "        mov rdi, 1\n"
                    "        mov rsi, endOfFileMsg\n"
                    "        mov rdx, 45\n"
                    "        syscall\n"
                    "        call exitError\n";

        // Reads the first non-whitespace character entered
        // Overwrites: rax, rbx, rdx, rdi, rsi, unReadFlag, readBuffer
        // Returns: rax = Character Value
        m_output << "\nreadChar:\n"
                    "    readCharLoop:\n"
                    "        call readRawChar\n"
                    "        call isWhiteSpace\n"
                    "        cmp rbx, 1\n"
                    "        je readCharLoop\n"
                    "    ret\n";

        // Reads the integer value entered
        // Overwrites: rax, rbx, rdx, rdi, rsi, r15, r14, rbx, r9, r10, unReadFlag, readBuffer
        // Returns: rax = Integer Value
        m_output << "\nreadInt:\n"
                    "    call readChar\n"
                    "    call unReadChar\n"
                    "    mov r15, 0\n"
                    "    mov r14, 0\n"
                    "    call readRawChar\n"
                    "    cmp rax, " << ASCII_MINUS << "\n"
                    "    jne readIntIsPositive\n"
                    "    mov r15, 1\n"
                    "    jmp readIntLoop\n"
                    "    readIntIsPositive:\n"
                    "        call unReadChar\n"
                    "    readIntLoop:\n"
                    "        call readRawChar\n"
                    "        cmp rax, " << ASCII_ZERO << "\n"
                    "        jl doneReadingInt\n"
                    "        cmp rax, " << ASCII_NINE << "\n"
                    "        jg doneReadingInt\n"
                    "        push rax\n"
                    "        add r14, 1\n"
                    "        jmp readIntLoop\n"
                    "    doneReadingInt:\n"
                    "        call isWhiteSpace\n"
                    "        cmp rbx, 1\n"
                    "        je setUpCalculations\n"
                    "        call unReadChar\n"
                    "    setUpCalculations:\n"
                    "        mov rax, 0\n"
                    "        mov r9, 1\n"
                    "    calculateInt:\n"
                    "        cmp r14, 0\n"
                    "        jle doneProcessingInt\n"
                    "        pop r10\n"
                    "        sub r10, " << ASCII_ZERO << "\n"
                    "        imul r10, r9\n"
                    "        add rax, r10\n"
                    "        imul r9, 10\n"
                    "        sub r14, 1\n"
                    "        jmp calculateInt\n"
                    "    doneProcessingInt:\n"
                    "        cmp r15, 1\n"
                    "        jne returnIntInput\n"
                    "        imul rax, -1\n"
                    "    returnIntInput:\n"
                    "        ret\n";

        // Reads characters until a whitespace character is met. Ignores leading whitespace characters
        // Overwrites: rax, rbx, rcx, rdx, rdi, rsi, r8, r9, r10, r12, unReadFlag, readBuffer
        // Returns: rax = String Address
        m_output << "\nreadNext:\n"
                    "    call readChar\n"
                    "    call unReadChar\n"
                    "    mov r12, 0\n"
                    "    readNextLoop:\n"
                    "        call readRawChar\n"
                    "        call isWhiteSpace\n"
                    "        cmp rbx, 1\n"
                    "        je doneReadingWord\n"
                    "        push rax\n"
                    "        add r12, 1\n"
                    "        jmp readNextLoop\n"
                    "    doneReadingWord:\n"
                    "        mov rsi, r12\n"
                    "        add rsi, 1\n"
                    "        imul rsi, 8\n"
                    "        call allocateMemory\n"
                    "        mov [rax], r12\n"
                    "        mov rcx, r12\n"
                    "        imul rcx, 8\n"
                    "        fillWordLoop:\n"
                    "            cmp rcx, 0\n"
                    "            jle doneProcessingWord\n"
                    "            pop rbx\n"
                    "            mov [rax + rcx], rbx\n"
                    "            sub rcx, 8\n"
                    "            jmp fillWordLoop\n"
                    "        doneProcessingWord:\n"
                    "            ret\n";

        // Reads characters until a new line character (\n) is met. Ignores leading new line characters
        // Overwrites: rax, rbx, rcx, rdx, rdi, rsi, r8, r9, r10, r12, unReadFlag, readBuffer
        // Returns: rax = String Address
        m_output << "\nreadLine:\n"
                    "    removeStartingNewLines:\n"
                    "        call readRawChar\n"
                    "        cmp rax, " << ASCII_NEWLINE << "\n"
                    "        je removeStartingNewLines\n"
                    "    call unReadChar\n"
                    "    mov r12, 0\n"
                    "    readLineLoop:\n"
                    "        call readRawChar\n"
                    "        cmp rax, " << ASCII_NEWLINE << "\n"
                    "        je doneReadingLine\n"
                    "        push rax\n"
                    "        add r12, 1\n"
                    "        jmp readLineLoop\n"
                    "    doneReadingLine:\n"
                    "        mov rsi, r12\n"
                    "        add rsi, 1\n"
                    "        imul rsi, 8\n"
                    "        call allocateMemory\n"
                    "        mov [rax], r12\n"
                    "        mov rcx, r12\n"
                    "        imul rcx, 8\n"
                    "        fillLineLoop:\n"
                    "            cmp rcx, 0\n"
                    "            jle doneProcessingLine\n"
                    "            pop rbx\n"
                    "            mov [rax + rcx], rbx\n"
                    "            sub rcx, 8\n"
                    "            jmp fillLineLoop\n"
                    "        doneProcessingLine:\n"
                    "            ret\n";

        // Sets the unReadFlag to true, so the next call for readRawChar returns the previously read character
        m_output << "\nunReadChar:\n"
                    "    mov [unReadFlag], 1\n"
                    "    ret\n";
    }

    void Generator::generateStringFunctions() {
        // Prints the contents of a string
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
                    "        ret\n";

        // Prints the new line character
        // Overwrites: rax, rdx, rdi, rsi
        // Returns: Nothing
        m_output << "\nprintNewLine:\n"
                    "    mov rax, 1\n"
                    "    mov rdi, 1\n"
                    "    mov rsi, newLine\n"
                    "    mov rdx, 1\n"
                    "    syscall\n"
                    "    ret\n";

        // Allocates memory using mmap. Used for any non-primitive data type
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

        // Fill a term with the contents of another. Only works for non-primitive data types
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

        // Compares the equality of 2 strings
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

        // Compares if the left string is greater than the right
        // r14 = Left String Address, r15 = Right String Address
        // Overwrites: rax, rbx, rcx, r8, r9, r12, r13
        // Returns: rax = Boolean Result
        m_output << "\ncmpStringGt:\n"
                    "    mov r8, [r14]\n"
                    "    mov r9, [r15]\n"
                    "    mov rbx, 0\n"
                    "    mov rcx, " << VAR_SIZE << "\n"
                    "    cmpGtLoop:\n"
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

        // Compares if the left string is less than the right
        // r14 = Left String Address, r15 = Right String Address
        // Overwrites: rax, rbx, rcx, r8, r9, r12, r13
        // Returns: rax = Boolean Result
        m_output << "\ncmpStringLt:\n"
                    "    mov r8, [r14]\n"
                    "    mov r9, [r15]\n"
                    "    mov rbx, 0\n"
                    "    mov rcx, " << VAR_SIZE << "\n"
                    "    cmpLtLoop:\n"
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

    }

    void Generator::generateTypeConversionFunctions() {
// Converts a string to an integer. This can then be used to convert strings to any of the other primitive data types
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
                    "        call exitError\n";

        // Converts an integer to a string. This can then be used to convert other primitive data types into strings
        // r12 = Integer Value
        // Overwrites: rax, rbx, rcx, rdx, rdi, rsi, r8, r9, r10, r15
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

    void Generator::push(const std::string &reg) {
        m_output << "    push " << reg << "\n";
        m_stackSize++;
    }

    void Generator::pop(const std::string &reg) {
        m_output << "    pop " << reg << "\n";
        m_stackSize--;
    }

    void Generator::throwError(int lineNumber, std::string message) {
        std::cerr << "Line " << lineNumber << ": " << message << std::endl;
        exit(EXIT_FAILURE);
    }
}