#include "MLabParser.hpp"
#include <stdexcept>

namespace mlab {

// ============================================================
// Конструктор
// ============================================================

Parser::Parser(const std::vector<Token> &tokens)
    : tokens_(tokens)
{
    if (tokens_.empty() || tokens_.back().type != TokenType::END_OF_INPUT) {
        Token eof;
        eof.type = TokenType::END_OF_INPUT;
        eof.value = "";
        eof.line = tokens_.empty() ? 1 : tokens_.back().line;
        eof.col = 0;
        tokens_.push_back(eof);
    }
}

// ============================================================
// Навигация по токенам
// ============================================================

Parser::SourceLoc Parser::loc() const
{
    return {current().line, current().col};
}

const Token &Parser::current() const
{
    if (pos_ >= tokens_.size())
        return tokens_.back();
    return tokens_[pos_];
}

const Token &Parser::peekToken(int off) const
{
    size_t p = pos_ + static_cast<size_t>(off);
    if (p >= tokens_.size())
        return tokens_.back();
    return tokens_[p];
}

bool Parser::isAtEnd() const
{
    return current().type == TokenType::END_OF_INPUT;
}

bool Parser::check(TokenType t) const
{
    return current().type == t;
}

bool Parser::match(TokenType t)
{
    if (check(t)) {
        pos_++;
        return true;
    }
    return false;
}

Token Parser::consume(TokenType t, const std::string &msg)
{
    if (check(t))
        return tokens_[pos_++];
    std::string expected = msg.empty() ? ("token type " + std::to_string(static_cast<int>(t)))
                                       : msg;
    throw std::runtime_error("Parse error at line " + std::to_string(current().line) + " col "
                             + std::to_string(current().col) + ": expected " + expected + ", got '"
                             + current().value + "'");
}

void Parser::skipNewlines()
{
    while (!isAtEnd() && (check(TokenType::NEWLINE) || check(TokenType::SEMICOLON)))
        pos_++;
}

void Parser::skipTerminators()
{
    while (!isAtEnd()
           && (check(TokenType::NEWLINE) || check(TokenType::SEMICOLON) || check(TokenType::COMMA)))
        pos_++;
}

bool Parser::isTerminator(std::initializer_list<TokenType> terminators) const
{
    auto cur = current().type;
    for (auto t : terminators) {
        if (cur == t)
            return true;
    }
    return false;
}

// ============================================================
// Безопасный парсинг числа (чистая функция)
// ============================================================

double Parser::parseDouble(const std::string &text, int line, int col)
{
    try {
        return std::stod(text);
    } catch (const std::exception &) {
        throw std::runtime_error("Invalid number literal '" + text + "' at line "
                                 + std::to_string(line) + " col " + std::to_string(col));
    }
}

// ============================================================
// Точка входа
// ============================================================

ASTNodePtr Parser::parse()
{
    auto [ln, cl] = loc();
    auto block = makeNode(NodeType::BLOCK, ln, cl);
    skipNewlines();
    while (!isAtEnd()) {
        auto stmt = parseStatement();
        if (stmt)
            block->children.push_back(std::move(stmt));
        skipNewlines();
    }
    return block;
}

// ============================================================
// Statements
// ============================================================

ASTNodePtr Parser::parseStatement()
{
    switch (current().type) {
    case TokenType::KW_FUNCTION:
        return parseFunctionDef();
    case TokenType::KW_IF:
        return parseIf();
    case TokenType::KW_FOR:
        return parseFor();
    case TokenType::KW_WHILE:
        return parseWhile();
    case TokenType::KW_SWITCH:
        return parseSwitch();
    case TokenType::KW_TRY:
        return parseTryCatch();
    case TokenType::KW_GLOBAL:
    case TokenType::KW_PERSISTENT:
        return parseGlobalPersistent();
    case TokenType::KW_BREAK: {
        auto [ln, cl] = loc();
        auto node = makeNode(NodeType::BREAK_STMT, ln, cl);
        pos_++;
        skipTerminators();
        return node;
    }
    case TokenType::KW_CONTINUE: {
        auto [ln, cl] = loc();
        auto node = makeNode(NodeType::CONTINUE_STMT, ln, cl);
        pos_++;
        skipTerminators();
        return node;
    }
    case TokenType::KW_RETURN: {
        // MATLAB return не принимает выражения — просто возврат из функции
        auto [ln, cl] = loc();
        auto node = makeNode(NodeType::RETURN_STMT, ln, cl);
        pos_++;
        skipTerminators();
        return node;
    }
    default:
        return parseExpressionStatement();
    }
}

ASTNodePtr Parser::parseExpressionStatement()
{
    // Попытка multi-assign: [a, b] = expr  или  [~, b] = expr
    if (check(TokenType::LBRACKET)) {
        size_t save = pos_;
        auto ma = tryMultiAssign();
        if (ma)
            return ma;
        pos_ = save;
    }

    auto [startLine, startCol] = loc();
    auto expr = parseExpression();

    if (check(TokenType::ASSIGN)) {
        pos_++;
        // a = [] — удаление
        if (check(TokenType::LBRACKET) && peekToken(1).type == TokenType::RBRACKET) {
            pos_ += 2;
            auto node = makeNode(NodeType::DELETE_ASSIGN, startLine, startCol);
            node->children.push_back(std::move(expr));
            node->suppressOutput = match(TokenType::SEMICOLON);
            skipNewlines();
            return node;
        }
        auto rhs = parseExpression();
        auto node = makeNode(NodeType::ASSIGN, startLine, startCol);
        node->children.push_back(std::move(expr));
        node->children.push_back(std::move(rhs));
        node->suppressOutput = match(TokenType::SEMICOLON);
        skipNewlines();
        return node;
    }

    auto stmt = makeNode(NodeType::EXPR_STMT, startLine, startCol);
    stmt->children.push_back(std::move(expr));
    stmt->suppressOutput = match(TokenType::SEMICOLON);
    skipNewlines();
    return stmt;
}

ASTNodePtr Parser::tryMultiAssign()
{
    // Мягкий разбор — возвращает nullptr без исключений при неудаче.
    // Поддерживает ~ (tilde) для игнорируемых выходов: [~, b] = func()
    if (!check(TokenType::LBRACKET))
        return nullptr;

    auto [startLine, startCol] = loc();
    pos_++;

    std::vector<std::string> names;

    // Первый элемент: идентификатор или ~
    if (check(TokenType::IDENTIFIER)) {
        names.push_back(current().value);
        pos_++;
    } else if (check(TokenType::TILDE)) {
        names.push_back("~");
        pos_++;
    } else {
        return nullptr;
    }

    // Остальные элементы через запятую
    while (check(TokenType::COMMA)) {
        pos_++;
        if (check(TokenType::IDENTIFIER)) {
            names.push_back(current().value);
            pos_++;
        } else if (check(TokenType::TILDE)) {
            names.push_back("~");
            pos_++;
        } else {
            return nullptr;
        }
    }

    if (!check(TokenType::RBRACKET))
        return nullptr;
    pos_++;

    if (!check(TokenType::ASSIGN))
        return nullptr;

    // Точка невозврата: после '=' это точно multi-assign.
    // Ошибка в RHS — реальная синтаксическая ошибка.
    pos_++;

    auto rhs = parseExpression();
    auto node = makeNode(NodeType::MULTI_ASSIGN, startLine, startCol);
    node->returnNames = std::move(names);
    node->children.push_back(std::move(rhs));
    node->suppressOutput = match(TokenType::SEMICOLON);
    skipNewlines();
    return node;
}

// ============================================================
// Control flow
// ============================================================

ASTNodePtr Parser::parseIf()
{
    auto [ln, cl] = loc();
    auto node = makeNode(NodeType::IF_STMT, ln, cl);
    consume(TokenType::KW_IF, "if");

    auto cond = parseExpression();
    skipTerminators();
    auto body = parseBlock({TokenType::KW_ELSEIF, TokenType::KW_ELSE, TokenType::KW_END});
    node->branches.push_back({std::move(cond), std::move(body)});

    while (check(TokenType::KW_ELSEIF)) {
        pos_++;
        auto c = parseExpression();
        skipTerminators();
        auto b = parseBlock({TokenType::KW_ELSEIF, TokenType::KW_ELSE, TokenType::KW_END});
        node->branches.push_back({std::move(c), std::move(b)});
    }

    if (match(TokenType::KW_ELSE)) {
        skipTerminators();
        node->elseBranch = parseBlock({TokenType::KW_END});
    }

    consume(TokenType::KW_END, "end");
    skipTerminators();
    return node;
}

ASTNodePtr Parser::parseFor()
{
    auto [ln, cl] = loc();
    auto node = makeNode(NodeType::FOR_STMT, ln, cl);
    consume(TokenType::KW_FOR, "for");

    node->strValue = consume(TokenType::IDENTIFIER, "loop variable").value;
    consume(TokenType::ASSIGN, "=");
    node->children.push_back(parseExpression());
    skipTerminators();

    node->children.push_back(parseBlock({TokenType::KW_END}));
    consume(TokenType::KW_END, "end");
    skipTerminators();
    return node;
}

ASTNodePtr Parser::parseWhile()
{
    auto [ln, cl] = loc();
    auto node = makeNode(NodeType::WHILE_STMT, ln, cl);
    consume(TokenType::KW_WHILE, "while");

    node->children.push_back(parseExpression());
    skipTerminators();

    node->children.push_back(parseBlock({TokenType::KW_END}));
    consume(TokenType::KW_END, "end");
    skipTerminators();
    return node;
}

ASTNodePtr Parser::parseSwitch()
{
    auto [ln, cl] = loc();
    auto node = makeNode(NodeType::SWITCH_STMT, ln, cl);
    consume(TokenType::KW_SWITCH, "switch");

    node->children.push_back(parseExpression());
    skipTerminators();

    while (check(TokenType::KW_CASE)) {
        pos_++;
        auto ce = parseExpression();
        skipTerminators();
        auto b = parseBlock({TokenType::KW_CASE, TokenType::KW_OTHERWISE, TokenType::KW_END});
        node->branches.push_back({std::move(ce), std::move(b)});
    }

    if (match(TokenType::KW_OTHERWISE)) {
        skipTerminators();
        node->elseBranch = parseBlock({TokenType::KW_END});
    }

    consume(TokenType::KW_END, "end");
    skipTerminators();
    return node;
}

ASTNodePtr Parser::parseTryCatch()
{
    auto [ln, cl] = loc();
    auto node = makeNode(NodeType::TRY_STMT, ln, cl);
    consume(TokenType::KW_TRY, "try");
    skipTerminators();

    node->children.push_back(parseBlock({TokenType::KW_CATCH, TokenType::KW_END}));

    if (match(TokenType::KW_CATCH)) {
        if (check(TokenType::IDENTIFIER)) {
            node->strValue = current().value;
            pos_++;
        }
        skipTerminators();
        node->children.push_back(parseBlock({TokenType::KW_END}));
    }

    consume(TokenType::KW_END, "end");
    skipTerminators();
    return node;
}

ASTNodePtr Parser::parseGlobalPersistent()
{
    bool isGlobal = check(TokenType::KW_GLOBAL);
    auto [ln, cl] = loc();
    auto node = makeNode(isGlobal ? NodeType::GLOBAL_STMT : NodeType::PERSISTENT_STMT, ln, cl);
    pos_++;

    while (check(TokenType::IDENTIFIER)) {
        node->paramNames.push_back(current().value);
        pos_++;
    }

    skipTerminators();
    return node;
}

// ============================================================
// Function definition
// ============================================================

bool Parser::probeHasOutputSignature() const
{
    if (current().type == TokenType::LBRACKET) {
        size_t probe = pos_ + 1;
        int depth = 1;
        while (probe < tokens_.size() && depth > 0) {
            if (tokens_[probe].type == TokenType::END_OF_INPUT)
                break;
            if (tokens_[probe].type == TokenType::LBRACKET)
                depth++;
            else if (tokens_[probe].type == TokenType::RBRACKET)
                depth--;
            probe++;
        }
        return (probe < tokens_.size() && tokens_[probe].type == TokenType::ASSIGN);
    }

    if (current().type == TokenType::IDENTIFIER && peekToken(1).type == TokenType::ASSIGN)
        return true;

    return false;
}

ASTNodePtr Parser::parseFunctionDef()
{
    auto [ln, cl] = loc();
    auto node = makeNode(NodeType::FUNCTION_DEF, ln, cl);
    consume(TokenType::KW_FUNCTION, "function");

    bool hasOutput = probeHasOutputSignature();

    if (hasOutput) {
        if (check(TokenType::LBRACKET)) {
            pos_++;
            node->returnNames.push_back(consume(TokenType::IDENTIFIER, "return var").value);
            while (match(TokenType::COMMA))
                node->returnNames.push_back(consume(TokenType::IDENTIFIER, "return var").value);
            consume(TokenType::RBRACKET, "]");
        } else {
            node->returnNames.push_back(consume(TokenType::IDENTIFIER, "return var").value);
        }
        consume(TokenType::ASSIGN, "=");
    }

    node->strValue = consume(TokenType::IDENTIFIER, "function name").value;

    if (match(TokenType::LPAREN)) {
        if (!check(TokenType::RPAREN)) {
            node->paramNames.push_back(consume(TokenType::IDENTIFIER, "param").value);
            while (match(TokenType::COMMA))
                node->paramNames.push_back(consume(TokenType::IDENTIFIER, "param").value);
        }
        consume(TokenType::RPAREN, ")");
    }

    skipTerminators();
    node->children.push_back(parseBlock({TokenType::KW_END}));

    if (check(TokenType::KW_END)) {
        pos_++;
    } else if (!isAtEnd()) {
        throw std::runtime_error("Expected 'end' for function '" + node->strValue
                                 + "' defined at line " + std::to_string(node->line));
    }

    skipTerminators();
    return node;
}

// ============================================================
// Block
// ============================================================

ASTNodePtr Parser::parseBlock(std::initializer_list<TokenType> terminators)
{
    auto [ln, cl] = loc();
    auto block = makeNode(NodeType::BLOCK, ln, cl);
    while (!isAtEnd()) {
        if (isTerminator(terminators))
            return block;
        size_t before = pos_;
        auto stmt = parseStatement();
        if (stmt)
            block->children.push_back(std::move(stmt));
        skipNewlines();
        if (pos_ == before) {
            throw std::runtime_error("Parse error: stuck in block at line "
                                     + std::to_string(current().line) + " col "
                                     + std::to_string(current().col));
        }
    }
    return block;
}

// ============================================================
// Expressions
// ============================================================

ASTNodePtr Parser::parseExpression()
{
    return parseOr();
}

ASTNodePtr Parser::parseOr()
{
    auto left = parseAnd();
    while (check(TokenType::OR) || check(TokenType::OR_SHORT)) {
        auto [ln, cl] = loc();
        std::string op = current().value;
        pos_++;
        auto n = makeNode(NodeType::BINARY_OP, ln, cl);
        n->strValue = std::move(op);
        n->children.push_back(std::move(left));
        n->children.push_back(parseAnd());
        left = std::move(n);
    }
    return left;
}

ASTNodePtr Parser::parseAnd()
{
    auto left = parseComparison();
    while (check(TokenType::AND) || check(TokenType::AND_SHORT)) {
        auto [ln, cl] = loc();
        std::string op = current().value;
        pos_++;
        auto n = makeNode(NodeType::BINARY_OP, ln, cl);
        n->strValue = std::move(op);
        n->children.push_back(std::move(left));
        n->children.push_back(parseComparison());
        left = std::move(n);
    }
    return left;
}

ASTNodePtr Parser::parseComparison()
{
    auto left = parseColon();
    while (check(TokenType::EQ) || check(TokenType::NEQ) || check(TokenType::LT)
           || check(TokenType::GT) || check(TokenType::LEQ) || check(TokenType::GEQ)) {
        auto [ln, cl] = loc();
        std::string op = current().value;
        pos_++;
        auto n = makeNode(NodeType::BINARY_OP, ln, cl);
        n->strValue = std::move(op);
        n->children.push_back(std::move(left));
        n->children.push_back(parseColon());
        left = std::move(n);
    }
    return left;
}

ASTNodePtr Parser::parseColon()
{
    auto start = parseAddSub();
    if (check(TokenType::COLON)) {
        auto [ln, cl] = loc();
        pos_++;
        auto second = parseAddSub();
        if (check(TokenType::COLON)) {
            pos_++;
            auto n = makeNode(NodeType::COLON_EXPR, ln, cl);
            n->children.push_back(std::move(start));
            n->children.push_back(std::move(second));
            n->children.push_back(parseAddSub());
            return n;
        }
        auto n = makeNode(NodeType::COLON_EXPR, ln, cl);
        n->children.push_back(std::move(start));
        n->children.push_back(std::move(second));
        return n;
    }
    return start;
}

ASTNodePtr Parser::parseAddSub()
{
    auto left = parseMulDiv();
    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        auto [ln, cl] = loc();
        std::string op = current().value;
        pos_++;
        auto n = makeNode(NodeType::BINARY_OP, ln, cl);
        n->strValue = std::move(op);
        n->children.push_back(std::move(left));
        n->children.push_back(parseMulDiv());
        left = std::move(n);
    }
    return left;
}

