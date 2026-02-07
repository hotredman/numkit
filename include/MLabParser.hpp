#pragma once

#include "MLabAst.hpp"
#include "MLabLexer.hpp"
#include <vector>

namespace mlab {

class Parser
{
public:
    explicit Parser(const std::vector<Token> &tokens);
    ASTNodePtr parse();

private:
    std::vector<Token> tokens_;
    size_t pos_ = 0;

    const Token &current() const;
    const Token &peekToken(int off = 0) const;
    bool isAtEnd() const;
    bool check(TokenType t) const;
    bool match(TokenType t);
    Token consume(TokenType t, const std::string &msg = "");
    void skipNewlines();
    void skipTerminators();

    ASTNodePtr parseStatement();
    ASTNodePtr parseExpressionStatement();
    ASTNodePtr tryMultiAssign();
    ASTNodePtr parseIf();
    ASTNodePtr parseFor();
    ASTNodePtr parseWhile();
    ASTNodePtr parseSwitch();
    ASTNodePtr parseFunctionDef();
    ASTNodePtr parseTryCatch();
    ASTNodePtr parseGlobalPersistent();
    ASTNodePtr parseBlock(std::vector<TokenType> terminators);

    ASTNodePtr parseExpression();
    ASTNodePtr parseOr();
    ASTNodePtr parseAnd();
    ASTNodePtr parseComparison();
    ASTNodePtr parseColon();
    ASTNodePtr parseAddSub();
    ASTNodePtr parseMulDiv();
    ASTNodePtr parseUnary();
    ASTNodePtr parsePower();
    ASTNodePtr parsePostfix();
    ASTNodePtr parsePrimary();

    ASTNodePtr parseMatrixLiteral();
    ASTNodePtr parseCellLiteral();
    ASTNodePtr parseAnonFunc();
};

} // namespace mlab
