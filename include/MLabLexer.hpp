// include/MLabLexer.hpp
#pragma once

#include <string>
#include <vector>

namespace mlab {

enum class TokenType {
    NUMBER,
    IMAG_NUMBER,
    STRING,
    IDENTIFIER,
    PLUS,
    MINUS,
    STAR,
    SLASH,
    BACKSLASH,
    CARET,
    DOT_STAR,
    DOT_SLASH,
    DOT_CARET,
    DOT_APOSTROPHE,
    EQ,
    NEQ,
    LT,
    GT,
    LEQ,
    GEQ,
    AND,
    OR,
    NOT,
    AND_SHORT,
    OR_SHORT,
    ASSIGN,
    LPAREN,
    RPAREN,
    LBRACKET,
    RBRACKET,
    LBRACE,
    RBRACE,
    COMMA,
    SEMICOLON,
    COLON,
    DOT,
    NEWLINE,
    APOSTROPHE,
    AT,
    KW_IF,
    KW_ELSEIF,
    KW_ELSE,
    KW_END,
    KW_FOR,
    KW_WHILE,
    KW_BREAK,
    KW_CONTINUE,
    KW_RETURN,
    KW_FUNCTION,
    KW_TRUE,
    KW_FALSE,
    KW_SWITCH,
    KW_CASE,
    KW_OTHERWISE,
    KW_TRY,
    KW_CATCH,
    KW_GLOBAL,
    KW_PERSISTENT,
    END_OF_INPUT
};

struct Token
{
    TokenType type;
    std::string value;
    int line = 0;
    int col = 0;
};

class Lexer
{
public:
    explicit Lexer(const std::string &source);
    std::vector<Token> tokenize();

private:
    std::string src_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;
    std::vector<Token> tokens_;
    int bracketDepth_ = 0; // для [], {}

    char peek() const;
    char peek(int offset) const;
    char advance();

    void skipSpacesAndComments();
    void addToken(TokenType type, const std::string &val, int line, int col);
    bool isTransposeContext() const;
    bool isValueToken(TokenType t) const;

    void readNumber();
    void readString(int startLine, int startCol);
    void readDoubleQuotedString(int startLine, int startCol);
    void readIdentifier();
    bool readOperator();

    void insertImplicitComma();
};

} // namespace mlab
