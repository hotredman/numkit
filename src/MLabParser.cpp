#include "MLabParser.hpp"
#include <stdexcept>

namespace mlab {

// ============================================================
// Конструктор
// ============================================================

Parser::Parser(const std::vector<Token> &tokens)
    : tokens_(tokens)
{
    // Гарантируем, что список токенов заканчивается END_OF_INPUT
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

const Token &Parser::current() const
{
    if (pos_ >= tokens_.size()) {
        return tokens_.back(); // всегда END_OF_INPUT благодаря конструктору
    }
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
    throw std::runtime_error("Parse error at line " + std::to_string(current().line) + " col "
                             + std::to_string(current().col) + ": expected " + msg + " got '"
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
// Точка входа
// ============================================================

ASTNodePtr Parser::parse()
{
    auto block = makeNode(NodeType::BLOCK, current().line, current().col);
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
        auto node = makeNode(NodeType::BREAK_STMT, current().line, current().col);
        pos_++;
        skipTerminators();
        return node;
    }
    case TokenType::KW_CONTINUE: {
        auto node = makeNode(NodeType::CONTINUE_STMT, current().line, current().col);
        pos_++;
        skipTerminators();
        return node;
    }
    case TokenType::KW_RETURN: {
        // MATLAB return не принимает выражения — просто возврат из функции
        auto node = makeNode(NodeType::RETURN_STMT, current().line, current().col);
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
    // Попытка multi-assign: [a, b] = expr
    if (check(TokenType::LBRACKET)) {
        size_t save = pos_;
        auto ma = tryMultiAssign();
        if (ma)
            return ma;
        pos_ = save; // tryMultiAssign гарантирует мягкий откат
    }

    int startLine = current().line;
    int startCol = current().col;

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
    // Мягкий разбор — возвращает nullptr без исключений при неудаче
    if (!check(TokenType::LBRACKET))
        return nullptr;
    pos_++;

    std::vector<std::string> names;

    if (!check(TokenType::IDENTIFIER))
        return nullptr;
    names.push_back(current().value);
    pos_++;

    while (check(TokenType::COMMA)) {
        pos_++;
        if (!check(TokenType::IDENTIFIER))
            return nullptr;
        names.push_back(current().value);
        pos_++;
    }

    if (!check(TokenType::RBRACKET))
        return nullptr;
    pos_++;

    if (!check(TokenType::ASSIGN))
        return nullptr;
    int startLine = current().line;
    int startCol = current().col;
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
    auto node = makeNode(NodeType::IF_STMT, current().line, current().col);
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
    auto node = makeNode(NodeType::FOR_STMT, current().line, current().col);
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
    auto node = makeNode(NodeType::WHILE_STMT, current().line, current().col);
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
    auto node = makeNode(NodeType::SWITCH_STMT, current().line, current().col);
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
    auto node = makeNode(NodeType::TRY_STMT, current().line, current().col);
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
    auto node = makeNode(isGlobal ? NodeType::GLOBAL_STMT : NodeType::PERSISTENT_STMT,
                         current().line,
                         current().col);
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
    // Проверяем, есть ли выходные аргументы, не мутируя pos_
    if (current().type == TokenType::LBRACKET) {
        size_t probe = pos_ + 1;
        int depth = 1;
        while (probe < tokens_.size() && depth > 0) {
            if (tokens_[probe].type == TokenType::LBRACKET)
                depth++;
            else if (tokens_[probe].type == TokenType::RBRACKET)
                depth--;
            if (tokens_[probe].type == TokenType::END_OF_INPUT)
                break;
            probe++;
        }
        return (probe < tokens_.size() && tokens_[probe].type == TokenType::ASSIGN);
    }

    if (current().type == TokenType::IDENTIFIER && peekToken(1).type == TokenType::ASSIGN) {
        return true;
    }

    return false;
}

ASTNodePtr Parser::parseFunctionDef()
{
    auto node = makeNode(NodeType::FUNCTION_DEF, current().line, current().col);
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

    if (check(TokenType::KW_END))
        pos_++;

    skipTerminators();
    return node;
}

// ============================================================
// Block
// ============================================================

ASTNodePtr Parser::parseBlock(std::initializer_list<TokenType> terminators)
{
    auto block = makeNode(NodeType::BLOCK, current().line, current().col);
    while (!isAtEnd()) {
        if (isTerminator(terminators))
            return block;
        auto stmt = parseStatement();
        if (stmt)
            block->children.push_back(std::move(stmt));
        skipNewlines();
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
        std::string op = current().value;
        int ln = current().line, cl = current().col;
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
        std::string op = current().value;
        int ln = current().line, cl = current().col;
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
        std::string op = current().value;
        int ln = current().line, cl = current().col;
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
        int ln = current().line, cl = current().col;
        pos_++;
        auto second = parseAddSub();
        if (check(TokenType::COLON)) {
            pos_++;
            auto n = makeNode(NodeType::COLON_EXPR, ln, cl);
            n->children.push_back(std::move(start));  // start
            n->children.push_back(std::move(second)); // step
            n->children.push_back(parseAddSub());     // stop
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
        std::string op = current().value;
        int ln = current().line, cl = current().col;
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
        std::string op = current().value;
        int ln = current().line, cl = current().col;
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
        int ln = current().line, cl = current().col;
        pos_++;
        auto n = makeNode(NodeType::UNARY_OP, ln, cl);
        n->strValue = "-";
        n->children.push_back(parsePower());
        return n;
    }
    if (check(TokenType::TILDE)) {
        int ln = current().line, cl = current().col;
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
        std::string op = current().value;
        int ln = current().line, cl = current().col;
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
        if (check(TokenType::LPAREN)) {
            int ln = current().line, cl = current().col;
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
            int ln = current().line, cl = current().col;
            pos_++;
            auto cn = makeNode(NodeType::CELL_INDEX, ln, cl);
            cn->children.push_back(std::move(node));
            cn->children.push_back(parseExpression());
            while (match(TokenType::COMMA))
                cn->children.push_back(parseExpression());
            consume(TokenType::RBRACE, "}");
            node = std::move(cn);
        } else if (check(TokenType::DOT) && peekToken(1).type == TokenType::IDENTIFIER) {
            int ln = current().line, cl = current().col;
            pos_++;
            auto fn = makeNode(NodeType::FIELD_ACCESS, ln, cl);
            fn->strValue = consume(TokenType::IDENTIFIER, "field").value;
            fn->children.push_back(std::move(node));
            node = std::move(fn);
        } else if (check(TokenType::APOSTROPHE)) {
            int ln = current().line, cl = current().col;
            pos_++;
            auto tn = makeNode(NodeType::UNARY_OP, ln, cl);
            tn->strValue = "'";
            tn->children.push_back(std::move(node));
            node = std::move(tn);
        } else if (check(TokenType::DOT_APOSTROPHE)) {
            int ln = current().line, cl = current().col;
            pos_++;
            auto tn = makeNode(NodeType::UNARY_OP, ln, cl);
            tn->strValue = ".'";
            tn->children.push_back(std::move(node));
            node = std::move(tn);
        } else {
            break;
        }
    }
    return node;
}

ASTNodePtr Parser::parsePrimary()
{
    int ln = current().line;
    int cl = current().col;

    if (check(TokenType::NUMBER)) {
        auto n = makeNode(NodeType::NUMBER_LITERAL, ln, cl);
        n->numValue = std::stod(current().value);
        n->strValue = current().value;
        pos_++;
        return n;
    }
    if (check(TokenType::IMAG_NUMBER)) {
        auto n = makeNode(NodeType::IMAG_LITERAL, ln, cl);
        n->numValue = std::stod(current().value);
        n->strValue = current().value;
        pos_++;
        return n;
    }
    if (check(TokenType::STRING)) {
        auto n = makeNode(NodeType::STRING_LITERAL, ln, cl);
        n->strValue = current().value;
        pos_++;
        return n;
    }
    if (check(TokenType::KW_TRUE)) {
        pos_++;
        auto n = makeNode(NodeType::BOOL_LITERAL, ln, cl);
        n->boolValue = true;
        return n;
    }
    if (check(TokenType::KW_FALSE)) {
        pos_++;
        auto n = makeNode(NodeType::BOOL_LITERAL, ln, cl);
        n->boolValue = false;
        return n;
    }
    if (check(TokenType::KW_END)) {
        pos_++;
        return makeNode(NodeType::END_VAL, ln, cl);
    }
    if (check(TokenType::IDENTIFIER)) {
        auto n = makeNode(NodeType::IDENTIFIER, ln, cl);
        n->strValue = current().value;
        pos_++;
        return n;
    }
    if (check(TokenType::AT))
        return parseAnonFunc();
    if (check(TokenType::LPAREN)) {
        pos_++;
        auto e = parseExpression();
        consume(TokenType::RPAREN, ")");
        return e;
    }
    if (check(TokenType::LBRACKET))
        return parseMatrixLiteral();
    if (check(TokenType::LBRACE))
        return parseCellLiteral();
    if (check(TokenType::COLON)) {
        pos_++;
        return makeNode(NodeType::COLON_EXPR, ln, cl);
    }

    throw std::runtime_error("Unexpected token '" + current().value + "' at line "
                             + std::to_string(ln) + " col " + std::to_string(cl));
}

// ============================================================
// Anonymous functions
// ============================================================

ASTNodePtr Parser::parseAnonFunc()
{
    int ln = current().line, cl = current().col;
    consume(TokenType::AT, "@");

    // @funcName  — хэндл на существующую функцию
    if (check(TokenType::IDENTIFIER) && peekToken(1).type != TokenType::LPAREN) {
        auto n = makeNode(NodeType::ANON_FUNC, ln, cl);
        n->strValue = current().value;
        pos_++;
        return n;
    }

    // @(params) expr  — анонимная функция
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
    int ln = current().line, cl = current().col;
    consume(open, (open == TokenType::LBRACKET) ? "[" : "{");

    auto node = makeNode(nodeType, ln, cl);

    if (check(close)) {
        pos_++;
        return node;
    }

    auto row = makeNode(NodeType::BLOCK, current().line, current().col);
    row->children.push_back(parseExpression());

    while (!check(close) && !isAtEnd()) {
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
    }

    if (!row->children.empty())
        node->children.push_back(std::move(row));

    consume(close, (close == TokenType::RBRACKET) ? "]" : "}");
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
