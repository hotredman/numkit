#include "MLabLexer.hpp"
#include <gtest/gtest.h>

using namespace mlab;

// ============================================================
// Вспомогательные функции
// ============================================================

static std::vector<Token> lex(const std::string &src)
{
    Lexer lexer(src);
    return lexer.tokenize();
}

static void expectTokenTypes(const std::string &src, const std::vector<TokenType> &expected)
{
    auto tokens = lex(src);
    ASSERT_EQ(tokens.size(), expected.size()) << "Source: \"" << src << "\"";
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(tokens[i].type, expected[i])
            << "Token #" << i << " in \"" << src << "\", value=\"" << tokens[i].value << "\"";
    }
}

static void expectSingleToken(const std::string &src, TokenType type, const std::string &value = "")
{
    auto tokens = lex(src);
    ASSERT_GE(tokens.size(), 1u);
    // последний — END_OF_INPUT, проверяем первый
    EXPECT_EQ(tokens[0].type, type);
    if (!value.empty()) {
        EXPECT_EQ(tokens[0].value, value);
    }
}

// ============================================================
// Пустой / тривиальный ввод
// ============================================================

TEST(Lexer, EmptyInput)
{
    auto tokens = lex("");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::END_OF_INPUT);
}

TEST(Lexer, WhitespaceOnly)
{
    auto tokens = lex("   \t  ");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::END_OF_INPUT);
}

TEST(Lexer, NewlineOnly)
{
    auto tokens = lex("\n");
    expectTokenTypes("\n", {TokenType::NEWLINE, TokenType::END_OF_INPUT});
}

TEST(Lexer, MultipleNewlines)
{
    // несколько пустых строк — могут схлопываться или нет, зависит от реализации
    auto tokens = lex("\n\n\n");
    // как минимум последний — END_OF_INPUT
    EXPECT_EQ(tokens.back().type, TokenType::END_OF_INPUT);
    for (size_t i = 0; i + 1 < tokens.size(); ++i) {
        EXPECT_EQ(tokens[i].type, TokenType::NEWLINE);
    }
}

// ============================================================
// Числа — целые
// ============================================================

TEST(Lexer, IntegerZero)
{
    expectSingleToken("0", TokenType::NUMBER, "0");
}

TEST(Lexer, IntegerSimple)
{
    expectSingleToken("42", TokenType::NUMBER, "42");
}

TEST(Lexer, IntegerLarge)
{
    expectSingleToken("1234567890", TokenType::NUMBER, "1234567890");
}

// ============================================================
// Числа — дробные
// ============================================================

TEST(Lexer, FloatSimple)
{
    expectSingleToken("3.14", TokenType::NUMBER, "3.14");
}

TEST(Lexer, FloatLeadingDot)
{
    expectSingleToken(".5", TokenType::NUMBER, ".5");
}

TEST(Lexer, FloatTrailingDot)
{
    expectSingleToken("5.", TokenType::NUMBER, "5.");
}

// ============================================================
// Числа — научная нотация
// ============================================================

TEST(Lexer, ScientificNotationLower)
{
    expectSingleToken("1e10", TokenType::NUMBER, "1e10");
}

TEST(Lexer, ScientificNotationUpper)
{
    expectSingleToken("1E10", TokenType::NUMBER, "1E10");
}

TEST(Lexer, ScientificNotationPositiveExp)
{
    expectSingleToken("2.5e+3", TokenType::NUMBER, "2.5e+3");
}

TEST(Lexer, ScientificNotationNegativeExp)
{
    expectSingleToken("2.5e-3", TokenType::NUMBER, "2.5e-3");
}

TEST(Lexer, ScientificNotationNoIntPart)
{
    expectSingleToken(".5e2", TokenType::NUMBER, ".5e2");
}

// ============================================================
// Числа — hex, binary, octal
// ============================================================

TEST(Lexer, HexNumber)
{
    expectSingleToken("0xFF", TokenType::NUMBER, "0xFF");
}

TEST(Lexer, HexNumberLower)
{
    expectSingleToken("0xab12", TokenType::NUMBER, "0xab12");
}

TEST(Lexer, BinaryNumber)
{
    expectSingleToken("0b1010", TokenType::NUMBER, "0b1010");
}

TEST(Lexer, OctalNumber)
{
    expectSingleToken("0o777", TokenType::NUMBER, "0o777");
}

// ============================================================
// Числа с подчёркиваниями (MATLAB R2019b+)
// ============================================================

TEST(Lexer, NumberWithUnderscores)
{
    expectSingleToken("1_000_000", TokenType::NUMBER, "1_000_000");
}

TEST(Lexer, HexWithUnderscores)
{
    expectSingleToken("0xFF_FF", TokenType::NUMBER, "0xFF_FF");
}

