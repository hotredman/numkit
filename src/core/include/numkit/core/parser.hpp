#pragma once

#include <numkit/core/ast.hpp>
#include <numkit/core/lexer.hpp>
#include <initializer_list>
#include <stdexcept>
#include <string>
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
    size_t nestingDepth_ = 0;
    static constexpr size_t MAX_NESTING_DEPTH = 200;

    struct NestingGuard
    {
        Parser &p;
        explicit NestingGuard(Parser &parser)
            : p(parser)
        {
            if (++p.nestingDepth_ > MAX_NESTING_DEPTH) {
                throw std::runtime_error("Parse error at line " + std::to_string(p.current().line)
                                         + " col " + std::to_string(p.current().col)
                                         + ": Expression or block nesting exceeds maximum supported depth ("
                                         + std::to_string(MAX_NESTING_DEPTH) + ")");
            }
        }
        ~NestingGuard() { --p.nestingDepth_; }
    };

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
    // Consume a statement-terminating SEMICOLON or COMMA token (if
    // present), recording its position into node.endLine / node.endCol
    // so downstream tools (the graph viewer's sourceText slicer) can
    // bound the statement's source range precisely. Returns true iff
    // a SEMICOLON was consumed (i.e. suppress-output is set); COMMA
    // returns false (it terminates the stmt but doesn't suppress).
    bool consumeStmtTerminator(ASTNode &node);

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
    ASTNodePtr parseClassDef();
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
    // Reconstruct anonymous-function source from the token span [from, to)
    // (for func2str). MATLAB-style: no inter-token whitespace, literals re-quoted.
    std::string reconstructAnonSource(size_t from, size_t to) const;

    // — Lookahead для function def —
    bool probeHasOutputSignature() const;
};

} // namespace numkit