ASTNodePtr Parser::parseMulDiv()
{
    auto left = parseUnary();
    while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::BACKSLASH)
           || check(TokenType::DOT_BACKSLASH) || check(TokenType::DOT_STAR)
           || check(TokenType::DOT_SLASH)) {
        auto [ln, cl] = loc();
        std::string op = current().value;
        pos_++;
        auto n = makeNode(NodeType::BINARY_OP, ln, cl);
        n->strValue = std::move(op);
        n->children.push_back(std::move(left));
        n->children.push_back(parseUnary());
        left = std::move(n);
    }
    return left;
}

ASTNodePtr Parser::parseUnary()
{
    if (check(TokenType::MINUS)) {
        auto [ln, cl] = loc();
        pos_++;
        auto n = makeNode(NodeType::UNARY_OP, ln, cl);
        n->strValue = "-";
        n->children.push_back(parsePower());
        return n;
    }
    if (check(TokenType::TILDE)) {
        auto [ln, cl] = loc();
        pos_++;
        auto n = makeNode(NodeType::UNARY_OP, ln, cl);
        n->strValue = "~";
        n->children.push_back(parsePower());
        return n;
    }
    if (check(TokenType::PLUS)) {
        pos_++;
        return parsePower();
    }
    return parsePower();
}

ASTNodePtr Parser::parsePower()
{
    auto left = parsePostfix();
    if (check(TokenType::CARET) || check(TokenType::DOT_CARET)) {
        auto [ln, cl] = loc();
        std::string op = current().value;
        pos_++;
        auto n = makeNode(NodeType::BINARY_OP, ln, cl);
        n->strValue = std::move(op);
        n->children.push_back(std::move(left));
        n->children.push_back(parseUnary());
        return n;
    }
    return left;
}

