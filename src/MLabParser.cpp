#include "MLabParser.hpp"
#include <stdexcept>

namespace mlab {

Parser::Parser(const std::vector<Token> &tokens)
    : tokens_(tokens)
{}

const Token &Parser::current() const
{
    return tokens_[pos_];
}
const Token &Parser::peekToken(int off) const
{
    size_t p = pos_ + off;
    return (p < tokens_.size()) ? tokens_[p] : tokens_.back();
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

ASTNodePtr Parser::parse()
{
    auto block = makeNode(NodeType::BLOCK);
    skipNewlines(); // было skipNewlines — ОК
    while (!isAtEnd()) {
        auto stmt = parseStatement();
        if (stmt)
            block->children.push_back(std::move(stmt));
        skipNewlines(); // между statement'ами — только newline/semicolon
    }
    return block;
}

ASTNodePtr Parser::parseStatement()
{
    if (check(TokenType::KW_FUNCTION))
        return parseFunctionDef();
    if (check(TokenType::KW_IF))
        return parseIf();
    if (check(TokenType::KW_FOR))
        return parseFor();
    if (check(TokenType::KW_WHILE))
        return parseWhile();
    if (check(TokenType::KW_SWITCH))
        return parseSwitch();
    if (check(TokenType::KW_TRY))
        return parseTryCatch();
    if (check(TokenType::KW_GLOBAL) || check(TokenType::KW_PERSISTENT))
        return parseGlobalPersistent();
    if (check(TokenType::KW_BREAK)) {
        pos_++;
        skipTerminators();
        return makeNode(NodeType::BREAK_STMT);
    }
    if (check(TokenType::KW_CONTINUE)) {
        pos_++;
        skipTerminators();
        return makeNode(NodeType::CONTINUE_STMT);
    }
    if (check(TokenType::KW_RETURN)) {
        pos_++;
        skipTerminators();
        return makeNode(NodeType::RETURN_STMT);
    }
    return parseExpressionStatement();
}

ASTNodePtr Parser::parseExpressionStatement()
{
    if (check(TokenType::LBRACKET)) {
        size_t save = pos_;
        if (auto ma = tryMultiAssign())
            return ma;
        pos_ = save;
    }
    auto expr = parseExpression();
    if (check(TokenType::ASSIGN)) {
        pos_++;
        if (check(TokenType::LBRACKET) && peekToken(1).type == TokenType::RBRACKET) {
            pos_ += 2;
            auto node = makeNode(NodeType::DELETE_ASSIGN);
            node->children.push_back(std::move(expr));
            node->suppressOutput = match(TokenType::SEMICOLON);
            skipNewlines();
            return node;
        }
        auto rhs = parseExpression();
        auto node = makeNode(NodeType::ASSIGN);
        node->children.push_back(std::move(expr));
        node->children.push_back(std::move(rhs));
        node->suppressOutput = match(TokenType::SEMICOLON);
        skipNewlines();
        return node;
    }
    auto stmt = makeNode(NodeType::EXPR_STMT);
    stmt->children.push_back(std::move(expr));
    stmt->suppressOutput = match(TokenType::SEMICOLON);
    skipNewlines();
    return stmt;
}

ASTNodePtr Parser::tryMultiAssign()
{
    consume(TokenType::LBRACKET, "[");
    std::vector<std::string> names;
    names.push_back(consume(TokenType::IDENTIFIER, "identifier").value);
    while (match(TokenType::COMMA))
        names.push_back(consume(TokenType::IDENTIFIER, "identifier").value);
    consume(TokenType::RBRACKET, "]");
    if (!check(TokenType::ASSIGN))
        return nullptr;
    pos_++;
    auto rhs = parseExpression();
    auto node = makeNode(NodeType::MULTI_ASSIGN);
    node->returnNames = std::move(names);
    node->children.push_back(std::move(rhs));
    node->suppressOutput = match(TokenType::SEMICOLON);
    skipNewlines();
    return node;
}

ASTNodePtr Parser::parseIf()
{
    consume(TokenType::KW_IF, "if");
    auto node = makeNode(NodeType::IF_STMT);
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
    consume(TokenType::KW_FOR, "for");
    auto node = makeNode(NodeType::FOR_STMT);
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
    consume(TokenType::KW_WHILE, "while");
    auto node = makeNode(NodeType::WHILE_STMT);
    node->children.push_back(parseExpression());
    skipTerminators();
    node->children.push_back(parseBlock({TokenType::KW_END}));
    consume(TokenType::KW_END, "end");
    skipTerminators();
    return node;
}

ASTNodePtr Parser::parseSwitch()
{
    consume(TokenType::KW_SWITCH, "switch");
    auto node = makeNode(NodeType::SWITCH_STMT);
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
    consume(TokenType::KW_TRY, "try");
    auto node = makeNode(NodeType::TRY_STMT);
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
    pos_++;
    auto node = makeNode(isGlobal ? NodeType::GLOBAL_STMT : NodeType::PERSISTENT_STMT);
    while (check(TokenType::IDENTIFIER)) {
        node->paramNames.push_back(current().value);
        pos_++;
    }
    skipTerminators();
    return node;
}

ASTNodePtr Parser::parseFunctionDef()
{
    consume(TokenType::KW_FUNCTION, "function");
    auto node = makeNode(NodeType::FUNCTION_DEF);
    size_t save = pos_;
    bool hasOutput = false;
    if (check(TokenType::LBRACKET)) {
        pos_++;
        while (!check(TokenType::RBRACKET) && !isAtEnd())
            pos_++;
        if (!isAtEnd())
            pos_++;
        if (check(TokenType::ASSIGN))
            hasOutput = true;
        pos_ = save;
    } else if (check(TokenType::IDENTIFIER) && peekToken(1).type == TokenType::ASSIGN) {
        hasOutput = true;
    }
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

ASTNodePtr Parser::parseBlock(std::vector<TokenType> terminators)
{
    auto block = makeNode(NodeType::BLOCK);
    while (!isAtEnd()) {
        for (auto t : terminators)
            if (check(t))
                return block;
        auto stmt = parseStatement();
        if (stmt)
            block->children.push_back(std::move(stmt));
        skipNewlines(); // только newline/semicolon
    }
    return block;
}

ASTNodePtr Parser::parseExpression()
{
    return parseOr();
}

ASTNodePtr Parser::parseOr()
{
    auto left = parseAnd();
    while (check(TokenType::OR) || check(TokenType::OR_SHORT)) {
        std::string op = current().value;
        pos_++;
        auto n = makeNode(NodeType::BINARY_OP);
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
        pos_++;
        auto n = makeNode(NodeType::BINARY_OP);
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
        pos_++;
        auto n = makeNode(NodeType::BINARY_OP);
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
        pos_++;
        auto second = parseAddSub();
        if (check(TokenType::COLON)) {
            pos_++;
            auto n = makeNode(NodeType::COLON_EXPR);
            n->children.push_back(std::move(start));
            n->children.push_back(std::move(second));
            n->children.push_back(parseAddSub());
            return n;
        }
        auto n = makeNode(NodeType::COLON_EXPR);
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
        pos_++;
        auto n = makeNode(NodeType::BINARY_OP);
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
           || check(TokenType::DOT_STAR) || check(TokenType::DOT_SLASH)) {
        std::string op = current().value;
        pos_++;
        auto n = makeNode(NodeType::BINARY_OP);
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
        pos_++;
        auto n = makeNode(NodeType::UNARY_OP);
        n->strValue = "-";
        n->children.push_back(parsePower());
        return n;
    }
    if (check(TokenType::NOT)) {
        pos_++;
        auto n = makeNode(NodeType::UNARY_OP);
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
        pos_++;
        auto n = makeNode(NodeType::BINARY_OP);
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
            pos_++;
            auto cn = makeNode(NodeType::CALL);
            cn->children.push_back(std::move(node));
            if (!check(TokenType::RPAREN)) {
                cn->children.push_back(parseExpression());
                while (match(TokenType::COMMA))
                    cn->children.push_back(parseExpression());
            }
            consume(TokenType::RPAREN, ")");
            node = std::move(cn);
        } else if (check(TokenType::LBRACE)) {
            pos_++;
            auto cn = makeNode(NodeType::CELL_INDEX);
            cn->children.push_back(std::move(node));
            cn->children.push_back(parseExpression());
            while (match(TokenType::COMMA))
                cn->children.push_back(parseExpression());
            consume(TokenType::RBRACE, "}");
            node = std::move(cn);
        } else if (check(TokenType::DOT) && peekToken(1).type == TokenType::IDENTIFIER) {
            pos_++;
            auto fn = makeNode(NodeType::FIELD_ACCESS);
            fn->strValue = consume(TokenType::IDENTIFIER, "field").value;
            fn->children.push_back(std::move(node));
            node = std::move(fn);
        } else if (check(TokenType::APOSTROPHE)) {
            pos_++;
            auto tn = makeNode(NodeType::UNARY_OP);
            tn->strValue = "'";
            tn->children.push_back(std::move(node));
            node = std::move(tn);
        } else if (check(TokenType::DOT_APOSTROPHE)) {
            pos_++;
            auto tn = makeNode(NodeType::UNARY_OP);
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
    if (check(TokenType::NUMBER)) {
        auto n = makeNode(NodeType::NUMBER_LITERAL);
        n->numValue = std::stod(current().value);
        n->strValue = current().value;
        pos_++;
        return n;
    }
    if (check(TokenType::IMAG_NUMBER)) {
        auto n = makeNode(NodeType::IMAG_LITERAL);
        n->numValue = std::stod(current().value);
        n->strValue = current().value;
        pos_++;
        return n;
    }
    if (check(TokenType::STRING)) {
        auto n = makeNode(NodeType::STRING_LITERAL);
        n->strValue = current().value;
        pos_++;
        return n;
    }
    if (check(TokenType::KW_TRUE)) {
        pos_++;
        auto n = makeNode(NodeType::BOOL_LITERAL);
        n->boolValue = true;
        return n;
    }
    if (check(TokenType::KW_FALSE)) {
        pos_++;
        auto n = makeNode(NodeType::BOOL_LITERAL);
        n->boolValue = false;
        return n;
    }
    if (check(TokenType::KW_END)) {
        pos_++;
        return makeNode(NodeType::END_VAL);
    }
    if (check(TokenType::IDENTIFIER)) {
        auto n = makeNode(NodeType::IDENTIFIER);
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
        return makeNode(NodeType::COLON_EXPR);
    }
    throw std::runtime_error("Unexpected token '" + current().value + "' at line "
                             + std::to_string(current().line));
}

ASTNodePtr Parser::parseAnonFunc()
{
    consume(TokenType::AT, "@");
    if (check(TokenType::IDENTIFIER) && peekToken(1).type != TokenType::LPAREN) {
        auto n = makeNode(NodeType::ANON_FUNC);
        n->strValue = current().value;
        pos_++;
        return n;
    }
    auto n = makeNode(NodeType::ANON_FUNC);
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

ASTNodePtr Parser::parseMatrixLiteral()
{
    consume(TokenType::LBRACKET, "[");
    auto node = makeNode(NodeType::MATRIX_LITERAL);
    if (check(TokenType::RBRACKET)) {
        pos_++;
        return node;
    }
    auto row = makeNode(NodeType::BLOCK);
    row->children.push_back(parseExpression());
    while (!check(TokenType::RBRACKET) && !isAtEnd()) {
        if (check(TokenType::SEMICOLON) || check(TokenType::NEWLINE)) {
            node->children.push_back(std::move(row));
            row = makeNode(NodeType::BLOCK);
            pos_++;
            while (!isAtEnd() && (check(TokenType::NEWLINE) || check(TokenType::SEMICOLON)))
                pos_++;
            if (check(TokenType::RBRACKET))
                break;
        } else if (check(TokenType::COMMA)) {
            pos_++;
        }
        if (!check(TokenType::RBRACKET) && !check(TokenType::SEMICOLON)
            && !check(TokenType::NEWLINE))
            row->children.push_back(parseExpression());
    }
    if (!row->children.empty())
        node->children.push_back(std::move(row));
    consume(TokenType::RBRACKET, "]");
    return node;
}

ASTNodePtr Parser::parseCellLiteral()
{
    consume(TokenType::LBRACE, "{");
    auto node = makeNode(NodeType::CELL_LITERAL);

    if (check(TokenType::RBRACE)) {
        pos_++;
        return node;
    }

    // Каждый child — одна строка cell, внутри которой элементы через запятую
    auto row = makeNode(NodeType::BLOCK);
    row->children.push_back(parseExpression());

    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        if (check(TokenType::SEMICOLON) || check(TokenType::NEWLINE)) {
            node->children.push_back(std::move(row));
            row = makeNode(NodeType::BLOCK);
            pos_++;
            while (!isAtEnd() && (check(TokenType::NEWLINE) || check(TokenType::SEMICOLON)))
                pos_++;
            if (check(TokenType::RBRACE))
                break;
        } else if (check(TokenType::COMMA)) {
            pos_++;
        }
        if (!check(TokenType::RBRACE) && !check(TokenType::SEMICOLON) && !check(TokenType::NEWLINE))
            row->children.push_back(parseExpression());
    }
    if (!row->children.empty())
        node->children.push_back(std::move(row));

    consume(TokenType::RBRACE, "}");
    return node;
}

} // namespace mlab
