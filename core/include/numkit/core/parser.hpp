#pragma once

#include <numkit/core/ast.hpp>
#include <numkit/core/lexer.hpp>
#include <initializer_list>
#include <vector>

namespace numkit {

class Parser
{
public:
    explicit Parser(const std::vector<Token> &tokens);
    ASTNodePtr parse();

private:
    std::vector<Token> tokens_;
    size_t pos_ = 0;

    // — Позиция в исходнике —
    struct SourceLoc
    {
        int line;
        int col;
    };
    SourceLoc loc() const;

    // — Утилиты навигации по токенам —
    const Token &current() const;
    const Token &peekToken(int off = 0) const;
    bool isAtEnd() const;
    bool check(TokenType t) const;
    bool match(TokenType t);
    Token consume(TokenType t, const std::string &msg = "");
    void skipNewlines();
    void skipTerminators();

    // Single chokepoint for token-position changes. After moving pos_
    // forward, skips past any COMMENT tokens so the parser never has
    // to deal with them — COMMENT is purely lexical metadata for
    // downstream tools (script-graph viewer, formatters, doc
    // extractors). All `pos_++` sites in parser.cpp go through this
    // helper, plus the constructor invokes skipPastComments() once
    // to handle a leading comment.
    void advance();
    void skipPastComments();

    // — Проверка терминаторов —
    bool isTerminator(std::initializer_list<TokenType> terminators) const;

    // — Безопасный парсинг числа —
    static double parseDouble(const std::string &text, int line, int col);

    // — Command-style calls (clear all, grid on, cd dir, etc.) —
    bool isCommandStyleCall() const;
    ASTNodePtr parseCommandStyleCall();

    // — Statements —
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
    ASTNodePtr parseBlock(std::initializer_list<TokenType> terminators);

    // — Expressions (MATLAB precedence, low to high) —
    ASTNodePtr parseExpression();
    ASTNodePtr parseShortCircuitOr();  // ||
    ASTNodePtr parseShortCircuitAnd(); // &&
    ASTNodePtr parseElementOr();       // |
    ASTNodePtr parseElementAnd();      // &
    ASTNodePtr parseComparison();      // == ~= < > <= >=
    ASTNodePtr parseColon();           // :
    ASTNodePtr parseAddSub();          // + -
    ASTNodePtr parseMulDiv();          // * / .* ./
    ASTNodePtr parseUnary();           // - ~ +
    ASTNodePtr parsePower();           // ^ .^
    ASTNodePtr parsePostfix();         // () {} . ' .'
    ASTNodePtr parsePrimary();

    // — Литералы —
    ASTNodePtr parseArrayLiteral(TokenType open, TokenType close, NodeType nodeType);
    ASTNodePtr parseMatrixLiteral();
    ASTNodePtr parseCellLiteral();
    ASTNodePtr parseAnonFunc();

    // — Lookahead для function def —
    bool probeHasOutputSignature() const;
};

} // namespace numkit