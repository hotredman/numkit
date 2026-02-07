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
    col_++;
    return src_[pos_++];
}

void Lexer::addToken(TokenType type, const std::string &val)
{
    tokens_.push_back({type, val, line_, col_});
}

bool Lexer::isValueToken(TokenType t) const
{
    return t == TokenType::NUMBER || t == TokenType::IDENTIFIER || t == TokenType::RPAREN
           || t == TokenType::RBRACKET || t == TokenType::RBRACE || t == TokenType::APOSTROPHE
           || t == TokenType::DOT_APOSTROPHE || t == TokenType::KW_END || t == TokenType::KW_TRUE
           || t == TokenType::KW_FALSE || t == TokenType::STRING;
}

void Lexer::insertImplicitComma()
{
    if (bracketDepth_ <= 0 || tokens_.empty())
        return;
    auto prev = tokens_.back().type;
    if (!isValueToken(prev))
        return;

    // Сохраняем позицию и сканируем вперёд
    size_t scanPos = pos_;

    // Пропускаем пробелы/табы, запоминая были ли они
    size_t spacesStart = scanPos;
    while (scanPos < src_.size() && (src_[scanPos] == ' ' || src_[scanPos] == '\t'))
        scanPos++;

    if (scanPos >= src_.size())
        return;

    char next = src_[scanPos];

    // Если следующий символ — это оператор +/-, нужно определить:
    // это бинарный оператор или унарный знак?
    if (next == '+' || next == '-') {
        // Проверяем: есть ли пробел ПОСЛЕ знака перед следующим токеном?
        size_t afterSign = scanPos + 1;
        bool spaceAfterSign = false;
        while (afterSign < src_.size() && (src_[afterSign] == ' ' || src_[afterSign] == '\t')) {
            spaceAfterSign = true;
            afterSign++;
        }

        if (afterSign >= src_.size())
            return;

        char afterSignCh = src_[afterSign];
        bool nextIsValue = std::isdigit(afterSignCh) || std::isalpha(afterSignCh)
                           || afterSignCh == '_' || afterSignCh == '(' || afterSignCh == '['
                           || afterSignCh == '.';

        if (!nextIsValue)
            return;

        // Эвристика MATLAB:
        // "1 + 2"  (пробел-знак-пробел)  → бинарный оператор, НЕТ запятой
        // "1 +2"   (пробел-знак-нет)      → бинарный оператор, НЕТ запятой (MATLAB behaviour)
        // Но: "1 +2" в MATLAB тоже бинарный! Только если знак — это начало
        // нового выражения (что бывает при отсутствии пробела перед значением
        // и наличии пробела перед знаком), но MATLAB всегда трактует +/- как бинарный
        // внутри [], если слева есть значение.
        //
        // Поэтому: при +/- НИКОГДА не вставляем запятую — это всегда бинарный оператор.
        return;
    }

    // Для всех остальных символов: вставляем запятую если следующий — начало значения
    bool nextIsValue = std::isdigit(next) || std::isalpha(next) || next == '_' || next == '('
                       || next == '[' || next == '{' || next == '\'' || next == '"' || next == '~'
                       || next == '@' || next == '.';

    if (nextIsValue) {
        addToken(TokenType::COMMA, ",");
    }
}

void Lexer::skipSpacesAndComments()
{
    while (pos_ < src_.size()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') {
            if (bracketDepth_ > 0 && (c == ' ' || c == '\t')) {
                // Запоминаем что был пробел, пропускаем все пробелы,
                // затем решаем нужна ли запятая
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
            while (pos_ < src_.size() && peek() != '\n')
                advance();
            if (pos_ < src_.size()) {
                advance();
                line_++;
                col_ = 1;
            }
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
           || t == TokenType::IDENTIFIER || t == TokenType::NUMBER || t == TokenType::APOSTROPHE
           || t == TokenType::DOT_APOSTROPHE || t == TokenType::KW_END || t == TokenType::KW_TRUE
           || t == TokenType::KW_FALSE;
}

void Lexer::readNumber()
{
    size_t start = pos_;
    while (pos_ < src_.size() && std::isdigit(peek()))
        advance();
    if (pos_ < src_.size() && peek() == '.') {
        char next = peek(1);
        if (next != '*' && next != '/' && next != '^' && next != '\'' && next != '.') {
            advance();
            while (pos_ < src_.size() && std::isdigit(peek()))
                advance();
        }
    }
    if (pos_ < src_.size() && (peek() == 'e' || peek() == 'E')) {
        advance();
        if (pos_ < src_.size() && (peek() == '+' || peek() == '-'))
            advance();
        while (pos_ < src_.size() && std::isdigit(peek()))
            advance();
    }
    addToken(TokenType::NUMBER, src_.substr(start, pos_ - start));
}

void Lexer::readString()
{
    advance();
    std::string s;
    while (pos_ < src_.size()) {
        if (peek() == '\'') {
            if (peek(1) == '\'') {
                s += '\'';
                advance();
                advance();
            } else {
                advance();
                break;
            }
        } else {
            s += advance();
        }
    }
    addToken(TokenType::STRING, s);
}

void Lexer::readDoubleQuotedString()
{
    advance();
    std::string s;
    while (pos_ < src_.size() && peek() != '"') {
        if (peek() == '\\' && pos_ + 1 < src_.size()) {
            advance();
            char c = advance();
            switch (c) {
            case 'n':
                s += '\n';
                break;
            case 't':
                s += '\t';
                break;
            case '\\':
                s += '\\';
                break;
            case '"':
                s += '"';
                break;
            default:
                s += '\\';
                s += c;
                break;
            }
        } else {
            s += advance();
        }
    }
    if (pos_ < src_.size())
        advance();
    addToken(TokenType::STRING, s);
}

void Lexer::readIdentifier()
{
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
        addToken(it->second, word);
    else
        addToken(TokenType::IDENTIFIER, word);
}

bool Lexer::readOperator()
{
    char c = peek();
    char c2 = peek(1);
    auto twoChar = [&](TokenType t, const char *s) {
        addToken(t, s);
        advance();
        advance();
        return true;
    };
    auto oneChar = [&](TokenType t, const char *s) {
        addToken(t, s);
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
        bracketDepth_--;
        return oneChar(TokenType::RBRACKET, "]");
    case '{':
        return oneChar(TokenType::LBRACE, "{");
    case '}':
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
            addToken(TokenType::NEWLINE, "\\n");
            advance();
            line_++;
            col_ = 1;
            continue;
        }
        if (std::isdigit(c)
            || (c == '.' && pos_ + 1 < src_.size() && std::isdigit(src_[pos_ + 1]))) {
            readNumber();
            continue;
        }
        if (c == '\'') {
            if (isTransposeContext()) {
                addToken(TokenType::APOSTROPHE, "'");
                advance();
                continue;
            }
            readString();
            continue;
        }
        if (c == '"') {
            readDoubleQuotedString();
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

    addToken(TokenType::END_OF_INPUT, "");
    return tokens_;
}

} // namespace mlab
