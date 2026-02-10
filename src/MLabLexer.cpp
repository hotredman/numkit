#include "MLabLexer.hpp"

#include <stdexcept>
#include <unordered_map>

namespace mlab {

// ─── safe ctype wrappers (avoid UB with signed char > 127) ──────────────

bool Lexer::isDigit(char c)
{
    return std::isdigit(static_cast<unsigned char>(c));
}

bool Lexer::isAlpha(char c)
{
    return std::isalpha(static_cast<unsigned char>(c));
}

bool Lexer::isAlnum(char c)
{
    return std::isalnum(static_cast<unsigned char>(c));
}

bool Lexer::isXDigit(char c)
{
    return std::isxdigit(static_cast<unsigned char>(c));
}

// ─── error helpers ──────────────────────────────────────────────────────

void Lexer::error(const std::string &msg)
{
    throw std::runtime_error(msg + " at line " + std::to_string(line_) + " col "
                             + std::to_string(col_));
}

void Lexer::error(const std::string &msg, int line, int col)
{
    throw std::runtime_error(msg + " at line " + std::to_string(line) + " col "
                             + std::to_string(col));
}

// ─── constructor / basic methods ────────────────────────────────────────

Lexer::Lexer(const std::string &source)
    : src_(source)
{}

char Lexer::peek() const
{
    return pos_ < src_.size() ? src_[pos_] : '\0';
}

char Lexer::peek(int offset) const
{
    int p = static_cast<int>(pos_) + offset;
    if (p < 0 || static_cast<size_t>(p) >= src_.size())
        return '\0';
    return src_[static_cast<size_t>(p)];
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

// ─── bracket tracking ──────────────────────────────────────────────────

int Lexer::bracketDepth() const
{
    return static_cast<int>(bracketStack_.size());
}

bool Lexer::inMatrixContext() const
{
    return !bracketStack_.empty() && (bracketStack_.back() == '[' || bracketStack_.back() == '{');
}

char Lexer::closingFor(char open)
{
    switch (open) {
    case '(':
        return ')';
    case '[':
        return ']';
    case '{':
        return '}';
    default:
        return '?';
    }
}

void Lexer::pushBracket(char open)
{
    bracketStack_.push_back(open);
}

void Lexer::popBracket(char expected)
{
    if (bracketStack_.empty()) {
        error("Unexpected closing '" + std::string(1, expected) + "' without matching open");
    }
    char open = bracketStack_.back();
    char expectedClosing = closingFor(open);
    if (expected != expectedClosing) {
        error("Mismatched bracket: expected '" + std::string(1, expectedClosing) + "' but found '"
              + std::string(1, expected) + "'");
    }
    bracketStack_.pop_back();
}

// ─── context checks ────────────────────────────────────────────────────

bool Lexer::isValueToken(TokenType t) const
{
    switch (t) {
    case TokenType::NUMBER:
    case TokenType::IMAG_NUMBER:
    case TokenType::STRING:
    case TokenType::IDENTIFIER:
    case TokenType::RPAREN:
    case TokenType::RBRACKET:
    case TokenType::RBRACE:
    case TokenType::APOSTROPHE:
    case TokenType::DOT_APOSTROPHE:
    case TokenType::KW_END:
    case TokenType::KW_TRUE:
    case TokenType::KW_FALSE:
        return true;
    default:
        return false;
    }
}

bool Lexer::isTransposeContext() const
{
    if (tokens_.empty())
        return false;
    return isValueToken(tokens_.back().type);
}

// ─── implicit comma inside [] and {} ────────────────────────────────────

void Lexer::insertImplicitComma()
{
    // only inside [] and {}
    if (!inMatrixContext() || tokens_.empty())
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

    // +/- after a value token inside [] — binary operator, no comma
    if (next == '+' || next == '-')
        return;

    // dot can be start of number (.5) or dot-operator (.*, ./, .^, .')
    if (next == '.') {
        char afterDot = (scanPos + 1 < src_.size()) ? src_[scanPos + 1] : '\0';
        // only if digit follows dot — it's a number, insert comma
        if (!isDigit(afterDot))
            return;
    }

    bool nextIsValue = isDigit(next) || isAlpha(next) || next == '_' || next == '(' || next == '['
                       || next == '{' || next == '\'' || next == '"' || next == '~' || next == '@'
                       || next == '.';

    if (nextIsValue) {
        addToken(TokenType::COMMA, ",", line_, col_);
    }
}

// ─── block comment %{ ... %} ───────────────────────────────────────────

bool Lexer::isBlockCommentStart() const
{
    // %{ must be at the beginning of the line (only whitespace before %)
    if (peek() != '%' || peek(1) != '{')
        return false;

    // scan backwards to check if only whitespace before current pos on this line
    if (pos_ == 0)
        return true;

    for (size_t i = pos_; i > 0; i--) {
        char ch = src_[i - 1];
        if (ch == '\n')
            return true; // reached start of line — ok
        if (ch != ' ' && ch != '\t')
            return false; // non-whitespace before % — not a block comment
    }
    // reached start of file
    return true;
}

void Lexer::skipBlockComment()
{
    // called when pos_ is at '%' and peek(1) == '{'
    advance(); // '%'
    advance(); // '{'

    // skip rest of the %{ line
    while (pos_ < src_.size() && peek() != '\n')
        advance();
    if (pos_ < src_.size())
        advance(); // '\n'

    // nested block comments supported
    int depth = 1;
    while (pos_ < src_.size() && depth > 0) {
        if (peek() == '%' && peek(1) == '}') {
            depth--;
            advance(); // '%'
            advance(); // '}'
        } else if (peek() == '%' && peek(1) == '{') {
            depth++;
            advance(); // '%'
            advance(); // '{'
        } else {
            advance();
        }
    }

    if (depth > 0)
        error("Unterminated block comment");

    // skip rest of the %} line
    while (pos_ < src_.size() && peek() != '\n')
        advance();
}

// ─── skip spaces, comments, line continuation ──────────────────────────

void Lexer::skipSpacesAndComments()
{
    while (pos_ < src_.size()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') {
            if (inMatrixContext() && (c == ' ' || c == '\t')) {
                while (pos_ < src_.size() && (peek() == ' ' || peek() == '\t'))
                    advance();
                insertImplicitComma();
                continue;
            }
            advance();
        } else if (c == '%') {
            if (isBlockCommentStart()) {
                skipBlockComment();
            } else {
                // single-line comment
                while (pos_ < src_.size() && peek() != '\n')
                    advance();
            }
        } else if (c == '.' && peek(1) == '.' && peek(2) == '.') {
            // line continuation (...)
            while (pos_ < src_.size() && peek() != '\n')
                advance();
            // skip the newline itself — advance() updates line_/col_
            if (pos_ < src_.size())
                advance();
        } else {
            break;
        }
    }
}

// ─── underscore validation in numbers ───────────────────────────────────

void Lexer::validateUnderscores(size_t start, size_t end, int startLine, int startCol)
{
    if (start >= end)
        return;

    // no leading underscore in digit group
    if (src_[start] == '_')
        error("Number literal cannot start digit group with underscore", startLine, startCol);

    // no trailing underscore
    if (src_[end - 1] == '_')
        error("Number literal cannot end with underscore", startLine, startCol);

    // no consecutive underscores
    for (size_t i = start; i + 1 < end; i++) {
        if (src_[i] == '_' && src_[i + 1] == '_')
            error("Number literal cannot have consecutive underscores", startLine, startCol);
    }
}

// ─── read number ────────────────────────────────────────────────────────

void Lexer::readNumber()
{
    int startLine = line_;
    int startCol = col_;
    size_t start = pos_;
    bool hasDigits = false;

    // ── Hex: 0x... ──
    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
        advance(); // '0'
        advance(); // 'x'
        if (pos_ >= src_.size() || !isXDigit(peek()))
            error("Invalid hex literal", startLine, startCol);
        size_t digitStart = pos_;
        while (pos_ < src_.size() && (isXDigit(peek()) || peek() == '_')) {
            if (peek() != '_')
                hasDigits = true;
            advance();
        }
        validateUnderscores(digitStart, pos_, startLine, startCol);
        if (pos_ < src_.size() && (peek() == 'i' || peek() == 'j')) {
            char afterSuffix = peek(1);
            if (!isAlnum(afterSuffix) && afterSuffix != '_') {
                advance();
                addToken(TokenType::IMAG_NUMBER,
                         src_.substr(start, pos_ - start),
                         startLine,
                         startCol);
                return;
            }
        }
        addToken(TokenType::NUMBER, src_.substr(start, pos_ - start), startLine, startCol);
        return;
    }

