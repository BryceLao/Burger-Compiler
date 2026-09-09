#include "parser.hpp"

namespace Parser {
    Parser::Parser(std::vector<Token> tokens)
        : m_Tokens(std::move(tokens)),
          m_ArenaAllocator(1024 * 1024 * 4) {}

    ProgramNode Parser::parseProgram() {
        ProgramNode program;

        while(peek().has_value()) {
            if(auto statement = parseStatement(m_variables)) program.statements.push_back(statement.value());
            else throwError(peek().value().lineNumber, "Error: Couldn't parse statement");
        }

        return program;
    }

    std::optional<StatementNode*> Parser::parseStatement(VarMap& variables, std::string_view curFunction) {
        if(!peek().has_value()) return {};

        switch(peek().value().type) {
            case TokenType::exit: return parseExit(variables);
            case TokenType::print: return parsePrint(variables);
            case TokenType::set: return parseVariableDeclaration(variables);
            case TokenType::identifier: return parseReAssignment(variables);
            case TokenType::openCurlyBrace: return parseScopeStatement(variables, curFunction);
            case TokenType::ifStatement: return parseConditional(variables, curFunction);
            case TokenType::whileLoop: return parseLoop(variables, curFunction);
            case TokenType::define: return parseFunctionDeclaration(curFunction);
            case TokenType::returnCall: return parseReturn(variables, curFunction);
            case TokenType::call: return parseFunctionCallStatement(variables);
            default: throwError(peek().value().lineNumber, "Error: Couldn't parse statement");
        }
    }

    std::vector<StatementNode*> Parser::parseScope(VarMap& variables, std::string_view curFunction) {
        std::vector<StatementNode*> statements = {};

        while(peek().has_value() && !tryPeek(TokenType::closeCurlyBrace)) {
            statements.push_back(parseStatement(variables, curFunction).value());
        }

        expectCharacter(TokenType::closeCurlyBrace);

        return statements;
    }

    std::optional<ExpressionNode*> Parser::parseExpression(VarMap& variables, int minimumPrecedence) {
        std::optional<TermExpressionNode*> term = parseTerm(variables, minimumPrecedence);

        if(!term.has_value()) return {};

        ExpressionNode* left = m_ArenaAllocator.allocate<ExpressionNode>();
        left->variant = term.value();
        left->type = term.value()->type;
        left->lineNumber = lastLine();

        while(true) {
            if(!peek().has_value()) break;

            TokenType _operator = peek().value().type;

            std::optional<int> precedenceLevel = getPrecedenceLevel(_operator);
            if(!precedenceLevel.has_value() || precedenceLevel.value() < minimumPrecedence) break;
            consume();

            auto right = expectExpression(variables, precedenceLevel.value() + 1);

            auto operationExpression = m_ArenaAllocator.allocate<OperationExpressionNode>();
            operationExpression->_operator = _operator;

            DataType resultantType = getResultantType(left, right, _operator);

            auto temp = m_ArenaAllocator.allocate<ExpressionNode>();
            temp->variant = left->variant;
            temp->type = left->type;

            operationExpression->left = temp;
            operationExpression->right = right;

            left->variant = operationExpression;
            left->type = resultantType;
        }

        return left;
    }

    std::optional<TermExpressionNode*> Parser::parseTerm(VarMap& variables, int minimumPrecedence) {
        if(!peek().has_value()) return {};

        switch(peek().value().type) {
            case TokenType::intLiteral:
            case TokenType::boolLiteral:
            case TokenType::charLiteral:
            case TokenType::stringLiteral:
                return parseLiteral();
            case TokenType::identifier:
                return parseIdentifier(variables);
            case TokenType::intType:
            case TokenType::boolType:
            case TokenType::charType:
            case TokenType::toString:
            case TokenType::stoi:
                return parseTypeCast(variables);
            case TokenType::readInt:
            case TokenType::readBool:
            case TokenType::readChar:
            case TokenType::readNext:
            case TokenType::readLine:
                return parseReadInput(variables);
            case TokenType::openParenthesis:
                return parseParenthesisTerm(variables);
            case TokenType::call:
                return parseFunctionCallTerm(variables);
            case TokenType::subtraction:
            case TokenType::notOperator:
                return parseUnaryExpression(variables, minimumPrecedence);
            default:
                return {};
        }
    }

