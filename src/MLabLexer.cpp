// src/MLabLexer.cpp
#include "MLabLexer.hpp"

#include <stdexcept>
#include <unordered_map>

namespace mlab {

Lexer::Lexer(const std::string &source)
    : src_(source)
{}

char Lexer::peek() const
{
    return pos_ < src_.size() ? src_[pos_] : '\0';
}

char Lexer::peek(int offset) const
{
    size_t p = pos_ + offset;
    return (p < src_.size()) ? src_[p] : '\0';
}

char Lexer::advance()
{
    if (pos_ >= src_.size())
        return '\0';
    char c = src_[pos_++];
    if (c == '\n') {
        line_++;
        col_ = 1;
    } else {
        col_++;
    }
    return c;
}

void Lexer::addToken(TokenType type, const std::string &val, int line, int col)
{
    tokens_.push_back({type, val, line, col});
}

bool Lexer::isValueToken(TokenType t) const
{
    return t == TokenType::NUMBER || t == TokenType::IMAG_NUMBER || t == TokenType::IDENTIFIER
           || t == TokenType::RPAREN || t == TokenType::RBRACKET || t == TokenType::RBRACE
           || t == TokenType::APOSTROPHE || t == TokenType::DOT_APOSTROPHE || t == TokenType::KW_END
           || t == TokenType::KW_TRUE || t == TokenType::KW_FALSE || t == TokenType::STRING;
}

void Lexer::insertImplicitComma()
{
    if (bracketDepth_ <= 0 || tokens_.empty())
        return;
    auto prev = tokens_.back().type;
    if (!isValueToken(prev))
        return;

    size_t scanPos = pos_;

    while (scanPos < src_.size() && (src_[scanPos] == ' ' || src_[scanPos] == '\t'))
        scanPos++;

    if (scanPos >= src_.size())
        return;

    char next = src_[scanPos];

    // +/- внутри [] после значения — всегда бинарный оператор в MATLAB
    if (next == '+' || next == '-')
        return;

    bool nextIsValue = std::isdigit(next) || std::isalpha(next) || next == '_' || next == '('
                       || next == '[' || next == '{' || next == '\'' || next == '"' || next == '~'
                       || next == '@' || next == '.';

    if (nextIsValue) {
        addToken(TokenType::COMMA, ",", line_, col_);
    }
}

void Lexer::skipSpacesAndComments()
{
    while (pos_ < src_.size()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') {
            if (bracketDepth_ > 0 && (c == ' ' || c == '\t')) {
                while (pos_ < src_.size() && (peek() == ' ' || peek() == '\t'))
                    advance();
                insertImplicitComma();
                continue;
            }
            advance();
        } else if (c == '%') {
            while (pos_ < src_.size() && peek() != '\n')
                advance();
        } else if (c == '.' && peek(1) == '.' && peek(2) == '.') {
            // line continuation ...
            while (pos_ < src_.size() && peek() != '\n')
                advance();
            if (pos_ < src_.size())
                advance(); // пропускаем \n, advance() обновит line_/col_
        } else {
            break;
        }
    }
}

bool Lexer::isTransposeContext() const
{
    if (tokens_.empty())
        return false;
    auto t = tokens_.back().type;
    return t == TokenType::RPAREN || t == TokenType::RBRACKET || t == TokenType::RBRACE
           || t == TokenType::IDENTIFIER || t == TokenType::NUMBER || t == TokenType::IMAG_NUMBER
           || t == TokenType::STRING || t == TokenType::APOSTROPHE || t == TokenType::DOT_APOSTROPHE
           || t == TokenType::KW_END || t == TokenType::KW_TRUE || t == TokenType::KW_FALSE;
}