    // ── Binary: 0b... ──
    if (peek() == '0' && (peek(1) == 'b' || peek(1) == 'B')) {
        advance(); // '0'
        advance(); // 'b'
        if (pos_ >= src_.size() || (peek() != '0' && peek() != '1'))
            error("Invalid binary literal", startLine, startCol);
        size_t digitStart = pos_;
        while (pos_ < src_.size() && (peek() == '0' || peek() == '1' || peek() == '_'))
            advance();
        validateUnderscores(digitStart, pos_, startLine, startCol);
        if (pos_ < src_.size() && (peek() == 'i' || peek() == 'j')) {
            char afterSuffix = peek(1);
            if (!isAlnum(afterSuffix) && afterSuffix != '_') {
                advance();
                addToken(TokenType::IMAG_NUMBER,
                         src_.substr(start, pos_ - start),
                         startLine,
                         startCol);
                return;
            }
        }
        addToken(TokenType::NUMBER, src_.substr(start, pos_ - start), startLine, startCol);
        return;
    }

    // ── Decimal / float ──

    // integer part (may be empty if number starts with '.')
    size_t intStart = pos_;
    while (pos_ < src_.size() && (isDigit(peek()) || peek() == '_')) {
        if (peek() != '_')
            hasDigits = true;
        advance();
    }
    if (pos_ > intStart)
        validateUnderscores(intStart, pos_, startLine, startCol);