    StatementNode* Parser::parseExit(VarMap& variables) {
        ExpressionNode* expression;

        consume();
        expectCharacter(TokenType::openParenthesis);

        expression = expectExpression(variables);

        expectCharacter(TokenType::closeParenthesis);
        expectCharacter(TokenType::semiColon);

        if(getGroupType(expression->type) != GroupType::Primitive)
            throwError(lastLine(), "Error: Exit code must be an integer, boolean, or character");

        auto exitNode = m_ArenaAllocator.allocate<ExitNode>();
        exitNode->expression = expression;

        return makeStatementNode(exitNode, lastLine());
    }

    StatementNode* Parser::parsePrint(VarMap& variables) {
        ExpressionNode* expression;

        consume();

        expectCharacter(TokenType::openParenthesis);

        expression = expectExpression(variables);

        expectCharacter(TokenType::closeParenthesis);
        expectCharacter(TokenType::semiColon);

        if(getGroupType(expression->type) == GroupType::Arrays)
            throwError(lastLine(), "Error: Unable to print expression of type '" + dataTypeToString(expression->type)  + "'");

        auto printNode = m_ArenaAllocator.allocate<PrintNode>();
        printNode->expression = expression;

        return makeStatementNode(printNode, lastLine());
    }

    StatementNode* Parser::parseVariableDeclaration(VarMap& variables) {
        Token token, identifier;
        DataType dataType;
        ExpressionNode* expression;

        consume();

        token = expectToken("Error: Expected a data type after 'set'", TokenType::intType, TokenType::boolType, TokenType::charType, TokenType::stringType);
        dataType = tokenToDataType(token.type);

        identifier = expectToken("Error: Expected variable identifier", TokenType::identifier);

        expectCharacter(TokenType::assignment);

        if(getGroupType(dataType) == GroupType::Primitive || getGroupType(dataType) == GroupType::Strings) {
            expression = expectExpression(variables);

            if(getGroupType(expression->type) != getGroupType(dataType))
                throwError(lastLine(), "Error: Cannot assign value of type '" + dataTypeToString(expression->type) +
                                       "' to variable '" + identifier.value.value() + "' of type '" + dataTypeToString(dataType) + "'");
        }
        else {
            expectCharacter(TokenType::newKeyword);
            expectCharacter(TokenType::arrayType);
            expectCharacter(TokenType::openBracket);

            expression = expectExpression(variables);
            if(getGroupType(expression->type) != GroupType::Primitive)
                throwError(lastLine(), "Error: Array size must be an integer, boolean, or character");

            expectCharacter(TokenType::closeBracket);
        }

        expectCharacter(TokenType::semiColon);

        if(variables.find(identifier.value.value()) != variables.end())
            throwError(lastLine(), "Error: Redefinition of variable '" + identifier.value.value() + "'");

        variables.insert({identifier.value.value(), dataType});

        auto declarationNode = m_ArenaAllocator.allocate<DeclarationNode>();
        declarationNode->identifier = identifier;
        declarationNode->type = dataType;
        declarationNode->expression = expression;

        return makeStatementNode(declarationNode, lastLine());
    }

    StatementNode* Parser::parseReAssignment(VarMap& variables) {
        Token identifier;
        ExpressionNode* expression;
        std::optional<ExpressionNode*> index;
        DataType variableDataType;

        identifier = consume();

        if(variables.find(identifier.value.value()) == variables.end())
            throwError(lastLine(), "Error: Use of undeclared variable '" + identifier.value.value() + "'");

        variableDataType = variables[identifier.value.value()];

        if(tryPeek(TokenType::openBracket)) {
            if(getGroupType(variableDataType) == GroupType::Primitive)
                throwError(lastLine(), "Error: Cannot index value of type '" + dataTypeToString(variableDataType) + "'");

            consume();

            index = expectExpression(variables);

            expectCharacter(TokenType::closeBracket);

            variableDataType = getPrimitiveVariant(variableDataType);
        }

        expectCharacter(TokenType::assignment);

        expression = expectExpression(variables);

        expectCharacter(TokenType::semiColon);

        if(getGroupType(variableDataType) != getGroupType(expression->type))
            throwError(lastLine(), "Error: Cannot assign value of type '" + dataTypeToString(expression->type) +
                                   "' to variable '" + identifier.value.value() + "' of type '" + dataTypeToString(variableDataType) + "'");

        auto reAssignmentNode = m_ArenaAllocator.allocate<ReAssignmentNode>();
        reAssignmentNode->identifier = identifier;
        reAssignmentNode->expression = expression;
        reAssignmentNode->index = index;

        return makeStatementNode(reAssignmentNode, lastLine());
    }