void Lexer::readNumber()
{
    int startLine = line_;
    int startCol = col_;
    size_t start = pos_;

    // Hex: 0x...
    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
        advance(); // '0'
        advance(); // 'x'
        if (pos_ >= src_.size() || !std::isxdigit(peek()))
            throw std::runtime_error("Invalid hex literal at line " + std::to_string(startLine)
                                     + " col " + std::to_string(startCol));
        while (pos_ < src_.size() && std::isxdigit(peek()))
            advance();
        // hex с мнимой единицей
        if (pos_ < src_.size() && (peek() == 'i' || peek() == 'j')) {
            advance();
            addToken(TokenType::IMAG_NUMBER, src_.substr(start, pos_ - start), startLine, startCol);
        } else {
            addToken(TokenType::NUMBER, src_.substr(start, pos_ - start), startLine, startCol);
        }
        return;
    }

    // Binary: 0b...
    if (peek() == '0' && (peek(1) == 'b' || peek(1) == 'B')) {
        advance(); // '0'
        advance(); // 'b'
        if (pos_ >= src_.size() || (peek() != '0' && peek() != '1'))
            throw std::runtime_error("Invalid binary literal at line " + std::to_string(startLine)
                                     + " col " + std::to_string(startCol));
        while (pos_ < src_.size() && (peek() == '0' || peek() == '1'))
            advance();
        if (pos_ < src_.size() && (peek() == 'i' || peek() == 'j')) {
            advance();
            addToken(TokenType::IMAG_NUMBER, src_.substr(start, pos_ - start), startLine, startCol);
        } else {
            addToken(TokenType::NUMBER, src_.substr(start, pos_ - start), startLine, startCol);
        }
        return;
    }

    // Decimal / float
    while (pos_ < src_.size() && std::isdigit(peek()))
        advance();

    if (pos_ < src_.size() && peek() == '.') {
        char next = peek(1);
        // Не съедаем точку если за ней dot-оператор или другая точка
        if (next != '*' && next != '/' && next != '^' && next != '\'' && next != '.'
            && !std::isalpha(next) && next != '(' && next != '[') {
            advance(); // '.'
            while (pos_ < src_.size() && std::isdigit(peek()))
                advance();
        }
    }

    // Exponent
    if (pos_ < src_.size() && (peek() == 'e' || peek() == 'E')) {
        advance();
        if (pos_ < src_.size() && (peek() == '+' || peek() == '-'))
            advance();
        if (pos_ >= src_.size() || !std::isdigit(peek()))
            throw std::runtime_error("Invalid number exponent at line " + std::to_string(startLine)
                                     + " col " + std::to_string(startCol));
        while (pos_ < src_.size() && std::isdigit(peek()))
            advance();
    }

    // Imaginary suffix
    if (pos_ < src_.size() && (peek() == 'i' || peek() == 'j')) {
        advance();
        addToken(TokenType::IMAG_NUMBER, src_.substr(start, pos_ - start), startLine, startCol);
    } else {
        addToken(TokenType::NUMBER, src_.substr(start, pos_ - start), startLine, startCol);
    }
}

void Lexer::readString(int startLine, int startCol)
{
    advance(); // пропускаем открывающий '
    std::string s;
    while (pos_ < src_.size()) {
        if (peek() == '\'') {
            if (peek(1) == '\'') {
                s += '\'';
                advance();
                advance();
            } else {
                advance(); // закрывающий '
                addToken(TokenType::STRING, s, startLine, startCol);
                return;
            }
        } else if (peek() == '\n') {
            throw std::runtime_error("Unterminated string literal at line "
                                     + std::to_string(startLine) + " col "
                                     + std::to_string(startCol));
        } else {
            s += advance();
        }
    }
    throw std::runtime_error("Unterminated string literal at line " + std::to_string(startLine)
                             + " col " + std::to_string(startCol));
}

void Lexer::readDoubleQuotedString(int startLine, int startCol)
{
    advance(); // пропускаем открывающий "
    std::string s;
    while (pos_ < src_.size()) {
        if (peek() == '"') {
            if (peek(1) == '"') {
                // MATLAB-стиль: "" → "
                s += '"';
                advance();
                advance();
            } else {
                advance(); // закрывающий "
                addToken(TokenType::STRING, s, startLine, startCol);
                return;
            }
        } else if (peek() == '\n') {
            throw std::runtime_error("Unterminated string literal at line "
                                     + std::to_string(startLine) + " col "
                                     + std::to_string(startCol));
        } else {
            s += advance();
        }
    }
    throw std::runtime_error("Unterminated string literal at line " + std::to_string(startLine)
                             + " col " + std::to_string(startCol));
}

void Lexer::readIdentifier()
{
    int startLine = line_;
    int startCol = col_;
    size_t start = pos_;

    while (pos_ < src_.size() && (std::isalnum(peek()) || peek() == '_'))
        advance();

    std::string word = src_.substr(start, pos_ - start);

    static const std::unordered_map<std::string, TokenType> keywords = {
        {"if", TokenType::KW_IF},
        {"elseif", TokenType::KW_ELSEIF},
        {"else", TokenType::KW_ELSE},
        {"end", TokenType::KW_END},
        {"for", TokenType::KW_FOR},
        {"while", TokenType::KW_WHILE},
        {"break", TokenType::KW_BREAK},
        {"continue", TokenType::KW_CONTINUE},
        {"return", TokenType::KW_RETURN},
        {"function", TokenType::KW_FUNCTION},
        {"true", TokenType::KW_TRUE},
        {"false", TokenType::KW_FALSE},
        {"switch", TokenType::KW_SWITCH},
        {"case", TokenType::KW_CASE},
        {"otherwise", TokenType::KW_OTHERWISE},
        {"try", TokenType::KW_TRY},
        {"catch", TokenType::KW_CATCH},
        {"global", TokenType::KW_GLOBAL},
        {"persistent", TokenType::KW_PERSISTENT},
    };

    auto it = keywords.find(word);
    if (it != keywords.end())
        addToken(it->second, word, startLine, startCol);
    else
        addToken(TokenType::IDENTIFIER, word, startLine, startCol);
}