    // fractional part
    if (pos_ < src_.size() && peek() == '.') {
        char next = peek(1);

        // don't consume dot if followed by dot-operator (.* ./ .^ .' .\ ..)
        bool isDotOperator = (next == '*' || next == '/' || next == '^' || next == '\''
                              || next == '\\' || next == '.');

        // don't consume dot if followed by letter (field access) — except e/E (exponent)
        bool isFieldAccess = (isAlpha(next) && next != 'e' && next != 'E') || next == '('
                             || next == '[';

        if (!isDotOperator && !isFieldAccess) {
            advance(); // '.'
            size_t fracStart = pos_;
            while (pos_ < src_.size() && (isDigit(peek()) || peek() == '_')) {
                if (peek() != '_')
                    hasDigits = true;
                advance();
            }
            if (pos_ > fracStart)
                validateUnderscores(fracStart, pos_, startLine, startCol);
        }
    }

    // must have at least one digit before exponent
    if (!hasDigits)
        error("Invalid number literal", startLine, startCol);

    // exponent
    if (pos_ < src_.size() && (peek() == 'e' || peek() == 'E')) {
        advance(); // 'e' / 'E'
        if (pos_ < src_.size() && (peek() == '+' || peek() == '-'))
            advance();
        if (pos_ >= src_.size() || !isDigit(peek()))
            error("Invalid number exponent", startLine, startCol);
        size_t expStart = pos_;
        while (pos_ < src_.size() && (isDigit(peek()) || peek() == '_'))
            advance();
        if (pos_ > expStart)
            validateUnderscores(expStart, pos_, startLine, startCol);
    }

    // imaginary suffix
    if (pos_ < src_.size() && (peek() == 'i' || peek() == 'j')) {
        char afterSuffix = peek(1);
        if (!isAlnum(afterSuffix) && afterSuffix != '_') {
            advance();
            addToken(TokenType::IMAG_NUMBER, src_.substr(start, pos_ - start), startLine, startCol);
            return;
        }
    }

    addToken(TokenType::NUMBER, src_.substr(start, pos_ - start), startLine, startCol);
}

// ─── read single-quoted string ──────────────────────────────────────────

void Lexer::readString(int startLine, int startCol)
{
    advance(); // skip opening '
    std::string s;
    while (pos_ < src_.size()) {
        if (peek() == '\'') {
            if (peek(1) == '\'') {
                // escape: '' → '
                s += '\'';
                advance();
                advance();
            } else {
                advance(); // closing '
                addToken(TokenType::STRING, s, startLine, startCol);
                return;
            }
        } else if (peek() == '\n') {
            error("Unterminated string literal", startLine, startCol);
        } else {
            s += advance();
        }
    }
    error("Unterminated string literal", startLine, startCol);
}

// ─── read double-quoted string (with escape sequences) ──────────────────