    StatementNode* Parser::parseScopeStatement(VarMap& variables, std::string_view curFunction) {
        std::vector<StatementNode*> statements;

        consume();

        statements = parseScope(variables, curFunction);

        auto scopeNode = m_ArenaAllocator.allocate<ScopeNode>();
        scopeNode->statements = statements;

        return makeStatementNode(scopeNode, lastLine());
    }

    StatementNode* Parser::parseConditional(VarMap& variables, std::string_view curFunction) {
        std::vector<ExpressionNode*> conditions = {};
        std::vector<std::vector<StatementNode*>> statements = {};

        consume();

        conditions.push_back(expectExpression(variables));

        expectCharacter(TokenType::openCurlyBrace);

        statements.push_back(parseScope(variables, curFunction));

        while(tryPeek(TokenType::elseIfStatement)) {
            consume();

            conditions.push_back(expectExpression(variables));

            expectCharacter(TokenType::openCurlyBrace);

            statements.push_back(parseScope(variables, curFunction));
        }

        if(tryPeek(TokenType::elseStatement)) {
            consume();

            expectCharacter(TokenType::openCurlyBrace);

            statements.push_back(parseScope(variables, curFunction));
        }

        auto conditionalNode = m_ArenaAllocator.allocate<ConditionalNode>();
        conditionalNode->conditions = conditions;
        conditionalNode->statements = statements;

        return makeStatementNode(conditionalNode, lastLine());
    }

    StatementNode* Parser::parseLoop(VarMap &variables, std::string_view curFunction) {
        ExpressionNode* condition;
        std::vector<StatementNode*> statements;

        consume();

        condition = expectExpression(variables);

        expectCharacter(TokenType::openCurlyBrace);

        statements = parseScope(variables, curFunction);

        auto loopNode = m_ArenaAllocator.allocate<LoopNode>();
        loopNode->condition = condition;
        loopNode->statements = statements;

        return makeStatementNode(loopNode, lastLine());
    }

    StatementNode* Parser::parseFunctionDeclaration(std::string_view curFunction) {
        DataType returnType;
        Token token, identifier;
        std::vector<Variable> arguments;
        std::vector<StatementNode*> statements = {};
        VarMap variableMap;

        if(curFunction != "") throwError(lastLine(), "Error: Cannot define a function inside another function");

        consume();

        token = expectToken("Error: Expected a data type after 'define'", TokenType::intType, TokenType::boolType, TokenType::charType, TokenType::stringType, TokenType::voidType);
        returnType = tokenToDataType(token.type);

        identifier = expectToken("Error: Expected curFunction identifier", TokenType::identifier);

        if(m_functions.find(identifier.value.value()) != m_functions.end())
            throwError(lastLine(), "Error: Redefinition of '" + identifier.value.value() + "'");

        expectCharacter(TokenType::openParenthesis);

        while(peek().has_value() && !tryPeek(TokenType::closeParenthesis)) {
            Token argumentIdentifier;
            DataType argumentDataType;

            argumentDataType = tokenToDataType(consume().type);
            argumentIdentifier = expectToken("Error: Expected variable identifier", TokenType::identifier);

            if(variableMap.find(argumentIdentifier.value.value()) != variableMap.end())
                throwError(lastLine(), "Error: Redefinition of argument '" + argumentIdentifier.value.value() + "'");

            arguments.push_back({.identifier = argumentIdentifier, .dataType = argumentDataType});
            variableMap.insert({argumentIdentifier.value.value(), argumentDataType});

            if(tryPeek(TokenType::comma)) consume();
            else break;
        }

        expectCharacter(TokenType::closeParenthesis);
        expectCharacter(TokenType::openCurlyBrace);

        m_functions.insert({identifier.value.value(), Function{.returnType = returnType, .arguments = arguments}});

        statements = parseScope(variableMap, std::string_view(identifier.value.value()));

        auto functionNode = m_ArenaAllocator.allocate<FunctionNode>();
        functionNode->returnType = returnType;
        functionNode->identifier = identifier;
        functionNode->arguments = arguments;
        functionNode->statements = statements;

        return makeStatementNode(functionNode, lastLine());
    }

