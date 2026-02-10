#include "MLabLexer.hpp"

#include <stdexcept>
#include <unordered_map>

namespace mlab {

// ─── безопасные обёртки над ctype (избегаем UB при signed char > 127) ───

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

// ─── конструктор / базовые методы ───────────────────────────────────────

Lexer::Lexer(const std::string &source)
    : src_(source)
{}

char Lexer::peek() const
{
    return pos_ < src_.size() ? src_[pos_] : '\0';
}

char Lexer::peek(int offset) const
{
    size_t p = pos_ + static_cast<size_t>(offset);
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

// ─── контекстные проверки ───────────────────────────────────────────────

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

// ─── implicit comma внутри [] и {} ──────────────────────────────────────

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

    // +/- внутри [] после value-токена — бинарный оператор, не вставляем запятую
    if (next == '+' || next == '-')
        return;

    // Точка может быть началом числа (.5) или dot-оператором (.*, ./, .^, .')
    if (next == '.') {
        char afterDot = (scanPos + 1 < src_.size()) ? src_[scanPos + 1] : '\0';
        // Только если после точки идёт цифра — это число, нужна запятая
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

// ─── пропуск пробелов, комментариев, line continuation ─────────────────

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
            // Однострочный комментарий
            while (pos_ < src_.size() && peek() != '\n')
                advance();
        } else if (c == '.' && peek(1) == '.' && peek(2) == '.') {
            // Line continuation (...)
            while (pos_ < src_.size() && peek() != '\n')
                advance();
            // Пропускаем сам \n — advance() обновит line_/col_
            if (pos_ < src_.size())
                advance();
        } else {
            break;
        }
    }
}

// ─── чтение числа ──────────────────────────────────────────────────────

void Lexer::readNumber()
{
    int startLine = line_;
    int startCol = col_;
    size_t start = pos_;

    // ── Hex: 0x... ──
    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
        advance(); // '0'
        advance(); // 'x'
        if (pos_ >= src_.size() || !isXDigit(peek()))
            error("Invalid hex literal", startLine, startCol);
        while (pos_ < src_.size() && isXDigit(peek()))
            advance();
        if (pos_ < src_.size() && (peek() == 'i' || peek() == 'j')) {
            advance();
            addToken(TokenType::IMAG_NUMBER, src_.substr(start, pos_ - start), startLine, startCol);
        } else {
            addToken(TokenType::NUMBER, src_.substr(start, pos_ - start), startLine, startCol);
        }
        return;
    }

    // ── Binary: 0b... ──
    if (peek() == '0' && (peek(1) == 'b' || peek(1) == 'B')) {
        advance(); // '0'
        advance(); // 'b'
        if (pos_ >= src_.size() || (peek() != '0' && peek() != '1'))
            error("Invalid binary literal", startLine, startCol);
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

    // ── Decimal / float ──

    // Целая часть (может быть пустой если число начинается с '.')
    while (pos_ < src_.size() && isDigit(peek()))
        advance();

    // Дробная часть
    if (pos_ < src_.size() && peek() == '.') {
        char next = peek(1);

        // Не съедаем точку если за ней dot-оператор (.*  ./  .^  .')
        // или другая точка (..), но 'e'/'E' после точки — часть числа (1.e5)
        bool isDotOperator = (next == '*' || next == '/' || next == '^' || next == '\''
                              || next == '.');
        // Проверяем что после точки идёт не буква (кроме e/E — экспонента)
        // и не скобка — тогда точка принадлежит числу
        bool isFieldAccess = (isAlpha(next) && next != 'e' && next != 'E') || next == '('
                             || next == '[';

        if (!isDotOperator && !isFieldAccess) {
            advance(); // '.'
            while (pos_ < src_.size() && isDigit(peek()))
                advance();
        }
    }

    // Экспонента
    if (pos_ < src_.size() && (peek() == 'e' || peek() == 'E')) {
        // Убедимся что перед 'e' были цифры или точка (т.е. это число, а не идентификатор)
        // Это всегда так, т.к. readNumber вызывается только когда первый символ — цифра или '.'
        advance(); // 'e' / 'E'
        if (pos_ < src_.size() && (peek() == '+' || peek() == '-'))
            advance();
        if (pos_ >= src_.size() || !isDigit(peek()))
            error("Invalid number exponent", startLine, startCol);
        while (pos_ < src_.size() && isDigit(peek()))
            advance();
    }

    // Мнимый суффикс
    if (pos_ < src_.size() && (peek() == 'i' || peek() == 'j')) {
        // Убедимся что за i/j не идёт буква/цифра (иначе это идентификатор: 1if, ...)
        char afterSuffix = peek(1);
        if (!isAlnum(afterSuffix) && afterSuffix != '_') {
            advance();
            addToken(TokenType::IMAG_NUMBER, src_.substr(start, pos_ - start), startLine, startCol);
            return;
        }
    }

    std::string numStr = src_.substr(start, pos_ - start);
    if (numStr.empty() || numStr == ".")
        error("Invalid number literal", startLine, startCol);

    addToken(TokenType::NUMBER, numStr, startLine, startCol);
}