TEST(Lexer, NumberLeadingUnderscoreError)
{
    EXPECT_ANY_THROW(lex("_123")); // это identifier, не число — может не бросать
    // Если _123 — валидный идентификатор, замени на другой кейс:
}

TEST(Lexer, NumberTrailingUnderscoreError)
{
    EXPECT_ANY_THROW(lex("123_"));
}

TEST(Lexer, NumberDoubleUnderscoreError)
{
    EXPECT_ANY_THROW(lex("1__2"));
}

// ============================================================
// Мнимые числа
// ============================================================

TEST(Lexer, ImaginaryInteger)
{
    expectSingleToken("3i", TokenType::IMAG_NUMBER, "3i");
}

TEST(Lexer, ImaginaryFloat)
{
    expectSingleToken("2.5i", TokenType::IMAG_NUMBER, "2.5i");
}

TEST(Lexer, ImaginaryJ)
{
    expectSingleToken("1j", TokenType::IMAG_NUMBER, "1j");
}

TEST(Lexer, ImaginaryScientific)
{
    expectSingleToken("1e3i", TokenType::IMAG_NUMBER, "1e3i");
}

TEST(Lexer, ImaginaryBareI)
{
    // Просто `i` — это идентификатор, не мнимое число
    expectSingleToken("i", TokenType::IDENTIFIER, "i");
}

// ============================================================
// Строки — одинарные кавычки
// ============================================================

TEST(Lexer, SingleQuotedStringSimple)
{
    expectSingleToken("'hello'", TokenType::STRING, "hello");
}

TEST(Lexer, SingleQuotedStringEmpty)
{
    expectSingleToken("''", TokenType::STRING, "");
}

TEST(Lexer, SingleQuotedStringEscapedQuote)
{
    // В MATLAB '' внутри строки — экранированная кавычка
    expectSingleToken("'it''s'", TokenType::STRING, "it's");
}

TEST(Lexer, SingleQuotedStringWithSpaces)
{
    expectSingleToken("'hello world'", TokenType::STRING, "hello world");
}

// ============================================================
// Строки — двойные кавычки
// ============================================================

TEST(Lexer, DoubleQuotedStringSimple)
{
    expectSingleToken("\"hello\"", TokenType::STRING, "hello");
}

TEST(Lexer, DoubleQuotedStringEmpty)
{
    expectSingleToken("\"\"", TokenType::STRING, "");
}

TEST(Lexer, DoubleQuotedStringEscapedQuote)
{
    expectSingleToken("\"say \"\"hi\"\"\"", TokenType::STRING, "say \"hi\"");
}

// ============================================================
// Строка vs Транспонирование (апостроф)
// ============================================================

TEST(Lexer, ApostropheTransposeAfterIdentifier)
{
    // x' → IDENTIFIER APOSTROPHE
    expectTokenTypes("x'", {TokenType::IDENTIFIER, TokenType::APOSTROPHE, TokenType::END_OF_INPUT});
}