    StatementNode* Parser::parseReturn(VarMap& variables, std::string_view curFunction) {
        std::optional<ExpressionNode*> expression;
        std::string functionName = static_cast<std::string>(curFunction);

        consume();

        if(curFunction == "") throwError(lastLine(), "Error: Cannot return outside a function");

        expression = parseExpression(variables);

        if(m_functions[functionName].returnType == DataType::Void && expression.has_value())
            throwError(lastLine(), "Error: Void function '" + functionName + "()' cannot return a value");
        else if(m_functions[functionName].returnType != DataType::Void && !expression.has_value())
            throwError(lastLine(), "Error: Function '" + functionName + "()' must return a value of type '" +
                                   dataTypeToString(m_functions[functionName].returnType) + "'");
        else if(expression.has_value() && getGroupType(expression.value()->type) != getGroupType(m_functions[functionName].returnType))
            throwError(lastLine(), "Error: Function '" + functionName + "()' cannot return a value of type '" +
                                   dataTypeToString(expression.value()->type) + "'; expected '" + dataTypeToString(m_functions[functionName].returnType) + "'");

        if(m_functions[functionName].returnType != DataType::Void) expression.value()->type = m_functions[functionName].returnType;

        expectCharacter(TokenType::semiColon);

        auto returnNode = m_ArenaAllocator.allocate<ReturnNode>();
        returnNode->functionName = functionName;
        returnNode->expression = expression;

        return makeStatementNode(returnNode, lastLine());
    }

    StatementNode* Parser::parseFunctionCallStatement(VarMap& variables) {
        auto functionCall = parseFunctionCall(variables);

        expectCharacter(TokenType::semiColon);

        return makeStatementNode(functionCall, lastLine());
    }

    DataType Parser::getResultantType(ExpressionNode* left, ExpressionNode* right, TokenType _operator) {
        if(getGroupType(left->type) == GroupType::Primitive && getGroupType(right->type) == GroupType::Primitive) {
            switch(_operator) {
                case TokenType::addition:
                case TokenType::subtraction:
                case TokenType::multiplication:
                case TokenType::division:
                case TokenType::modulo:
                    return DataType::Integer;
                case TokenType::equalTo:
                case TokenType::notEqualTo:
                case TokenType::lessThan:
                case TokenType::lessThanOrEqual:
                case TokenType::greaterThan:
                case TokenType::greaterThanOrEqual:
                case TokenType::andOperator:
                case TokenType::orOperator:
                    return DataType::Boolean;
            }
        }
        else if(isTextType(left->type) && isTextType(right->type)) {
            convertToString(left);
            convertToString(right);

            switch(_operator) {
                case TokenType::addition:
                    return DataType::String;
                case TokenType::equalTo:
                case TokenType::notEqualTo:
                case TokenType::lessThan:
                case TokenType::lessThanOrEqual:
                case TokenType::greaterThan:
                case TokenType::greaterThanOrEqual:
                    return DataType::Boolean;
            }
        }
        throwError(lastLine(), "Error: Operator '" + Tokenizer::tokenToString(_operator) + "' cannot be applied to types '" +
                dataTypeToString(left->type) + "' and '" + dataTypeToString(right->type) + "'");
    }

    TermExpressionNode* Parser::parseLiteral() {
        Token literal = consume();

        DataType dataType;
        switch(literal.type) {
            case TokenType::intLiteral:
                dataType = DataType::Integer;
                break;
            case TokenType::boolLiteral:
                dataType = DataType::Boolean;
                break;
            case TokenType::charLiteral:
                dataType = DataType::Character;
                break;
            case TokenType::stringLiteral:
                dataType = DataType::String;
                break;
            default:
                throwError(lastLine(), "Internal compiler error: Unknown data type");
        }

        auto literalExpression = m_ArenaAllocator.allocate<LiteralTerm>();
        literalExpression->literal = literal;

        return makeTermNode(literalExpression, dataType, lastLine());
    }