ASTNodePtr Parser::parsePostfix()
{
    auto node = parsePrimary();
    while (true) {
        size_t before = pos_;

        if (check(TokenType::LPAREN)) {
            // В MATLAB f(args) может быть как вызовом функции, так и
            // индексацией массива. Различение происходит на этапе интерпретации.
            auto [ln, cl] = loc();
            pos_++;
            auto cn = makeNode(NodeType::CALL, ln, cl);
            cn->children.push_back(std::move(node));
            if (!check(TokenType::RPAREN)) {
                cn->children.push_back(parseExpression());
                while (match(TokenType::COMMA))
                    cn->children.push_back(parseExpression());
            }
            consume(TokenType::RPAREN, ")");
            node = std::move(cn);
        } else if (check(TokenType::LBRACE)) {
            auto [ln, cl] = loc();
            pos_++;
            auto cn = makeNode(NodeType::CELL_INDEX, ln, cl);
            cn->children.push_back(std::move(node));
            cn->children.push_back(parseExpression());
            while (match(TokenType::COMMA))
                cn->children.push_back(parseExpression());
            consume(TokenType::RBRACE, "}");
            node = std::move(cn);
        } else if (check(TokenType::DOT) && peekToken(1).type == TokenType::IDENTIFIER) {
            auto [ln, cl] = loc();
            pos_++;
            auto fn = makeNode(NodeType::FIELD_ACCESS, ln, cl);
            fn->strValue = consume(TokenType::IDENTIFIER, "field").value;
            fn->children.push_back(std::move(node));
            node = std::move(fn);
        } else if (check(TokenType::APOSTROPHE)) {
            auto [ln, cl] = loc();
            pos_++;
            auto tn = makeNode(NodeType::UNARY_OP, ln, cl);
            tn->strValue = "'";
            tn->children.push_back(std::move(node));
            node = std::move(tn);
        } else if (check(TokenType::DOT_APOSTROPHE)) {
            auto [ln, cl] = loc();
            pos_++;
            auto tn = makeNode(NodeType::UNARY_OP, ln, cl);
            tn->strValue = ".'";
            tn->children.push_back(std::move(node));
            node = std::move(tn);
        } else {
            break;
        }

        if (pos_ == before)
            break;
    }
    return node;
}