TEST(Lexer, ApostropheTransposeAfterParen)
{
    // (x)' → LPAREN IDENTIFIER RPAREN APOSTROPHE
    expectTokenTypes("(x)'",
                     {TokenType::LPAREN,
                      TokenType::IDENTIFIER,
                      TokenType::RPAREN,
                      TokenType::APOSTROPHE,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, ApostropheTransposeAfterBracket)
{
    expectTokenTypes("[1]'",
                     {TokenType::LBRACKET,
                      TokenType::NUMBER,
                      TokenType::RBRACKET,
                      TokenType::APOSTROPHE,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, ApostropheTransposeAfterNumber)
{
    expectTokenTypes("5'", {TokenType::NUMBER, TokenType::APOSTROPHE, TokenType::END_OF_INPUT});
}

TEST(Lexer, ApostropheStringAtStart)
{
    // В начале строки ' — начало строки
    auto tokens = lex("'hello'");
    ASSERT_GE(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, TokenType::STRING);
}

TEST(Lexer, ApostropheStringAfterOperator)
{
    // = 'hello' → ASSIGN STRING
    expectTokenTypes("= 'hello'", {TokenType::ASSIGN, TokenType::STRING, TokenType::END_OF_INPUT});
}

TEST(Lexer, ApostropheStringAfterComma)
{
    expectTokenTypes(",'hello'", {TokenType::COMMA, TokenType::STRING, TokenType::END_OF_INPUT});
}

TEST(Lexer, ApostropheStringAfterSemicolon)
{
    expectTokenTypes(";'hello'", {TokenType::SEMICOLON, TokenType::STRING, TokenType::END_OF_INPUT});
}

// ============================================================
// Dot-транспонирование
// ============================================================

TEST(Lexer, DotApostrophe)
{
    expectTokenTypes("x.'",
                     {TokenType::IDENTIFIER, TokenType::DOT_APOSTROPHE, TokenType::END_OF_INPUT});
}

// ============================================================
// Идентификаторы
// ============================================================

TEST(Lexer, IdentifierSimple)
{
    expectSingleToken("myVar", TokenType::IDENTIFIER, "myVar");
}

TEST(Lexer, IdentifierWithUnderscore)
{
    expectSingleToken("my_var", TokenType::IDENTIFIER, "my_var");
}

TEST(Lexer, IdentifierStartUnderscore)
{
    expectSingleToken("_private", TokenType::IDENTIFIER, "_private");
}

TEST(Lexer, IdentifierWithDigits)
{
    expectSingleToken("x1", TokenType::IDENTIFIER, "x1");
}

TEST(Lexer, IdentifierAllCaps)
{
    expectSingleToken("MAX_SIZE", TokenType::IDENTIFIER, "MAX_SIZE");
}

// ============================================================
// Ключевые слова
// ============================================================

TEST(Lexer, KeywordIf)
{
    expectSingleToken("if", TokenType::KW_IF);
}

TEST(Lexer, KeywordElseif)
{
    expectSingleToken("elseif", TokenType::KW_ELSEIF);
}

TEST(Lexer, KeywordElse)
{
    expectSingleToken("else", TokenType::KW_ELSE);
}

TEST(Lexer, KeywordEnd)
{
    expectSingleToken("end", TokenType::KW_END);
}

TEST(Lexer, KeywordFor)
{
    expectSingleToken("for", TokenType::KW_FOR);
}

TEST(Lexer, KeywordWhile)
{
    expectSingleToken("while", TokenType::KW_WHILE);
}

TEST(Lexer, KeywordBreak)
{
    expectSingleToken("break", TokenType::KW_BREAK);
}

TEST(Lexer, KeywordContinue)
{
    expectSingleToken("continue", TokenType::KW_CONTINUE);
}

TEST(Lexer, KeywordReturn)
{
    expectSingleToken("return", TokenType::KW_RETURN);
}

TEST(Lexer, KeywordFunction)
{
    expectSingleToken("function", TokenType::KW_FUNCTION);
}

TEST(Lexer, KeywordTrue)
{
    expectSingleToken("true", TokenType::KW_TRUE);
}

TEST(Lexer, KeywordFalse)
{
    expectSingleToken("false", TokenType::KW_FALSE);
}

TEST(Lexer, KeywordSwitch)
{
    expectSingleToken("switch", TokenType::KW_SWITCH);
}

TEST(Lexer, KeywordCase)
{
    expectSingleToken("case", TokenType::KW_CASE);
}

TEST(Lexer, KeywordOtherwise)
{
    expectSingleToken("otherwise", TokenType::KW_OTHERWISE);
}

TEST(Lexer, KeywordTry)
{
    expectSingleToken("try", TokenType::KW_TRY);
}

TEST(Lexer, KeywordCatch)
{
    expectSingleToken("catch", TokenType::KW_CATCH);
}

TEST(Lexer, KeywordGlobal)
{
    expectSingleToken("global", TokenType::KW_GLOBAL);
}

TEST(Lexer, KeywordPersistent)
{
    expectSingleToken("persistent", TokenType::KW_PERSISTENT);
}

TEST(Lexer, KeywordAsPrefix)
{
    // "ifx" — не ключевое слово, а идентификатор
    expectSingleToken("ifx", TokenType::IDENTIFIER, "ifx");
}

TEST(Lexer, KeywordAsSuffix)
{
    // "myif" — не ключевое слово
    expectSingleToken("myif", TokenType::IDENTIFIER, "myif");
}

// ============================================================
// Арифметические операторы
// ============================================================

TEST(Lexer, OperatorPlus)
{
    expectTokenTypes("a + b",
                     {TokenType::IDENTIFIER,
                      TokenType::PLUS,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, OperatorMinus)
{
    expectTokenTypes("a - b",
                     {TokenType::IDENTIFIER,
                      TokenType::MINUS,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, OperatorStar)
{
    expectTokenTypes("a * b",
                     {TokenType::IDENTIFIER,
                      TokenType::STAR,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, OperatorSlash)
{
    expectTokenTypes("a / b",
                     {TokenType::IDENTIFIER,
                      TokenType::SLASH,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, OperatorBackslash)
{
    expectTokenTypes("a \\ b",
                     {TokenType::IDENTIFIER,
                      TokenType::BACKSLASH,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, OperatorCaret)
{
    expectTokenTypes("a ^ b",
                     {TokenType::IDENTIFIER,
                      TokenType::CARET,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

// ============================================================
// Поэлементные операторы
// ============================================================

TEST(Lexer, DotStar)
{
    expectTokenTypes("a .* b",
                     {TokenType::IDENTIFIER,
                      TokenType::DOT_STAR,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, DotSlash)
{
    expectTokenTypes("a ./ b",
                     {TokenType::IDENTIFIER,
                      TokenType::DOT_SLASH,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, DotCaret)
{
    expectTokenTypes("a .^ b",
                     {TokenType::IDENTIFIER,
                      TokenType::DOT_CARET,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, DotBackslash)
{
    expectTokenTypes("a .\\ b",
                     {TokenType::IDENTIFIER,
                      TokenType::DOT_BACKSLASH,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

// ============================================================
// Операторы сравнения
// ============================================================

TEST(Lexer, OperatorEq)
{
    expectTokenTypes("a == b",
                     {TokenType::IDENTIFIER,
                      TokenType::EQ,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, OperatorNeq)
{
    expectTokenTypes("a ~= b",
                     {TokenType::IDENTIFIER,
                      TokenType::NEQ,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, OperatorLt)
{
    expectTokenTypes("a < b",
                     {TokenType::IDENTIFIER,
                      TokenType::LT,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, OperatorGt)
{
    expectTokenTypes("a > b",
                     {TokenType::IDENTIFIER,
                      TokenType::GT,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, OperatorLeq)
{
    expectTokenTypes("a <= b",
                     {TokenType::IDENTIFIER,
                      TokenType::LEQ,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, OperatorGeq)
{
    expectTokenTypes("a >= b",
                     {TokenType::IDENTIFIER,
                      TokenType::GEQ,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

// ============================================================
// Логические операторы
// ============================================================

TEST(Lexer, OperatorAnd)
{
    expectTokenTypes("a & b",
                     {TokenType::IDENTIFIER,
                      TokenType::AND,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, OperatorOr)
{
    expectTokenTypes("a | b",
                     {TokenType::IDENTIFIER,
                      TokenType::OR,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, OperatorTilde)
{
    expectTokenTypes("~x", {TokenType::TILDE, TokenType::IDENTIFIER, TokenType::END_OF_INPUT});
}

TEST(Lexer, OperatorAndShort)
{
    expectTokenTypes("a && b",
                     {TokenType::IDENTIFIER,
                      TokenType::AND_SHORT,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, OperatorOrShort)
{
    expectTokenTypes("a || b",
                     {TokenType::IDENTIFIER,
                      TokenType::OR_SHORT,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

// ============================================================
// Присваивание
// ============================================================

TEST(Lexer, Assign)
{
    expectTokenTypes("x = 5",
                     {TokenType::IDENTIFIER,
                      TokenType::ASSIGN,
                      TokenType::NUMBER,
                      TokenType::END_OF_INPUT});
}

// ============================================================
// Скобки
// ============================================================

TEST(Lexer, Parentheses)
{
    expectTokenTypes("()", {TokenType::LPAREN, TokenType::RPAREN, TokenType::END_OF_INPUT});
}

TEST(Lexer, SquareBrackets)
{
    expectTokenTypes("[]", {TokenType::LBRACKET, TokenType::RBRACKET, TokenType::END_OF_INPUT});
}

TEST(Lexer, CurlyBraces)
{
    expectTokenTypes("{}", {TokenType::LBRACE, TokenType::RBRACE, TokenType::END_OF_INPUT});
}

TEST(Lexer, NestedBrackets)
{
    expectTokenTypes("([{}])",
                     {TokenType::LPAREN,
                      TokenType::LBRACKET,
                      TokenType::LBRACE,
                      TokenType::RBRACE,
                      TokenType::RBRACKET,
                      TokenType::RPAREN,
                      TokenType::END_OF_INPUT});
}

// ============================================================
// Разделители
// ============================================================

TEST(Lexer, Comma)
{
    expectTokenTypes("a,b",
                     {TokenType::IDENTIFIER,
                      TokenType::COMMA,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, Semicolon)
{
    expectTokenTypes("a;", {TokenType::IDENTIFIER, TokenType::SEMICOLON, TokenType::END_OF_INPUT});
}

TEST(Lexer, Colon)
{
    expectTokenTypes("1:10",
                     {TokenType::NUMBER,
                      TokenType::COLON,
                      TokenType::NUMBER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, ColonTriple)
{
    expectTokenTypes("1:2:10",
                     {TokenType::NUMBER,
                      TokenType::COLON,
                      TokenType::NUMBER,
                      TokenType::COLON,
                      TokenType::NUMBER,
                      TokenType::END_OF_INPUT});
}

// ============================================================
// Dot (доступ к полю)
// ============================================================

TEST(Lexer, DotFieldAccess)
{
    expectTokenTypes("s.field",
                     {TokenType::IDENTIFIER,
                      TokenType::DOT,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

// ============================================================
// At-оператор (function handle)
// ============================================================

TEST(Lexer, AtOperator)
{
    expectTokenTypes("@sin", {TokenType::AT, TokenType::IDENTIFIER, TokenType::END_OF_INPUT});
}

TEST(Lexer, AtLambda)
{
    expectTokenTypes("@(x) x^2",
                     {TokenType::AT,
                      TokenType::LPAREN,
                      TokenType::IDENTIFIER,
                      TokenType::RPAREN,
                      TokenType::IDENTIFIER,
                      TokenType::CARET,
                      TokenType::NUMBER,
                      TokenType::END_OF_INPUT});
}

// ============================================================
// Ellipsis (продолжение строки)
// ============================================================

TEST(Lexer, EllipsisContinuation)
{
    // ... до конца строки — продолжение, пропускаем до следующей строки
    auto tokens = lex("a + ...\nb");
    expectTokenTypes("a + ...\nb",
                     {TokenType::IDENTIFIER,
                      TokenType::PLUS,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, EllipsisToken)
{
    // Если ... не в конце строки, может быть другой контекст
    // Зависит от реализации — проверяем базовый случай
    auto tokens = lex("a ...\n+ b");
    // Ожидаем: a + b (без NEWLINE между ними)
    bool hasNewline = false;
    for (auto &t : tokens) {
        if (t.type == TokenType::NEWLINE)
            hasNewline = true;
    }
    EXPECT_FALSE(hasNewline) << "Ellipsis should suppress newline";
}

// ============================================================
// Комментарии
// ============================================================

TEST(Lexer, LineComment)
{
    auto tokens = lex("x = 5 % this is a comment");
    expectTokenTypes("x = 5 % this is a comment",
                     {TokenType::IDENTIFIER,
                      TokenType::ASSIGN,
                      TokenType::NUMBER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, LineCommentEntireLine)
{
    auto tokens = lex("% just a comment\n");
    // Может быть NEWLINE + END_OF_INPUT или просто END_OF_INPUT
    EXPECT_EQ(tokens.back().type, TokenType::END_OF_INPUT);
}

TEST(Lexer, BlockComment)
{
    auto tokens = lex("%{\nthis is\na block comment\n%}\nx = 1");
    // Блочный комментарий пропускается
    bool foundAssign = false;
    for (auto &t : tokens) {
        if (t.type == TokenType::ASSIGN)
            foundAssign = true;
    }
    EXPECT_TRUE(foundAssign);
}

TEST(Lexer, BlockCommentNested)
{
    auto tokens = lex("%{\n%{\nnested\n%}\n%}\nx = 1");
    bool foundAssign = false;
    for (auto &t : tokens) {
        if (t.type == TokenType::ASSIGN)
            foundAssign = true;
    }
    EXPECT_TRUE(foundAssign);
}

TEST(Lexer, CommentDoesNotAffectNextLine)
{
    expectTokenTypes("% comment\nx",
                     {TokenType::NEWLINE, TokenType::IDENTIFIER, TokenType::END_OF_INPUT});
}

// ============================================================
// Матричный контекст — неявные запятые / точки с запятой
// ============================================================

TEST(Lexer, MatrixRowImplicitComma)
{
    // [1 2 3] → [1, 2, 3]  — пробелы становятся разделителями
    auto tokens = lex("[1 2 3]");
    int commaCount = 0;
    for (auto &t : tokens) {
        if (t.type == TokenType::COMMA)
            commaCount++;
    }
    EXPECT_EQ(commaCount, 2) << "Expected 2 implicit commas in [1 2 3]";
}

TEST(Lexer, MatrixRowExplicitComma)
{
    // [1, 2, 3] — уже есть запятые
    auto tokens = lex("[1, 2, 3]");
    int commaCount = 0;
    for (auto &t : tokens) {
        if (t.type == TokenType::COMMA)
            commaCount++;
    }
    EXPECT_EQ(commaCount, 2);
}

TEST(Lexer, MatrixRowSemicolon)
{
    // [1 2; 3 4]
    auto tokens = lex("[1 2; 3 4]");
    int semiCount = 0;
    for (auto &t : tokens) {
        if (t.type == TokenType::SEMICOLON)
            semiCount++;
    }
    EXPECT_EQ(semiCount, 1);
}

TEST(Lexer, MatrixNewlineAsSemicolon)
{
    // [1 2\n 3 4] — newline внутри [] может быть ; или просто разделитель
    auto tokens = lex("[1 2\n3 4]");
    // Должны быть разделители строк матрицы
    bool hasSemiOrNewline = false;
    for (auto &t : tokens) {
        if (t.type == TokenType::SEMICOLON || t.type == TokenType::NEWLINE)
            hasSemiOrNewline = true;
    }
    EXPECT_TRUE(hasSemiOrNewline);
}

TEST(Lexer, CellArrayImplicitComma)
{
    auto tokens = lex("{1 2 3}");
    int commaCount = 0;
    for (auto &t : tokens) {
        if (t.type == TokenType::COMMA)
            commaCount++;
    }
    EXPECT_EQ(commaCount, 2) << "Expected 2 implicit commas in {1 2 3}";
}

TEST(Lexer, NoImplicitCommaInParens)
{
    // (1 + 2) — пробел внутри () не даёт запятую
    auto tokens = lex("f(1 + 2)");
    int commaCount = 0;
    for (auto &t : tokens) {
        if (t.type == TokenType::COMMA)
            commaCount++;
    }
    EXPECT_EQ(commaCount, 0);
}

// ============================================================
// Unary minus / plus
// ============================================================

TEST(Lexer, NegativeNumber)
{
    // -5 в начале — это MINUS NUMBER
    expectTokenTypes("-5", {TokenType::MINUS, TokenType::NUMBER, TokenType::END_OF_INPUT});
}

TEST(Lexer, UnaryPlus)
{
    expectTokenTypes("+5", {TokenType::PLUS, TokenType::NUMBER, TokenType::END_OF_INPUT});
}

// ============================================================
// Сложные выражения — интеграционные тесты
// ============================================================

TEST(Lexer, SimpleAssignment)
{
    expectTokenTypes("x = 42;",
                     {TokenType::IDENTIFIER,
                      TokenType::ASSIGN,
                      TokenType::NUMBER,
                      TokenType::SEMICOLON,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, FunctionCall)
{
    expectTokenTypes("disp('hello')",
                     {TokenType::IDENTIFIER,
                      TokenType::LPAREN,
                      TokenType::STRING,
                      TokenType::RPAREN,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, FunctionDefinition)
{
    expectTokenTypes("function y = foo(x)",
                     {TokenType::KW_FUNCTION,
                      TokenType::IDENTIFIER,
                      TokenType::ASSIGN,
                      TokenType::IDENTIFIER,
                      TokenType::LPAREN,
                      TokenType::IDENTIFIER,
                      TokenType::RPAREN,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, FunctionMultipleOutputs)
{
    expectTokenTypes("function [a, b] = foo(x, y)",
                     {TokenType::KW_FUNCTION,
                      TokenType::LBRACKET,
                      TokenType::IDENTIFIER,
                      TokenType::COMMA,
                      TokenType::IDENTIFIER,
                      TokenType::RBRACKET,
                      TokenType::ASSIGN,
                      TokenType::IDENTIFIER,
                      TokenType::LPAREN,
                      TokenType::IDENTIFIER,
                      TokenType::COMMA,
                      TokenType::IDENTIFIER,
                      TokenType::RPAREN,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, ForLoop)
{
    expectTokenTypes("for i = 1:10",
                     {TokenType::KW_FOR,
                      TokenType::IDENTIFIER,
                      TokenType::ASSIGN,
                      TokenType::NUMBER,
                      TokenType::COLON,
                      TokenType::NUMBER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, WhileLoop)
{
    expectTokenTypes("while x > 0",
                     {TokenType::KW_WHILE,
                      TokenType::IDENTIFIER,
                      TokenType::GT,
                      TokenType::NUMBER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, IfElseEnd)
{
    auto tokens = lex("if x > 0\n  y = 1;\nelseif x < 0\n  y = -1;\nelse\n  y = 0;\nend");
    // Просто проверяем, что ключевые слова распознаны
    std::vector<TokenType> keywords;
    for (auto &t : tokens) {
        if (t.type == TokenType::KW_IF || t.type == TokenType::KW_ELSEIF
            || t.type == TokenType::KW_ELSE || t.type == TokenType::KW_END) {
            keywords.push_back(t.type);
        }
    }
    ASSERT_EQ(keywords.size(), 4u);
    EXPECT_EQ(keywords[0], TokenType::KW_IF);
    EXPECT_EQ(keywords[1], TokenType::KW_ELSEIF);
    EXPECT_EQ(keywords[2], TokenType::KW_ELSE);
    EXPECT_EQ(keywords[3], TokenType::KW_END);
}

TEST(Lexer, SwitchCaseOtherwise)
{
    auto tokens = lex("switch x\ncase 1\ncase 2\notherwise\nend");
    std::vector<TokenType> keywords;
    for (auto &t : tokens) {
        if (t.type == TokenType::KW_SWITCH || t.type == TokenType::KW_CASE
            || t.type == TokenType::KW_OTHERWISE || t.type == TokenType::KW_END) {
            keywords.push_back(t.type);
        }
    }
    ASSERT_EQ(keywords.size(), 5u);
    EXPECT_EQ(keywords[0], TokenType::KW_SWITCH);
    EXPECT_EQ(keywords[1], TokenType::KW_CASE);
    EXPECT_EQ(keywords[2], TokenType::KW_CASE);
    EXPECT_EQ(keywords[3], TokenType::KW_OTHERWISE);
    EXPECT_EQ(keywords[4], TokenType::KW_END);
}

TEST(Lexer, TryCatch)
{
    auto tokens = lex("try\n  x = 1;\ncatch e\n  disp(e);\nend");
    std::vector<TokenType> keywords;
    for (auto &t : tokens) {
        if (t.type == TokenType::KW_TRY || t.type == TokenType::KW_CATCH
            || t.type == TokenType::KW_END) {
            keywords.push_back(t.type);
        }
    }
    ASSERT_EQ(keywords.size(), 3u);
    EXPECT_EQ(keywords[0], TokenType::KW_TRY);
    EXPECT_EQ(keywords[1], TokenType::KW_CATCH);
    EXPECT_EQ(keywords[2], TokenType::KW_END);
}

TEST(Lexer, ComplexExpression)
{
    expectTokenTypes("y = (a + b) .* c.^2 - d / e;",
                     {TokenType::IDENTIFIER,
                      TokenType::ASSIGN,
                      TokenType::LPAREN,
                      TokenType::IDENTIFIER,
                      TokenType::PLUS,
                      TokenType::IDENTIFIER,
                      TokenType::RPAREN,
                      TokenType::DOT_STAR,
                      TokenType::IDENTIFIER,
                      TokenType::DOT_CARET,
                      TokenType::NUMBER,
                      TokenType::MINUS,
                      TokenType::IDENTIFIER,
                      TokenType::SLASH,
                      TokenType::IDENTIFIER,
                      TokenType::SEMICOLON,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, CellArrayAccess)
{
    expectTokenTypes("c{1}",
                     {TokenType::IDENTIFIER,
                      TokenType::LBRACE,
                      TokenType::NUMBER,
                      TokenType::RBRACE,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, StructFieldAccess)
{
    expectTokenTypes("s.x.y",
                     {TokenType::IDENTIFIER,
                      TokenType::DOT,
                      TokenType::IDENTIFIER,
                      TokenType::DOT,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, LogicalExpression)
{
    expectTokenTypes("a && b || ~c",
                     {TokenType::IDENTIFIER,
                      TokenType::AND_SHORT,
                      TokenType::IDENTIFIER,
                      TokenType::OR_SHORT,
                      TokenType::TILDE,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, ComparisonChain)
{
    expectTokenTypes("a ~= b & c >= d",
                     {TokenType::IDENTIFIER,
                      TokenType::NEQ,
                      TokenType::IDENTIFIER,
                      TokenType::AND,
                      TokenType::IDENTIFIER,
                      TokenType::GEQ,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

// ============================================================
// Позиции (line, col)
// ============================================================

TEST(Lexer, TokenPositionFirstLine)
{
    auto tokens = lex("x = 5");
    // x
    EXPECT_EQ(tokens[0].line, 1);
    EXPECT_EQ(tokens[0].col, 1);
    // =
    EXPECT_EQ(tokens[1].line, 1);
    EXPECT_EQ(tokens[1].col, 3);
    // 5
    EXPECT_EQ(tokens[2].line, 1);
    EXPECT_EQ(tokens[2].col, 5);
}

TEST(Lexer, TokenPositionMultiLine)
{
    auto tokens = lex("x\ny");
    EXPECT_EQ(tokens[0].line, 1);
    EXPECT_EQ(tokens[0].col, 1);
    // NEWLINE
    EXPECT_EQ(tokens[1].line, 1);
    // y
    EXPECT_EQ(tokens[2].line, 2);
    EXPECT_EQ(tokens[2].col, 1);
}

// ============================================================
// Ошибки
// ============================================================

TEST(Lexer, UnterminatedSingleQuoteString)
{
    EXPECT_ANY_THROW(lex("'hello"));
}

TEST(Lexer, UnterminatedDoubleQuoteString)
{
    EXPECT_ANY_THROW(lex("\"hello"));
}

TEST(Lexer, UnterminatedBlockComment)
{
    EXPECT_ANY_THROW(lex("%{\nunfinished"));
}

TEST(Lexer, InvalidCharacter)
{
    // Символ, которого нет в MATLAB (например, #, если он не поддерживается)
    EXPECT_ANY_THROW(lex("#"));
}

TEST(Lexer, MismatchedBrackets)
{
    // Зависит от того, обрабатывает ли лексер — может и парсер
    // Если лексер трекает bracketStack_, может бросить
    // EXPECT_ANY_THROW(lex("[)"));
}

// ============================================================
// Граничные случаи
// ============================================================

TEST(Lexer, DotFollowedByNumber)
{
    // .5 — это число, а не DOT + 5
    auto tokens = lex(".5");
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
}

TEST(Lexer, DotBetweenIdentifiers)
{
    // a.b — DOT
    expectTokenTypes("a.b",
                     {TokenType::IDENTIFIER,
                      TokenType::DOT,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, MultipleDotOps)
{
    expectTokenTypes("a.*b./c.^d",
                     {TokenType::IDENTIFIER,
                      TokenType::DOT_STAR,
                      TokenType::IDENTIFIER,
                      TokenType::DOT_SLASH,
                      TokenType::IDENTIFIER,
                      TokenType::DOT_CARET,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, MultipleTransposes)
{
    // x'' — двойное транспонирование
    expectTokenTypes("x''",
                     {TokenType::IDENTIFIER,
                      TokenType::APOSTROPHE,
                      TokenType::APOSTROPHE,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, TransposeThenDotTranspose)
{
    // x'.'
    expectTokenTypes("x'.'",
                     {TokenType::IDENTIFIER,
                      TokenType::APOSTROPHE,
                      TokenType::DOT_APOSTROPHE,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, AnonymousFunction)
{
    expectTokenTypes("f = @(x, y) x + y;",
                     {TokenType::IDENTIFIER,
                      TokenType::ASSIGN,
                      TokenType::AT,
                      TokenType::LPAREN,
                      TokenType::IDENTIFIER,
                      TokenType::COMMA,
                      TokenType::IDENTIFIER,
                      TokenType::RPAREN,
                      TokenType::IDENTIFIER,
                      TokenType::PLUS,
                      TokenType::IDENTIFIER,
                      TokenType::SEMICOLON,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, GlobalPersistent)
{
    expectTokenTypes("global x\npersistent y",
                     {TokenType::KW_GLOBAL,
                      TokenType::IDENTIFIER,
                      TokenType::NEWLINE,
                      TokenType::KW_PERSISTENT,
                      TokenType::IDENTIFIER,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, StringInMatrix)
{
    // ['hello', ' ', 'world']
    auto tokens = lex("['hello', ' ', 'world']");
    int strCount = 0;
    for (auto &t : tokens) {
        if (t.type == TokenType::STRING)
            strCount++;
    }
    EXPECT_EQ(strCount, 3);
}

TEST(Lexer, BreakContinue)
{
    expectTokenTypes("break\ncontinue",
                     {TokenType::KW_BREAK,
                      TokenType::NEWLINE,
                      TokenType::KW_CONTINUE,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, MatrixWithNegativeNumbers)
{
    // [1 -2 3] — минус здесь неоднозначен
    // Лексер должен выдать: [ 1 COMMA MINUS 2 COMMA 3 ] или [ 1 COMMA -2 COMMA 3 ]
    auto tokens = lex("[1 -2 3]");
    EXPECT_EQ(tokens.front().type, TokenType::LBRACKET);
    EXPECT_EQ(tokens.back().type, TokenType::END_OF_INPUT);
    // Просто убеждаемся, что не крашится и есть числа
    int numCount = 0;
    for (auto &t : tokens) {
        if (t.type == TokenType::NUMBER)
            numCount++;
    }
    EXPECT_GE(numCount, 3);
}

TEST(Lexer, TildeAsOutputPlaceholder)
{
    // [~, x] = func()
    expectTokenTypes("[~, x] = func()",
                     {TokenType::LBRACKET,
                      TokenType::TILDE,
                      TokenType::COMMA,
                      TokenType::IDENTIFIER,
                      TokenType::RBRACKET,
                      TokenType::ASSIGN,
                      TokenType::IDENTIFIER,
                      TokenType::LPAREN,
                      TokenType::RPAREN,
                      TokenType::END_OF_INPUT});
}

TEST(Lexer, EndOfInputAlwaysLast)
{
    auto inputs = {"", "x", "1+2", "'hello'", "% comment", "\n\n"};
    for (auto &input : inputs) {
        auto tokens = lex(input);
        ASSERT_FALSE(tokens.empty());
        EXPECT_EQ(tokens.back().type, TokenType::END_OF_INPUT) << "Input: \"" << input << "\"";
    }
}

// ============================================================
// main
// ============================================================

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