    TermExpressionNode* Parser::parseIdentifier(VarMap& variables) {
        Token identifier = consume();
        DataType dataType;

        if(variables.find(identifier.value.value()) == variables.end())
            throwError(lastLine(), "Error: Use of undeclared variable '" + identifier.value.value() + "'");

        dataType = variables[identifier.value.value()];

        if(tryPeek(TokenType::openBracket)) {
            ExpressionNode* index;

            consume();

            if(getGroupType(dataType) == GroupType::Primitive)
                throwError(lastLine(), "Error: Cannot index primitive variable '" + identifier.value.value() + "'");

            index = expectExpression(variables);

            expectCharacter(TokenType::closeBracket);

            auto indexedTerm = m_ArenaAllocator.allocate<IndexedTerm>();
            indexedTerm->identifier = identifier;
            indexedTerm->index = index;

            return makeTermNode(indexedTerm, getPrimitiveVariant(dataType), lastLine());
        }
        else if(tryPeek(TokenType::dot)) {
            Token property;
            consume();

            if(!peek().has_value()) throwError(lastLine(), "Error: Expected a property after '.'");
            property = consume();

            if((property.type == TokenType::size && getGroupType(dataType) != GroupType::Arrays) ||
               (property.type == TokenType::length && dataType != DataType::String))
                throwError(lastLine(),"Error: Property '" + Tokenizer::tokenToString(property.type) +
                                      "' does not exist on type '" + dataTypeToString(dataType) + "'");

            expectCharacter(TokenType::openParenthesis);
            expectCharacter(TokenType::closeParenthesis);

            switch(property.type) {
                case TokenType::size:
                case TokenType::length:
                    dataType = DataType::Integer;
                    break;
            }

            auto propertyTerm = m_ArenaAllocator.allocate<PropertyTerm>();
            propertyTerm->identifier = identifier;
            propertyTerm->property = property.type;

            return makeTermNode(propertyTerm, dataType, lastLine());
        }
        else {
            auto identifierTerm = m_ArenaAllocator.allocate<IdentifierTerm>();
            identifierTerm->identifier = identifier;

            return makeTermNode(identifierTerm, dataType, lastLine());
        }
    }

    TermExpressionNode* Parser::parseTypeCast(VarMap& variables) {
        TokenType typeCast = consume().type;
        ExpressionNode* expression;

        DataType newDataType;
        GroupType expectedGroupType;
        switch(typeCast) {
            case TokenType::intType:
                expectedGroupType = GroupType::Primitive;
                newDataType = DataType::Integer;
                break;
            case TokenType::boolType:
                expectedGroupType = GroupType::Primitive;
                newDataType = DataType::Boolean;
                break;
            case TokenType::charType:
                expectedGroupType = GroupType::Primitive;
                newDataType = DataType::Character;
                break;
            case TokenType::toString:
                expectedGroupType = GroupType::Primitive;
                newDataType = DataType::String;
                break;
            case TokenType::stoi:
                expectedGroupType = GroupType::Strings;
                newDataType = DataType::Integer;
                break;
            default:
                throwError(lastLine(), "Internal compiler error: Unknown type cast '" + Tokenizer::tokenToString(typeCast) + "'");
        }

        expectCharacter(TokenType::openParenthesis);

        expression = expectExpression(variables);

        expectCharacter(TokenType::closeParenthesis);

        if(expectedGroupType != getGroupType(expression->type))
            throwError(lastLine(), "Error: Type cast '" + Tokenizer::tokenToString(typeCast) +
                                   "' cannot be used with type '" + dataTypeToString(expression->type) + "'");

        auto typeCastTerm = m_ArenaAllocator.allocate<TypeCastTerm>();
        typeCastTerm->typeCast = typeCast;
        typeCastTerm->expression = expression;

        return makeTermNode(typeCastTerm, newDataType, lastLine());
    }

    TermExpressionNode* Parser::parseReadInput(VarMap& variables) {
        TokenType readType = consume().type;
        DataType dataType;

        expectCharacter(TokenType::openParenthesis);

        auto expression = parseExpression(variables);
        if(expression.has_value() && expression.value()->type != DataType::String)
            throwError(expression.value()->lineNumber, "Error: Argument of '" + Tokenizer::tokenToString(readType) + "' must be type 'string'");

        expectCharacter(TokenType::closeParenthesis);

        switch(readType) {
            case TokenType::readInt:
                dataType = DataType::Integer;
                break;
            case TokenType::readBool:
                dataType = DataType::Boolean;
                break;
            case TokenType::readChar:
                dataType = DataType::Character;
                break;
            case TokenType::readNext:
            case TokenType::readLine:
                dataType = DataType::String;
                break;
            default:
                throwError(lastLine(), "Internal compiler error: Unknown method");
        }

        auto inputTerm = m_ArenaAllocator.allocate<InputTerm>();
        inputTerm->readType = readType;
        inputTerm->inputMessage = expression;

        return makeTermNode(inputTerm, dataType, lastLine());
    }