ASTNodePtr Parser::parsePrimary()
{
    auto [ln, cl] = loc();

    switch (current().type) {
    case TokenType::NUMBER: {
        auto n = makeNode(NodeType::NUMBER_LITERAL, ln, cl);
        n->numValue = parseDouble(current().value, ln, cl);
        n->strValue = current().value;
        pos_++;
        return n;
    }
    case TokenType::IMAG_NUMBER: {
        auto n = makeNode(NodeType::IMAG_LITERAL, ln, cl);
        n->numValue = parseDouble(current().value, ln, cl);
        n->strValue = current().value;
        pos_++;
        return n;
    }
    case TokenType::STRING: {
        auto n = makeNode(NodeType::STRING_LITERAL, ln, cl);
        n->strValue = current().value;
        pos_++;
        return n;
    }
    case TokenType::KW_TRUE: {
        pos_++;
        auto n = makeNode(NodeType::BOOL_LITERAL, ln, cl);
        n->boolValue = true;
        return n;
    }
    case TokenType::KW_FALSE: {
        pos_++;
        auto n = makeNode(NodeType::BOOL_LITERAL, ln, cl);
        n->boolValue = false;
        return n;
    }
    case TokenType::KW_END: {
        pos_++;
        return makeNode(NodeType::END_VAL, ln, cl);
    }
    case TokenType::IDENTIFIER: {
        auto n = makeNode(NodeType::IDENTIFIER, ln, cl);
        n->strValue = current().value;
        pos_++;
        return n;
    }
    case TokenType::AT:
        return parseAnonFunc();
    case TokenType::LPAREN: {
        pos_++;
        auto e = parseExpression();
        consume(TokenType::RPAREN, ")");
        return e;
    }
    case TokenType::LBRACKET:
        return parseMatrixLiteral();
    case TokenType::LBRACE:
        return parseCellLiteral();
    case TokenType::COLON: {
        pos_++;
        return makeNode(NodeType::COLON_EXPR, ln, cl);
    }
    default:
        throw std::runtime_error("Unexpected token '" + current().value + "' at line "
                                 + std::to_string(ln) + " col " + std::to_string(cl));
    }
}