// ─── чтение строк ──────────────────────────────────────────────────────

void Lexer::readString(int startLine, int startCol)
{
    advance(); // пропускаем открывающий '
    std::string s;
    while (pos_ < src_.size()) {
        if (peek() == '\'') {
            if (peek(1) == '\'') {
                // Экранирование: '' → '
                s += '\'';
                advance();
                advance();
            } else {
                advance(); // закрывающий '
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
            error("Unterminated string literal", startLine, startCol);
        } else {
            s += advance();
        }
    }
    error("Unterminated string literal", startLine, startCol);
}

// ─── чтение идентификатора / ключевого слова ────────────────────────────

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

// ─── чтение операторов / пунктуации ─────────────────────────────────────

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
        // В контексте [~, b] = f() — это TILDE (игнорирование выхода)
        // В контексте ~x — это NOT (логическое отрицание)
        // Лексер выдаёт TILDE, парсер разберётся по контексту
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

// ─── основной цикл токенизации ──────────────────────────────────────────

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

        // ── Newline ──
        if (c == '\n') {
            int nl = line_;
            int nc = col_;

            if (bracketDepth_ > 0) {
                // Внутри [] / {}: newline ≡ разделитель строк матрицы (;)
                if (!tokens_.empty() && isValueToken(tokens_.back().type)) {
                    addToken(TokenType::SEMICOLON, ";", nl, nc);
                }
            } else {
                addToken(TokenType::NEWLINE, "\\n", nl, nc);
            }
            advance(); // advance() обновит line_/col_
            continue;
        }

        // ── Число (начинается с цифры или с '.' перед цифрой) ──
        if (isDigit(c) || (c == '.' && pos_ + 1 < src_.size() && isDigit(src_[pos_ + 1]))) {
            readNumber();
            continue;
        }

        // ── Одинарная кавычка: строка или транспонирование ──
        if (c == '\'') {
            if (isTransposeContext()) {
                addToken(TokenType::APOSTROPHE, "'", line_, col_);
                advance();
                continue;
            }
            readString(line_, col_);
            continue;
        }

        // ── Двойная кавычка: строка ──
        if (c == '"') {
            readDoubleQuotedString(line_, col_);
            continue;
        }

        // ── Идентификатор / ключевое слово ──
        if (isAlpha(c) || c == '_') {
            readIdentifier();
            continue;
        }

        // ── Оператор / пунктуация ──
        if (readOperator())
            continue;

        error("Unexpected character '" + std::string(1, c) + "'");
    }

    addToken(TokenType::END_OF_INPUT, "", line_, col_);
    return tokens_;
}

} // namespace mlab