    TermExpressionNode* Parser::parseParenthesisTerm(VarMap& variables) {
        ExpressionNode* expression;

        consume();

        expression = expectExpression(variables);

        expectCharacter(TokenType::closeParenthesis);

        auto parenthesisTerm = m_ArenaAllocator.allocate<ParenthesisTerm>();
        parenthesisTerm->expression = expression;

        return makeTermNode(parenthesisTerm, parenthesisTerm->expression->type, lastLine());
    }

    TermExpressionNode* Parser::parseFunctionCallTerm(VarMap& variables) {
        auto functionCall = parseFunctionCall(variables);
        auto returnType = m_functions[functionCall->identifier.value.value()].returnType;

        if(returnType == DataType::Void)
            throwError(lastLine(), "Error: Function '" + functionCall->identifier.value.value() +
            "()' returns void and cannot be used in an expression");

        return makeTermNode(functionCall, returnType, lastLine());
    }

    TermExpressionNode* Parser::parseUnaryExpression(VarMap& variables, int minimumPrecedence) {
        TokenType _operator = consume().type;
        DataType resultantType;
        int precedenceLevel;

        switch(_operator) {
            case TokenType::subtraction:
                precedenceLevel = NEGATION_PRECEDENCE_LEVEL;
                break;
            case TokenType::notOperator:
                precedenceLevel = NOT_OPERATOR_PRECEDENCE_LEVEL;
                resultantType = DataType::Boolean;
                break;
            default:
                throwError(lastLine(), "Internal Compiler Error: Unknown unary operator");
        }

        auto unaryTerm = m_ArenaAllocator.allocate<UnaryExpressionTerm>();
        unaryTerm->_operator = _operator;
        unaryTerm->expression = expectExpression(variables, std::max(minimumPrecedence, precedenceLevel));

        if(getGroupType(unaryTerm->expression->type) != GroupType::Primitive)
            throwError(lastLine(), "Error: Unary operator '" + Tokenizer::tokenToString(_operator) + "' cannot be applied to type '" +
                                   dataTypeToString(unaryTerm->expression->type) + "'");

        if(_operator == TokenType::subtraction) resultantType = unaryTerm->expression->type;

        return makeTermNode(unaryTerm, resultantType, lastLine());
    }

    FunctionCall* Parser::parseFunctionCall(VarMap &variables) {
        Token identifier;
        std::vector<Argument> arguments;
        int argumentCount = 0;

        consume();

        identifier = expectToken("Error: Expected function identifier", TokenType::identifier);
        expectCharacter(TokenType::openParenthesis);

        if(m_functions.find(identifier.value.value()) == m_functions.end())
            throwError(lastLine(), "Error: Use of undeclared function '" + identifier.value.value() + "()'");

        auto curFunction = m_functions[identifier.value.value()];

        while(peek().has_value() && !tryPeek(TokenType::closeParenthesis)) {
            if(argumentCount >= curFunction.arguments.size()) {
                throwError(lastLine(), "Error: Function '" + identifier.value.value() + "()' expects " +
                                       std::to_string(curFunction.arguments.size()) + " arguments");
            }

            Token argumentIdentifier = curFunction.arguments[argumentCount].identifier;
            auto expression = expectExpression(variables);

            Argument curArgument = {.identifier = argumentIdentifier, .expression = expression};
            Variable expectedArgument = curFunction.arguments[argumentCount];

            if(getGroupType(expression->type) != getGroupType(expectedArgument.dataType)) {
                throwError(lastLine(), "Error: Argument '" + curArgument.identifier.value.value() + "' of '" +
                                       identifier.value.value() + "()' must be of type '" + dataTypeToString(expectedArgument.dataType) + "', but got '" +
                                       dataTypeToString(curArgument.expression->type) + "'");
            }

            arguments.push_back(curArgument);
            ++argumentCount;

            if(tryPeek(TokenType::comma)) consume();
            else break;
        }

        expectCharacter(TokenType::closeParenthesis);

        if(argumentCount != curFunction.arguments.size()) {
            throwError(lastLine(), "Error: Function '" + identifier.value.value() + "()' expects " +
                                   std::to_string(curFunction.arguments.size()) + " arguments, but got " + std::to_string(argumentCount));
        }

        auto functionCall = m_ArenaAllocator.allocate<FunctionCall>();
        functionCall->identifier = identifier;
        functionCall->arguments = arguments;

        return functionCall;
    }