bool Lexer::readOperator()
{
    char c = peek();
    char c2 = peek(1);

    int startLine = line_;
    int startCol = col_;

    auto twoChar = [&](TokenType t, const char *s) {
        addToken(t, s, startLine, startCol);
        advance();
        advance();
        return true;
    };
    auto oneChar = [&](TokenType t, const char *s) {
        addToken(t, s, startLine, startCol);
        advance();
        return true;
    };

    switch (c) {
    case '@':
        return oneChar(TokenType::AT, "@");
    case '.':
        if (c2 == '*')
            return twoChar(TokenType::DOT_STAR, ".*");
        if (c2 == '/')
            return twoChar(TokenType::DOT_SLASH, "./");
        if (c2 == '^')
            return twoChar(TokenType::DOT_CARET, ".^");
        if (c2 == '\'')
            return twoChar(TokenType::DOT_APOSTROPHE, ".'");
        return oneChar(TokenType::DOT, ".");
    case '+':
        return oneChar(TokenType::PLUS, "+");
    case '-':
        return oneChar(TokenType::MINUS, "-");
    case '*':
        return oneChar(TokenType::STAR, "*");
    case '/':
        return oneChar(TokenType::SLASH, "/");
    case '\\':
        return oneChar(TokenType::BACKSLASH, "\\");
    case '^':
        return oneChar(TokenType::CARET, "^");
    case '=':
        if (c2 == '=')
            return twoChar(TokenType::EQ, "==");
        return oneChar(TokenType::ASSIGN, "=");
    case '~':
        if (c2 == '=')
            return twoChar(TokenType::NEQ, "~=");
        return oneChar(TokenType::NOT, "~");
    case '<':
        if (c2 == '=')
            return twoChar(TokenType::LEQ, "<=");
        return oneChar(TokenType::LT, "<");
    case '>':
        if (c2 == '=')
            return twoChar(TokenType::GEQ, ">=");
        return oneChar(TokenType::GT, ">");
    case '&':
        if (c2 == '&')
            return twoChar(TokenType::AND_SHORT, "&&");
        return oneChar(TokenType::AND, "&");
    case '|':
        if (c2 == '|')
            return twoChar(TokenType::OR_SHORT, "||");
        return oneChar(TokenType::OR, "|");
    case '(':
        return oneChar(TokenType::LPAREN, "(");
    case ')':
        return oneChar(TokenType::RPAREN, ")");
    case '[':
        bracketDepth_++;
        return oneChar(TokenType::LBRACKET, "[");
    case ']':
        if (bracketDepth_ > 0)
            bracketDepth_--;
        return oneChar(TokenType::RBRACKET, "]");
    case '{':
        bracketDepth_++;
        return oneChar(TokenType::LBRACE, "{");
    case '}':
        if (bracketDepth_ > 0)
            bracketDepth_--;
        return oneChar(TokenType::RBRACE, "}");
    case ',':
        return oneChar(TokenType::COMMA, ",");
    case ';':
        return oneChar(TokenType::SEMICOLON, ";");
    case ':':
        return oneChar(TokenType::COLON, ":");
    }
    return false;
}

std::vector<Token> Lexer::tokenize()
{
    tokens_.clear();
    pos_ = 0;
    line_ = 1;
    col_ = 1;
    bracketDepth_ = 0;

    while (pos_ < src_.size()) {
        skipSpacesAndComments();
        if (pos_ >= src_.size())
            break;

        char c = peek();

        if (c == '\n') {
            int nl = line_;
            int nc = col_;

            // Внутри [] / {}: newline эквивалентен ; (разделитель строк матрицы)
            if (bracketDepth_ > 0) {
                // Вставляем ; только если предыдущий токен — значение
                if (!tokens_.empty() && isValueToken(tokens_.back().type)) {
                    addToken(TokenType::SEMICOLON, ";", nl, nc);
                }
            } else {
                addToken(TokenType::NEWLINE, "\\n", nl, nc);
            }
            advance(); // advance() обновит line_/col_
            continue;
        }

        if (std::isdigit(c)
            || (c == '.' && pos_ + 1 < src_.size() && std::isdigit(src_[pos_ + 1]))) {
            readNumber();
            continue;
        }

        if (c == '\'') {
            if (isTransposeContext()) {
                addToken(TokenType::APOSTROPHE, "'", line_, col_);
                advance();
                continue;
            }
            readString(line_, col_);
            continue;
        }

        if (c == '"') {
            readDoubleQuotedString(line_, col_);
            continue;
        }

        if (std::isalpha(c) || c == '_') {
            readIdentifier();
            continue;
        }

        if (readOperator())
            continue;

        throw std::runtime_error("Unexpected character '" + std::string(1, c) + "' at line "
                                 + std::to_string(line_) + " col " + std::to_string(col_));
    }

    addToken(TokenType::END_OF_INPUT, "", line_, col_);
    return tokens_;
}

} // namespace mlab