void Lexer::readDoubleQuotedString(int startLine, int startCol)
{
    advance(); // skip opening "
    std::string s;
    while (pos_ < src_.size()) {
        if (peek() == '"') {
            if (peek(1) == '"') {
                // MATLAB-style: "" → "
                s += '"';
                advance();
                advance();
            } else {
                advance(); // closing "
                addToken(TokenType::STRING, s, startLine, startCol);
                return;
            }
        } else if (peek() == '\\') {
            // escape sequences in double-quoted strings
            advance(); // skip backslash
            if (pos_ >= src_.size())
                error("Unterminated string literal", startLine, startCol);
            char esc = advance();
            switch (esc) {
            case 'n':
                s += '\n';
                break;
            case 't':
                s += '\t';
                break;
            case 'r':
                s += '\r';
                break;
            case '\\':
                s += '\\';
                break;
            case '"':
                s += '"';
                break;
            default:
                // unknown escape — keep as-is (MATLAB behavior)
                s += '\\';
                s += esc;
                break;
            }
        } else if (peek() == '\n') {
            error("Unterminated string literal", startLine, startCol);
        } else {
            s += advance();
        }
    }
    error("Unterminated string literal", startLine, startCol);
}

// ─── read identifier / keyword ──────────────────────────────────────────

void Lexer::readIdentifier()
{
    int startLine = line_;
    int startCol = col_;
    size_t start = pos_;

    while (pos_ < src_.size() && (isAlnum(peek()) || peek() == '_'))
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

// ─── read operators / punctuation ───────────────────────────────────────

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
        if (c2 == '\\')
            return twoChar(TokenType::DOT_BACKSLASH, ".\\");
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
        // TILDE: parser decides if it's NOT or ignore-output
        return oneChar(TokenType::TILDE, "~");
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
        pushBracket('(');
        return oneChar(TokenType::LPAREN, "(");
    case ')':
        popBracket(')');
        return oneChar(TokenType::RPAREN, ")");
    case '[':
        pushBracket('[');
        return oneChar(TokenType::LBRACKET, "[");
    case ']':
        popBracket(']');
        return oneChar(TokenType::RBRACKET, "]");
    case '{':
        pushBracket('{');
        return oneChar(TokenType::LBRACE, "{");
    case '}':
        popBracket('}');
        return oneChar(TokenType::RBRACE, "}");
    case ',':
        return oneChar(TokenType::COMMA, ",");
    case ';':
        return oneChar(TokenType::SEMICOLON, ";");
    case ':':
        return oneChar(TokenType::COLON, ":");
    default:
        return false;
    }
}

// ─── main tokenization loop ────────────────────────────────────────────

std::vector<Token> Lexer::tokenize()
{
    tokens_.clear();
    bracketStack_.clear();
    pos_ = 0;
    line_ = 1;
    col_ = 1;

    while (pos_ < src_.size()) {
        skipSpacesAndComments();
        if (pos_ >= src_.size())
            break;

        char c = peek();

        // ── Newline ──
        if (c == '\n') {
            int nl = line_;
            int nc = col_;

            if (bracketDepth() > 0) {
                if (inMatrixContext()) {
                    // inside [] or {}: newline = row separator (;)
                    if (!tokens_.empty() && isValueToken(tokens_.back().type)) {
                        addToken(TokenType::SEMICOLON, ";", nl, nc);
                    }
                }
                // inside (): newline silently ignored
            } else {
                addToken(TokenType::NEWLINE, "\\n", nl, nc);
            }
            advance();
            continue;
        }

        // ── Number (starts with digit or '.' before digit) ──
        if (isDigit(c) || (c == '.' && pos_ + 1 < src_.size() && isDigit(src_[pos_ + 1]))) {
            readNumber();
            continue;
        }

        // ── Single quote: string or transpose ──
        if (c == '\'') {
            if (isTransposeContext()) {
                addToken(TokenType::APOSTROPHE, "'", line_, col_);
                advance();
                continue;
            }
            readString(line_, col_);
            continue;
        }

        // ── Double quote: string ──
        if (c == '"') {
            readDoubleQuotedString(line_, col_);
            continue;
        }

        // ── Identifier / keyword ──
        if (isAlpha(c) || c == '_') {
            readIdentifier();
            continue;
        }

        // ── Operator / punctuation ──
        if (readOperator())
            continue;

        error("Unexpected character '" + std::string(1, c) + "'");
    }

    // check for unclosed brackets
    if (!bracketStack_.empty()) {
        error("Unclosed bracket '" + std::string(1, bracketStack_.back()) + "'");
    }

    addToken(TokenType::END_OF_INPUT, "", line_, col_);
    return tokens_;
}

} // namespace mlab
