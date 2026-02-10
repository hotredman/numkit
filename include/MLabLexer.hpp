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
    DOT_BACKSLASH,
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
    TILDE,
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
    ELLIPSIS,
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
    std::vector<char> bracketStack_;

    char peek() const;
    char peek(int offset) const;
    char advance();

    int bracketDepth() const;
    bool inMatrixContext() const;
    void pushBracket(char open);
    void popBracket(char expected);

    bool isAtLineStart() const;
    bool isAtLineStart(size_t position) const;

    void skipSpacesAndComments();
    void skipBlockComment();
    void addToken(TokenType type, const std::string &val, int line, int col);
    bool isValueToken(TokenType t) const;
    bool isTransposeContext() const;

    void readNumber();
    void validateUnderscores(size_t start, size_t end, int startLine, int startCol);
    void readString(int startLine, int startCol);
    void readDoubleQuotedString(int startLine, int startCol);
    void readIdentifier();
    bool readOperator();

    void insertImplicitComma();

    [[noreturn]] void error(const std::string &msg);
    [[noreturn]] void error(const std::string &msg, int line, int col);

    static bool isDigit(char c);
    static bool isAlpha(char c);
    static bool isAlnum(char c);
    static bool isXDigit(char c);
    static char closingFor(char open);
};

} // namespace mlab