// ============================================================
// Anonymous functions
// ============================================================

ASTNodePtr Parser::parseAnonFunc()
{
    auto [ln, cl] = loc();
    consume(TokenType::AT, "@");

    // @funcName — хэндл на существующую функцию
    if (check(TokenType::IDENTIFIER) && peekToken(1).type != TokenType::LPAREN) {
        auto n = makeNode(NodeType::ANON_FUNC, ln, cl);
        n->strValue = current().value;
        pos_++;
        return n;
    }

    // @(params) expr — анонимная функция
    auto n = makeNode(NodeType::ANON_FUNC, ln, cl);
    consume(TokenType::LPAREN, "(");
    if (!check(TokenType::RPAREN)) {
        n->paramNames.push_back(consume(TokenType::IDENTIFIER, "param").value);
        while (match(TokenType::COMMA))
            n->paramNames.push_back(consume(TokenType::IDENTIFIER, "param").value);
    }
    consume(TokenType::RPAREN, ")");
    n->children.push_back(parseExpression());
    return n;
}

// ============================================================
// Array / Cell literals (общий код)
// ============================================================

ASTNodePtr Parser::parseArrayLiteral(TokenType open, TokenType close, NodeType nodeType)
{
    auto [ln, cl] = loc();
    const char *openStr = (open == TokenType::LBRACKET) ? "[" : "{";
    const char *closeStr = (close == TokenType::RBRACKET) ? "]" : "}";
    consume(open, openStr);

    auto node = makeNode(nodeType, ln, cl);

    if (check(close)) {
        pos_++;
        return node;
    }

    auto row = makeNode(NodeType::BLOCK, current().line, current().col);
    row->children.push_back(parseExpression());

    while (!check(close) && !isAtEnd()) {
        size_t beforeIteration = pos_;

        if (check(TokenType::SEMICOLON) || check(TokenType::NEWLINE)) {
            node->children.push_back(std::move(row));
            row = makeNode(NodeType::BLOCK, current().line, current().col);
            pos_++;
            while (!isAtEnd() && (check(TokenType::NEWLINE) || check(TokenType::SEMICOLON)))
                pos_++;
            if (check(close))
                break;
        } else if (check(TokenType::COMMA)) {
            pos_++;
        }

        if (!check(close) && !check(TokenType::SEMICOLON) && !check(TokenType::NEWLINE))
            row->children.push_back(parseExpression());

        if (pos_ == beforeIteration) {
            throw std::runtime_error("Parse error: stuck in array literal at line "
                                     + std::to_string(current().line) + " col "
                                     + std::to_string(current().col));
        }
    }

    if (!row->children.empty())
        node->children.push_back(std::move(row));

    consume(close, closeStr);
    return node;
}

ASTNodePtr Parser::parseMatrixLiteral()
{
    return parseArrayLiteral(TokenType::LBRACKET, TokenType::RBRACKET, NodeType::MATRIX_LITERAL);
}

ASTNodePtr Parser::parseCellLiteral()
{
    return parseArrayLiteral(TokenType::LBRACE, TokenType::RBRACE, NodeType::CELL_LITERAL);
}

} // namespace mlab