    [[nodiscard]] std::optional<Token>Parser::peek(int ahead) const {
        if(m_Index + ahead >= m_Tokens.size() || m_Index + ahead < 0) return {};
        else return m_Tokens.at(m_Index + ahead);
    }

    Token Parser::consume() {
        return m_Tokens.at(m_Index++);
    }

    Token Parser::expectCharacter(TokenType type) {
        if(!tryPeek(type)) {
            if(peek().has_value()) throwError(lastLine(), "Error: Expected '" + Tokenizer::tokenToString(type) +
                                                          "' but found '" + Tokenizer::tokenToString(peek().value().type) + "'");
            else throwError(lastLine(), "Error: Expected '" + Tokenizer::tokenToString(type) + "' but found none");
        }
        return consume();
    }

    ExpressionNode* Parser::expectExpression(VarMap &variables, int minimumPrecedence) {
        auto expression = parseExpression(variables, minimumPrecedence);

        if(expression.has_value()) return expression.value();
        throwError(lastLine(), "Error: Expected expression after '" + Tokenizer::tokenToString(peek(-1).value().type) + "'");
    }

    void Parser::convertToString(ExpressionNode* expression) {
        if(expression->type == DataType::String) return;

        auto tempNode = m_ArenaAllocator.allocate<ExpressionNode>();
        tempNode->variant = expression->variant;
        tempNode->type = expression->type;
        tempNode->lineNumber = expression->lineNumber;

        auto typeCastTerm = m_ArenaAllocator.allocate<TypeCastTerm>();
        typeCastTerm->typeCast = TokenType::toString;
        typeCastTerm->expression = tempNode;

        expression->variant = makeTermNode(typeCastTerm, DataType::String, expression->lineNumber);
        expression->type = DataType::String;
    }

    [[nodiscard]] int Parser::lastLine() const {
        if(!peek(-1).has_value()) return -1;
        return peek(-1).value().lineNumber;
    }

    bool Parser::isTextType(DataType dataType) const{
        return dataType == DataType::String || dataType == DataType::Character;
    }

    DataType Parser::tokenToDataType(TokenType type) {
        switch (type) {
            case TokenType::intType:
                if(tryPeek(TokenType::openBracket) && tryPeek(1, TokenType::closeBracket)) {
                    consume(); consume();
                    return DataType::IntArray;
                }
                return DataType::Integer;
            case TokenType::boolType:
                if(tryPeek(TokenType::openBracket) && tryPeek(1, TokenType::closeBracket)) {
                    consume(); consume();
                    return DataType::BoolArray;
                }
                return DataType::Boolean;
            case TokenType::charType:
                if(tryPeek(TokenType::openBracket) && tryPeek(1, TokenType::closeBracket)) {
                    consume(); consume();
                    return DataType::CharArray;
                }
                return DataType::Character;
            case TokenType::stringType:
                return DataType::String;
            case TokenType::voidType:
                return DataType::Void;
            default:
                throwError(lastLine(), "Error: Unknown data type '" + Tokenizer::tokenToString(type) + "'");
        }
    }

    std::optional<int> Parser::getPrecedenceLevel(TokenType type) const{
        switch (type) {
            case TokenType::orOperator:
                return 0;
            case TokenType::andOperator:
                return 1;
            case TokenType::equalTo:
            case TokenType::notEqualTo:
            case TokenType::lessThan:
            case TokenType::lessThanOrEqual:
            case TokenType::greaterThan:
            case TokenType::greaterThanOrEqual:
                return 3;
            case TokenType::addition:
            case TokenType::subtraction:
                return 4;
            case TokenType::multiplication:
            case TokenType::division:
            case TokenType::modulo:
                return 5;
            default:
                return {};
        }
    }

    void Parser::throwError(int lineNumber, std::string message) {
        std::cerr << "Line " << lineNumber << ": " << message << std::endl;
        exit(EXIT_FAILURE);
    }
}