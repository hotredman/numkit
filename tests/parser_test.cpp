// tests/parser_test.cpp

#include "MLabAst.hpp"
#include "MLabLexer.hpp"
#include "MLabParser.hpp"
#include <gtest/gtest.h>

using namespace mlab;

// ============================================================
// Вспомогательная функция: source -> AST (Block)
// ============================================================
static ASTNodePtr parseSource(const std::string &source)
{
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    return parser.parse();
}

// Получить i-й statement из Block
static const ASTNode &stmt(const ASTNode &block, size_t i)
{
    EXPECT_EQ(block.type, NodeType::BLOCK);
    EXPECT_GT(block.children.size(), i);
    return *block.children[i];
}

// ============================================================
// Тесты: Числовые литералы
// ============================================================
class ParserNumberLiteralTest : public ::testing::Test
{};

TEST_F(ParserNumberLiteralTest, IntegerLiteral)
{
    auto ast = parseSource("42;");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::EXPR_STMT);
    ASSERT_EQ(s.children.size(), 1u);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::NUMBER_LITERAL);
    EXPECT_DOUBLE_EQ(expr.numValue, 42.0);
}

TEST_F(ParserNumberLiteralTest, FloatingPointLiteral)
{
    auto ast = parseSource("3.14;");
    const auto &s = stmt(*ast, 0);
    ASSERT_EQ(s.children.size(), 1u);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::NUMBER_LITERAL);
    EXPECT_DOUBLE_EQ(expr.numValue, 3.14);
}

TEST_F(ParserNumberLiteralTest, ScientificNotation)
{
    auto ast = parseSource("1e10;");
    const auto &s = stmt(*ast, 0);
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->type, NodeType::NUMBER_LITERAL);
    EXPECT_DOUBLE_EQ(s.children[0]->numValue, 1e10);
}

TEST_F(ParserNumberLiteralTest, ImaginaryLiteral)
{
    auto ast = parseSource("3i;");
    const auto &s = stmt(*ast, 0);
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->type, NodeType::IMAG_LITERAL);
}

TEST_F(ParserNumberLiteralTest, ZeroLiteral)
{
    auto ast = parseSource("0;");
    const auto &s = stmt(*ast, 0);
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->type, NodeType::NUMBER_LITERAL);
    EXPECT_DOUBLE_EQ(s.children[0]->numValue, 0.0);
}

TEST_F(ParserNumberLiteralTest, HexLiteral)
{
    auto ast = parseSource("0xFF;");
    const auto &s = stmt(*ast, 0);
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->type, NodeType::NUMBER_LITERAL);
    EXPECT_DOUBLE_EQ(s.children[0]->numValue, 255.0);
}

TEST_F(ParserNumberLiteralTest, BinaryLiteral)
{
    auto ast = parseSource("0b1010;");
    const auto &s = stmt(*ast, 0);
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->type, NodeType::NUMBER_LITERAL);
    EXPECT_DOUBLE_EQ(s.children[0]->numValue, 10.0);
}

TEST_F(ParserNumberLiteralTest, OctalLiteral)
{
    auto ast = parseSource("0o17;");
    const auto &s = stmt(*ast, 0);
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->type, NodeType::NUMBER_LITERAL);
    EXPECT_DOUBLE_EQ(s.children[0]->numValue, 15.0);
}

TEST_F(ParserNumberLiteralTest, UnderscoreLiteral)
{
    auto ast = parseSource("1_000;");
    const auto &s = stmt(*ast, 0);
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->type, NodeType::NUMBER_LITERAL);
    EXPECT_DOUBLE_EQ(s.children[0]->numValue, 1000.0);
}

TEST_F(ParserNumberLiteralTest, MultiUnderscoreLiteral)
{
    auto ast = parseSource("1_000_000;");
    const auto &s = stmt(*ast, 0);
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_DOUBLE_EQ(s.children[0]->numValue, 1000000.0);
}

TEST_F(ParserNumberLiteralTest, NegativeExponent)
{
    auto ast = parseSource("2.5e-3;");
    const auto &s = stmt(*ast, 0);
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->type, NodeType::NUMBER_LITERAL);
    EXPECT_DOUBLE_EQ(s.children[0]->numValue, 2.5e-3);
}

// ============================================================
// Тесты: Строковые литералы
// ============================================================
class ParserStringLiteralTest : public ::testing::Test
{};

TEST_F(ParserStringLiteralTest, SingleQuotedString)
{
    auto ast = parseSource("'hello';");
    const auto &s = stmt(*ast, 0);
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->type, NodeType::STRING_LITERAL);
    EXPECT_EQ(s.children[0]->strValue, "hello");
}

TEST_F(ParserStringLiteralTest, DoubleQuotedString)
{
    auto ast = parseSource("\"world\";");
    const auto &s = stmt(*ast, 0);
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->type, NodeType::STRING_LITERAL);
    EXPECT_EQ(s.children[0]->strValue, "world");
}

TEST_F(ParserStringLiteralTest, EmptyString)
{
    auto ast = parseSource("'';");
    const auto &s = stmt(*ast, 0);
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->type, NodeType::STRING_LITERAL);
    EXPECT_EQ(s.children[0]->strValue, "");
}

// ============================================================
// Тесты: Boolean литералы
// ============================================================
class ParserBoolLiteralTest : public ::testing::Test
{};

TEST_F(ParserBoolLiteralTest, TrueLiteral)
{
    auto ast = parseSource("true;");
    const auto &s = stmt(*ast, 0);
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->type, NodeType::BOOL_LITERAL);
    EXPECT_TRUE(s.children[0]->boolValue);
}

TEST_F(ParserBoolLiteralTest, FalseLiteral)
{
    auto ast = parseSource("false;");
    const auto &s = stmt(*ast, 0);
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->type, NodeType::BOOL_LITERAL);
    EXPECT_FALSE(s.children[0]->boolValue);
}

// ============================================================
// Тесты: Идентификаторы
// ============================================================
class ParserIdentifierTest : public ::testing::Test
{};

TEST_F(ParserIdentifierTest, SimpleIdentifier)
{
    auto ast = parseSource("x;");
    const auto &s = stmt(*ast, 0);
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->type, NodeType::IDENTIFIER);
    EXPECT_EQ(s.children[0]->strValue, "x");
}

TEST_F(ParserIdentifierTest, LongIdentifier)
{
    auto ast = parseSource("myVariable_123;");
    const auto &s = stmt(*ast, 0);
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->type, NodeType::IDENTIFIER);
    EXPECT_EQ(s.children[0]->strValue, "myVariable_123");
}

// ============================================================
// Тесты: Бинарные операции
// ============================================================
class ParserBinaryOpTest : public ::testing::Test
{};

TEST_F(ParserBinaryOpTest, Addition)
{
    auto ast = parseSource("1 + 2;");
    const auto &s = stmt(*ast, 0);
    ASSERT_EQ(s.children.size(), 1u);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "+");
    ASSERT_EQ(expr.children.size(), 2u);
    EXPECT_EQ(expr.children[0]->type, NodeType::NUMBER_LITERAL);
    EXPECT_EQ(expr.children[1]->type, NodeType::NUMBER_LITERAL);
}

TEST_F(ParserBinaryOpTest, Subtraction)
{
    auto ast = parseSource("5 - 3;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "-");
}

TEST_F(ParserBinaryOpTest, Multiplication)
{
    auto ast = parseSource("2 * 3;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "*");
}

TEST_F(ParserBinaryOpTest, Division)
{
    auto ast = parseSource("6 / 2;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "/");
}

TEST_F(ParserBinaryOpTest, Power)
{
    auto ast = parseSource("2 ^ 3;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "^");
}

TEST_F(ParserBinaryOpTest, ElementWiseMul)
{
    auto ast = parseSource("a .* b;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, ".*");
}

TEST_F(ParserBinaryOpTest, ElementWiseDiv)
{
    auto ast = parseSource("a ./ b;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "./");
}

TEST_F(ParserBinaryOpTest, ElementWisePow)
{
    auto ast = parseSource("a .^ b;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, ".^");
}

TEST_F(ParserBinaryOpTest, Backslash)
{
    auto ast = parseSource("A \\ b;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "\\");
}

// ============================================================
// Тесты: Приоритет операций
// ============================================================
class ParserPrecedenceTest : public ::testing::Test
{};

TEST_F(ParserPrecedenceTest, MulBeforeAdd)
{
    // 1 + 2 * 3 → +(1, *(2,3))
    auto ast = parseSource("1 + 2 * 3;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "+");
    EXPECT_EQ(expr.children[0]->type, NodeType::NUMBER_LITERAL);
    EXPECT_EQ(expr.children[1]->type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.children[1]->strValue, "*");
}

TEST_F(ParserPrecedenceTest, PowerBeforeMul)
{
    // 2 * 3 ^ 4 → *(2, ^(3,4))
    auto ast = parseSource("2 * 3 ^ 4;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "*");
    EXPECT_EQ(expr.children[1]->type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.children[1]->strValue, "^");
}

TEST_F(ParserPrecedenceTest, ParenthesesOverride)
{
    // (1 + 2) * 3 → *(+(1,2), 3)
    auto ast = parseSource("(1 + 2) * 3;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "*");
    EXPECT_EQ(expr.children[0]->type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.children[0]->strValue, "+");
}

TEST_F(ParserPrecedenceTest, ComparisonAfterArithmetic)
{
    // a + b > c → >(+(a,b), c)
    auto ast = parseSource("a + b > c;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, ">");
    EXPECT_EQ(expr.children[0]->type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.children[0]->strValue, "+");
}

TEST_F(ParserPrecedenceTest, AndOrPrecedence)
{
    // a | b & c → |(a, &(b,c))
    // В MATLAB: & имеет более высокий приоритет, чем |
    auto ast = parseSource("a | b & c;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "|");
    EXPECT_EQ(expr.children[1]->type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.children[1]->strValue, "&");
}

TEST_F(ParserPrecedenceTest, ShortCircuitAndOr)
{
    // a || b && c → ||(a, &&(b,c))
    // В MATLAB: && имеет более высокий приоритет, чем ||
    auto ast = parseSource("a || b && c;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "||");
    EXPECT_EQ(expr.children[1]->type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.children[1]->strValue, "&&");
}

TEST_F(ParserPrecedenceTest, ElementAndHigherThanShortCircuit)
{
    // a && b | c → &&(a, |(b,c))
    // В MATLAB: | имеет более высокий приоритет, чем &&
    auto ast = parseSource("a && b | c;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "&&");
    EXPECT_EQ(expr.children[1]->type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.children[1]->strValue, "|");
}

TEST_F(ParserPrecedenceTest, ElementOrHigherThanShortCircuitAnd)
{
    // a || b | c → ||(a, |(b,c))
    auto ast = parseSource("a || b | c;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.strValue, "||");
    EXPECT_EQ(expr.children[1]->strValue, "|");
}

TEST_F(ParserPrecedenceTest, AllFourLogicalLevels)
{
    // a || b && c | d & e
    // Парсится как: a || (b && (c | (d & e)))
    auto ast = parseSource("a || b && c | d & e;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.strValue, "||");                                       // ||
    EXPECT_EQ(expr.children[1]->strValue, "&&");                          // &&
    EXPECT_EQ(expr.children[1]->children[1]->strValue, "|");              // |
    EXPECT_EQ(expr.children[1]->children[1]->children[1]->strValue, "&"); // &
}

TEST_F(ParserPrecedenceTest, UnaryMinusVsPower)
{
    // В MATLAB: -2^2 = -(2^2) = -4 (а не (-2)^2 = 4)
    // unary minus имеет более низкий приоритет, чем ^
    auto ast = parseSource("-2^2;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.strValue, "-");
    ASSERT_EQ(expr.children.size(), 1u);
    EXPECT_EQ(expr.children[0]->type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.children[0]->strValue, "^");
}

TEST_F(ParserPrecedenceTest, PowerRightAssociative)
{
    // В MATLAB: 2^3^4 = 2^(3^4)  (правоассоциативно)
    auto ast = parseSource("2^3^4;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "^");
    EXPECT_EQ(expr.children[0]->type, NodeType::NUMBER_LITERAL);
    EXPECT_DOUBLE_EQ(expr.children[0]->numValue, 2.0);
    // Правый потомок — тоже ^
    EXPECT_EQ(expr.children[1]->type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.children[1]->strValue, "^");
}

TEST_F(ParserPrecedenceTest, AddSubLeftAssociative)
{
    // a + b - c → -( +(a,b), c )
    auto ast = parseSource("a + b - c;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "-");
    EXPECT_EQ(expr.children[0]->type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.children[0]->strValue, "+");
}

TEST_F(ParserPrecedenceTest, MulDivLeftAssociative)
{
    // a * b / c → /( *(a,b), c )
    auto ast = parseSource("a * b / c;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "/");
    EXPECT_EQ(expr.children[0]->type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.children[0]->strValue, "*");
}

TEST_F(ParserPrecedenceTest, MixedOperatorsFullTree)
{
    // a + b * c - d / e → -( +(a, *(b,c)), /(d,e) )
    auto ast = parseSource("a + b * c - d / e;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.strValue, "-");
    EXPECT_EQ(expr.children[0]->strValue, "+");
    EXPECT_EQ(expr.children[1]->strValue, "/");
}

// ============================================================
// Тесты: Сравнения
// ============================================================
class ParserComparisonTest : public ::testing::Test
{};

TEST_F(ParserComparisonTest, Equal)
{
    auto ast = parseSource("a == b;");
    const auto &expr = *stmt(*ast, 0).children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "==");
}

TEST_F(ParserComparisonTest, NotEqual)
{
    auto ast = parseSource("a ~= b;");
    const auto &expr = *stmt(*ast, 0).children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "~=");
}

TEST_F(ParserComparisonTest, LessThan)
{
    auto ast = parseSource("a < b;");
    const auto &expr = *stmt(*ast, 0).children[0];
    EXPECT_EQ(expr.strValue, "<");
}

TEST_F(ParserComparisonTest, GreaterThan)
{
    auto ast = parseSource("a > b;");
    const auto &expr = *stmt(*ast, 0).children[0];
    EXPECT_EQ(expr.strValue, ">");
}

TEST_F(ParserComparisonTest, LessOrEqual)
{
    auto ast = parseSource("a <= b;");
    const auto &expr = *stmt(*ast, 0).children[0];
    EXPECT_EQ(expr.strValue, "<=");
}

TEST_F(ParserComparisonTest, GreaterOrEqual)
{
    auto ast = parseSource("a >= b;");
    const auto &expr = *stmt(*ast, 0).children[0];
    EXPECT_EQ(expr.strValue, ">=");
}

// ============================================================
// Тесты: Унарные операции
// ============================================================
class ParserUnaryOpTest : public ::testing::Test
{};

TEST_F(ParserUnaryOpTest, UnaryMinus)
{
    auto ast = parseSource("-x;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.strValue, "-");
    ASSERT_EQ(expr.children.size(), 1u);
    EXPECT_EQ(expr.children[0]->type, NodeType::IDENTIFIER);
}

TEST_F(ParserUnaryOpTest, UnaryPlus)
{
    // FIX: unary plus теперь создаёт узел UNARY_OP (для совместимости с uplus)
    auto ast = parseSource("+x;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.strValue, "+");
    ASSERT_EQ(expr.children.size(), 1u);
    EXPECT_EQ(expr.children[0]->type, NodeType::IDENTIFIER);
    EXPECT_EQ(expr.children[0]->strValue, "x");
}

TEST_F(ParserUnaryOpTest, LogicalNot)
{
    auto ast = parseSource("~x;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.strValue, "~");
}

TEST_F(ParserUnaryOpTest, Transpose)
{
    auto ast = parseSource("x';");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.strValue, "'");
}

TEST_F(ParserUnaryOpTest, DotTranspose)
{
    auto ast = parseSource("x.';");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.strValue, ".'");
}

TEST_F(ParserUnaryOpTest, DoubleNegationWithParens)
{
    // -(-x) — допустимая двойная негация через скобки
    auto ast = parseSource("-(-x);");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.strValue, "-");
    EXPECT_EQ(expr.children[0]->type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.children[0]->strValue, "-");
}

TEST_F(ParserUnaryOpTest, NotOfComparison)
{
    // ~(a == b) — в MATLAB: логическое отрицание результата сравнения
    auto ast = parseSource("~(a == b);");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.strValue, "~");
    EXPECT_EQ(expr.children[0]->type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.children[0]->strValue, "==");
}

// ============================================================
// Тесты: Присваивание
// ============================================================
class ParserAssignTest : public ::testing::Test
{};

TEST_F(ParserAssignTest, SimpleAssign)
{
    auto ast = parseSource("x = 5;");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::ASSIGN);
    ASSERT_EQ(s.children.size(), 2u);
    EXPECT_EQ(s.children[0]->type, NodeType::IDENTIFIER);
    EXPECT_EQ(s.children[0]->strValue, "x");
    EXPECT_EQ(s.children[1]->type, NodeType::NUMBER_LITERAL);
    EXPECT_DOUBLE_EQ(s.children[1]->numValue, 5.0);
    EXPECT_TRUE(s.suppressOutput);
}

TEST_F(ParserAssignTest, AssignWithoutSemicolon)
{
    // Без ; — вывод не подавлен
    auto ast = parseSource("x = 5\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::ASSIGN);
    EXPECT_FALSE(s.suppressOutput);
}

TEST_F(ParserAssignTest, IndexedAssign)
{
    auto ast = parseSource("x(1) = 10;");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::ASSIGN);
    // LHS парсится как CALL — разрешение index vs call на этапе интерпретации
    EXPECT_EQ(s.children[0]->type, NodeType::CALL);
}

TEST_F(ParserAssignTest, MultiAssign)
{
    auto ast = parseSource("[a, b] = func();");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::MULTI_ASSIGN);
    ASSERT_EQ(s.returnNames.size(), 2u);
    EXPECT_EQ(s.returnNames[0], "a");
    EXPECT_EQ(s.returnNames[1], "b");
}

TEST_F(ParserAssignTest, MultiAssignWithTilde)
{
    // [~, b] = size(A); — игнорирование первого выхода
    auto ast = parseSource("[~, b] = size(A);");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::MULTI_ASSIGN);
    ASSERT_EQ(s.returnNames.size(), 2u);
    EXPECT_EQ(s.returnNames[0], "~");
    EXPECT_EQ(s.returnNames[1], "b");
}

TEST_F(ParserAssignTest, MultiAssignThreeOutputs)
{
    auto ast = parseSource("[U, S, V] = svd(A);");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::MULTI_ASSIGN);
    ASSERT_EQ(s.returnNames.size(), 3u);
    EXPECT_EQ(s.returnNames[0], "U");
    EXPECT_EQ(s.returnNames[1], "S");
    EXPECT_EQ(s.returnNames[2], "V");
}

TEST_F(ParserAssignTest, DeleteAssign)
{
    // x = [] — обычное присваивание пустой матрицы (не delete)
    auto ast = parseSource("x = [];");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::ASSIGN);
    EXPECT_EQ(s.children[1]->type, NodeType::MATRIX_LITERAL);
    EXPECT_EQ(s.children[1]->children.size(), 0u); // пустая матрица
}

TEST_F(ParserAssignTest, IndexedDeleteAssign)
{
    // A(1:3) = [] — удаление элементов (DELETE_ASSIGN)
    auto ast = parseSource("A(1:3) = [];");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::DELETE_ASSIGN);
    EXPECT_EQ(s.children[0]->type, NodeType::CALL);
}

// ============================================================
// Тесты: Индексация / Вызов функций
// ============================================================
class ParserIndexCallTest : public ::testing::Test
{};

TEST_F(ParserIndexCallTest, SingleIndex)
{
    // В парсере x(1) всегда CALL — различение index/call при интерпретации
    auto ast = parseSource("x(1);");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::CALL);
    // children: [x, 1]
    ASSERT_EQ(expr.children.size(), 2u);
    EXPECT_EQ(expr.children[0]->type, NodeType::IDENTIFIER);
    EXPECT_EQ(expr.children[1]->type, NodeType::NUMBER_LITERAL);
}

TEST_F(ParserIndexCallTest, MultiIndex)
{
    auto ast = parseSource("A(1, 2);");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::CALL);
    // children: [A, 1, 2]
    ASSERT_EQ(expr.children.size(), 3u);
}

TEST_F(ParserIndexCallTest, CellIndex)
{
    auto ast = parseSource("c{1};");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::CELL_INDEX);
}

TEST_F(ParserIndexCallTest, FunctionCallNoArgs)
{
    auto ast = parseSource("foo();");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::CALL);
    // children: [foo] — только имя функции, без аргументов
    ASSERT_EQ(expr.children.size(), 1u);
}

TEST_F(ParserIndexCallTest, ChainedIndexing)
{
    // x(1)(2) — в MATLAB: результат x(1) индексируется (2)
    auto ast = parseSource("x(1)(2);");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::CALL);
    // Внутренний child[0] — тоже CALL
    EXPECT_EQ(expr.children[0]->type, NodeType::CALL);
}

TEST_F(ParserIndexCallTest, NestedCalls)
{
    auto ast = parseSource("foo(bar(x));");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::CALL);
    ASSERT_EQ(expr.children.size(), 2u);
    // Аргумент — вложенный вызов
    EXPECT_EQ(expr.children[1]->type, NodeType::CALL);
}

TEST_F(ParserIndexCallTest, ChainedCellFieldCall)
{
    // s.data{2}.name(1) — цепочка доступов
    auto ast = parseSource("s.data{2}.name(1);");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    // Внешний — CALL (индексация результата s.data{2}.name)
    EXPECT_EQ(expr.type, NodeType::CALL);
}

// ============================================================
// Тесты: Доступ к полям (dot access)
// ============================================================
class ParserFieldAccessTest : public ::testing::Test
{};

TEST_F(ParserFieldAccessTest, SimpleFieldAccess)
{
    auto ast = parseSource("s.field;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::FIELD_ACCESS);
    EXPECT_EQ(expr.strValue, "field");
    ASSERT_GE(expr.children.size(), 1u);
    EXPECT_EQ(expr.children[0]->type, NodeType::IDENTIFIER);
    EXPECT_EQ(expr.children[0]->strValue, "s");
}

TEST_F(ParserFieldAccessTest, ChainedFieldAccess)
{
    // a.b.c — цепочка: FIELD_ACCESS(c) → FIELD_ACCESS(b) → IDENTIFIER(a)
    auto ast = parseSource("a.b.c;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::FIELD_ACCESS);
    EXPECT_EQ(expr.strValue, "c");
    EXPECT_EQ(expr.children[0]->type, NodeType::FIELD_ACCESS);
    EXPECT_EQ(expr.children[0]->strValue, "b");
    EXPECT_EQ(expr.children[0]->children[0]->type, NodeType::IDENTIFIER);
    EXPECT_EQ(expr.children[0]->children[0]->strValue, "a");
}

// ============================================================
// Тесты: Colon-выражения
// ============================================================
class ParserColonExprTest : public ::testing::Test
{};

TEST_F(ParserColonExprTest, SimpleRange)
{
    // 1:10 → COLON_EXPR с 2 children: [1, 10]
    auto ast = parseSource("1:10;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::COLON_EXPR);
    ASSERT_EQ(expr.children.size(), 2u);
    EXPECT_DOUBLE_EQ(expr.children[0]->numValue, 1.0);
    EXPECT_DOUBLE_EQ(expr.children[1]->numValue, 10.0);
}

TEST_F(ParserColonExprTest, SteppedRange)
{
    // 1:2:10 → COLON_EXPR с 3 children: [start, step, stop]
    auto ast = parseSource("1:2:10;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::COLON_EXPR);
    ASSERT_EQ(expr.children.size(), 3u);
    EXPECT_DOUBLE_EQ(expr.children[0]->numValue, 1.0);
    EXPECT_DOUBLE_EQ(expr.children[1]->numValue, 2.0);
    EXPECT_DOUBLE_EQ(expr.children[2]->numValue, 10.0);
}

TEST_F(ParserColonExprTest, ColonWithExpressions)
{
    auto ast = parseSource("a:b;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::COLON_EXPR);
    ASSERT_EQ(expr.children.size(), 2u);
    EXPECT_EQ(expr.children[0]->type, NodeType::IDENTIFIER);
    EXPECT_EQ(expr.children[1]->type, NodeType::IDENTIFIER);
}

TEST_F(ParserColonExprTest, ColonWithArithmetic)
{
    // 1:n-1 → COLON_EXPR с children: [1, -(n,1)]
    auto ast = parseSource("1:n-1;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::COLON_EXPR);
    ASSERT_EQ(expr.children.size(), 2u);
    EXPECT_EQ(expr.children[1]->type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.children[1]->strValue, "-");
}

// ============================================================
// Тесты: Матричные литералы
// ============================================================
class ParserMatrixLiteralTest : public ::testing::Test
{};

TEST_F(ParserMatrixLiteralTest, EmptyMatrix)
{
    auto ast = parseSource("[];");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::MATRIX_LITERAL);
    EXPECT_EQ(expr.children.size(), 0u);
}

TEST_F(ParserMatrixLiteralTest, RowVector)
{
    // [1, 2, 3] → MATRIX_LITERAL с 1 row (BLOCK) содержащим 3 элемента
    auto ast = parseSource("[1, 2, 3];");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::MATRIX_LITERAL);
    ASSERT_EQ(expr.children.size(), 1u);              // один ряд
    EXPECT_EQ(expr.children[0]->children.size(), 3u); // три элемента
}

TEST_F(ParserMatrixLiteralTest, ColumnVector)
{
    // [1; 2; 3] → 3 ряда по 1 элементу
    auto ast = parseSource("[1; 2; 3];");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::MATRIX_LITERAL);
    ASSERT_EQ(expr.children.size(), 3u);
    EXPECT_EQ(expr.children[0]->children.size(), 1u);
    EXPECT_EQ(expr.children[1]->children.size(), 1u);
    EXPECT_EQ(expr.children[2]->children.size(), 1u);
}

TEST_F(ParserMatrixLiteralTest, Matrix2x2)
{
    // [1, 2; 3, 4] → 2 ряда по 2 элемента
    auto ast = parseSource("[1, 2; 3, 4];");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::MATRIX_LITERAL);
    ASSERT_EQ(expr.children.size(), 2u);
    EXPECT_EQ(expr.children[0]->children.size(), 2u);
    EXPECT_EQ(expr.children[1]->children.size(), 2u);
}

TEST_F(ParserMatrixLiteralTest, SpaceSeparatedRow)
{
    // В MATLAB: [1 2 3] эквивалентно [1, 2, 3]
    auto ast = parseSource("[1 2 3];");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::MATRIX_LITERAL);
    ASSERT_EQ(expr.children.size(), 1u);
    EXPECT_EQ(expr.children[0]->children.size(), 3u);
}

TEST_F(ParserMatrixLiteralTest, SpaceVsBinaryPlusInMatrix)
{
    // В MATLAB:
    // [1 +2] → два элемента: 1 и +2 (пробел перед +, нет пробела после)
    // [1 + 2] → одно выражение: 1+2 = 3 (пробелы с обеих сторон оператора)
    {
        auto ast = parseSource("[1 +2];");
        const auto &expr = *stmt(*ast, 0).children[0];
        EXPECT_EQ(expr.type, NodeType::MATRIX_LITERAL);
        ASSERT_EQ(expr.children.size(), 1u);
        EXPECT_EQ(expr.children[0]->children.size(), 2u); // два элемента
    }
    {
        auto ast = parseSource("[1 + 2];");
        const auto &expr = *stmt(*ast, 0).children[0];
        EXPECT_EQ(expr.type, NodeType::MATRIX_LITERAL);
        ASSERT_EQ(expr.children.size(), 1u);
        EXPECT_EQ(expr.children[0]->children.size(), 1u); // одно выражение
    }
}

TEST_F(ParserMatrixLiteralTest, SpaceVsBinaryMinusInMatrix)
{
    // [3 -1] → два элемента: 3 и -1
    // [3 - 1] → одно выражение: 3-1
    {
        auto ast = parseSource("[3 -1];");
        const auto &expr = *stmt(*ast, 0).children[0];
        ASSERT_EQ(expr.children.size(), 1u);
        EXPECT_EQ(expr.children[0]->children.size(), 2u);
    }
    {
        auto ast = parseSource("[3 - 1];");
        const auto &expr = *stmt(*ast, 0).children[0];
        ASSERT_EQ(expr.children.size(), 1u);
        EXPECT_EQ(expr.children[0]->children.size(), 1u);
    }
}

TEST_F(ParserMatrixLiteralTest, StringsInMatrix)
{
    // ['hello' ' ' 'world'] — конкатенация символьных массивов
    auto ast = parseSource("['hello' ' ' 'world'];");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::MATRIX_LITERAL);
    ASSERT_EQ(expr.children.size(), 1u);              // один ряд
    EXPECT_EQ(expr.children[0]->children.size(), 3u); // три строковых элемента
}

TEST_F(ParserMatrixLiteralTest, MatrixWithExpressions)
{
    auto ast = parseSource("[1+2, 3*4; 5-6, 7/8];");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::MATRIX_LITERAL);
    ASSERT_EQ(expr.children.size(), 2u);
    EXPECT_EQ(expr.children[0]->children.size(), 2u);
    EXPECT_EQ(expr.children[1]->children.size(), 2u);
}

// ============================================================
// Тесты: Cell literals
// ============================================================
class ParserCellLiteralTest : public ::testing::Test
{};

TEST_F(ParserCellLiteralTest, EmptyCell)
{
    auto ast = parseSource("{};");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::CELL_LITERAL);
    EXPECT_EQ(expr.children.size(), 0u);
}

TEST_F(ParserCellLiteralTest, CellWithElements)
{
    // {1, 'hello', true} — один ряд с 3 элементами
    auto ast = parseSource("{1, 'hello', true};");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::CELL_LITERAL);
    ASSERT_EQ(expr.children.size(), 1u);
    EXPECT_EQ(expr.children[0]->children.size(), 3u);
}

TEST_F(ParserCellLiteralTest, CellMultiRow)
{
    // {1, 2; 3, 4} — 2 ряда по 2 элемента
    auto ast = parseSource("{1, 2; 3, 4};");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::CELL_LITERAL);
    ASSERT_EQ(expr.children.size(), 2u);
    EXPECT_EQ(expr.children[0]->children.size(), 2u);
    EXPECT_EQ(expr.children[1]->children.size(), 2u);
}

// ============================================================
// Тесты: if / elseif / else
// ============================================================
class ParserIfTest : public ::testing::Test
{};

TEST_F(ParserIfTest, SimpleIf)
{
    auto ast = parseSource(R"(
        if x > 0
            y = 1;
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::IF_STMT);
    ASSERT_EQ(s.branches.size(), 1u);
    EXPECT_EQ(s.elseBranch, nullptr);
}

TEST_F(ParserIfTest, IfElse)
{
    auto ast = parseSource(R"(
        if x > 0
            y = 1;
        else
            y = -1;
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::IF_STMT);
    ASSERT_EQ(s.branches.size(), 1u);
    EXPECT_NE(s.elseBranch, nullptr);
}

TEST_F(ParserIfTest, IfElseifElse)
{
    auto ast = parseSource(R"(
        if x > 0
            y = 1;
        elseif x == 0
            y = 0;
        else
            y = -1;
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::IF_STMT);
    ASSERT_EQ(s.branches.size(), 2u);
    EXPECT_NE(s.elseBranch, nullptr);
}

TEST_F(ParserIfTest, IfMultipleElseif)
{
    auto ast = parseSource(R"(
        if a == 1
            x = 1;
        elseif a == 2
            x = 2;
        elseif a == 3
            x = 3;
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::IF_STMT);
    EXPECT_EQ(s.branches.size(), 3u);
    EXPECT_EQ(s.elseBranch, nullptr);
}

TEST_F(ParserIfTest, NestedIf)
{
    auto ast = parseSource(R"(
        if x > 0
            if y > 0
                z = 1;
            end
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::IF_STMT);
    const auto &body = *s.branches[0].second;
    EXPECT_EQ(body.type, NodeType::BLOCK);
    ASSERT_GE(body.children.size(), 1u);
    EXPECT_EQ(body.children[0]->type, NodeType::IF_STMT);
}

// ============================================================
// Тесты: for
// ============================================================
class ParserForTest : public ::testing::Test
{};

TEST_F(ParserForTest, SimpleFor)
{
    auto ast = parseSource(R"(
        for i = 1:10
            x = i;
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::FOR_STMT);
    EXPECT_EQ(s.strValue, "i");
    ASSERT_EQ(s.children.size(), 2u); // [range_expr, body_block]
    EXPECT_EQ(s.children[0]->type, NodeType::COLON_EXPR);
}

TEST_F(ParserForTest, ForWithStep)
{
    auto ast = parseSource(R"(
        for i = 1:2:10
            x = i;
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::FOR_STMT);
    EXPECT_EQ(s.children[0]->type, NodeType::COLON_EXPR);
    EXPECT_EQ(s.children[0]->children.size(), 3u);
}

TEST_F(ParserForTest, ForOverVector)
{
    auto ast = parseSource(R"(
        for i = [1, 3, 5, 7]
            disp(i);
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::FOR_STMT);
    EXPECT_EQ(s.children[0]->type, NodeType::MATRIX_LITERAL);
}

TEST_F(ParserForTest, ForOverMatrix)
{
    // В MATLAB: for i = [1 2; 3 4] итерирует по столбцам матрицы
    auto ast = parseSource(R"(
        for i = [1 2; 3 4]
            disp(i);
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::FOR_STMT);
    EXPECT_EQ(s.children[0]->type, NodeType::MATRIX_LITERAL);
}

TEST_F(ParserForTest, NestedFor)
{
    auto ast = parseSource(R"(
        for i = 1:3
            for j = 1:3
                A(i,j) = i + j;
            end
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::FOR_STMT);
    ASSERT_EQ(s.children.size(), 2u);
    // Тело содержит вложенный for
    const auto &body = *s.children[1];
    EXPECT_EQ(body.type, NodeType::BLOCK);
    ASSERT_GE(body.children.size(), 1u);
    EXPECT_EQ(body.children[0]->type, NodeType::FOR_STMT);
}

// ============================================================
// Тесты: while
// ============================================================
class ParserWhileTest : public ::testing::Test
{};

TEST_F(ParserWhileTest, SimpleWhile)
{
    auto ast = parseSource(R"(
        while x > 0
            x = x - 1;
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::WHILE_STMT);
    ASSERT_EQ(s.children.size(), 2u); // [condition, body]
}

TEST_F(ParserWhileTest, WhileTrue)
{
    auto ast = parseSource(R"(
        while true
            break;
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::WHILE_STMT);
    EXPECT_EQ(s.children[0]->type, NodeType::BOOL_LITERAL);
    EXPECT_TRUE(s.children[0]->boolValue);
}

// ============================================================
// Тесты: break / continue / return
// ============================================================
class ParserControlFlowTest : public ::testing::Test
{};

TEST_F(ParserControlFlowTest, Break)
{
    auto ast = parseSource(R"(
        while true
            break;
        end
    )");
    const auto &w = stmt(*ast, 0);
    EXPECT_EQ(w.type, NodeType::WHILE_STMT);
    const auto &body = *w.children[1];
    bool foundBreak = false;
    for (auto &c : body.children) {
        if (c->type == NodeType::BREAK_STMT)
            foundBreak = true;
    }
    EXPECT_TRUE(foundBreak);
}

TEST_F(ParserControlFlowTest, Continue)
{
    auto ast = parseSource(R"(
        for i = 1:10
            continue;
        end
    )");
    const auto &f = stmt(*ast, 0);
    EXPECT_EQ(f.type, NodeType::FOR_STMT);
    const auto &body = *f.children[1];
    bool foundContinue = false;
    for (auto &c : body.children) {
        if (c->type == NodeType::CONTINUE_STMT)
            foundContinue = true;
    }
    EXPECT_TRUE(foundContinue);
}

TEST_F(ParserControlFlowTest, Return)
{
    auto ast = parseSource(R"(
        function y = foo(x)
            y = x;
            return;
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::FUNCTION_DEF);
    // Проверяем что return есть в теле функции
    const auto &body = *s.children[0];
    bool foundReturn = false;
    for (auto &c : body.children) {
        if (c->type == NodeType::RETURN_STMT)
            foundReturn = true;
    }
    EXPECT_TRUE(foundReturn);
}

// ============================================================
// Тесты: switch / case / otherwise
// ============================================================
class ParserSwitchTest : public ::testing::Test
{};

TEST_F(ParserSwitchTest, SimpleSwitch)
{
    auto ast = parseSource(R"(
        switch x
            case 1
                y = 10;
            case 2
                y = 20;
            otherwise
                y = 0;
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::SWITCH_STMT);
    EXPECT_EQ(s.branches.size(), 2u);
    EXPECT_NE(s.elseBranch, nullptr);
}

TEST_F(ParserSwitchTest, SwitchWithoutOtherwise)
{
    auto ast = parseSource(R"(
        switch x
            case 'a'
                y = 1;
            case 'b'
                y = 2;
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::SWITCH_STMT);
    EXPECT_EQ(s.branches.size(), 2u);
    EXPECT_EQ(s.elseBranch, nullptr);
}

TEST_F(ParserSwitchTest, SwitchCaseWithCell)
{
    // В MATLAB: case {1, 2} — совпадение с любым из элементов
    auto ast = parseSource(R"(
        switch x
            case {1, 2}
                y = 1;
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::SWITCH_STMT);
    ASSERT_EQ(s.branches.size(), 1u);
    // Условие case — CELL_LITERAL
    EXPECT_EQ(s.branches[0].first->type, NodeType::CELL_LITERAL);
}

// ============================================================
// Тесты: function
// ============================================================
class ParserFunctionTest : public ::testing::Test
{};

TEST_F(ParserFunctionTest, SimpleFunction)
{
    auto ast = parseSource(R"(
        function y = foo(x)
            y = x * 2;
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::FUNCTION_DEF);
    EXPECT_EQ(s.strValue, "foo");
    ASSERT_EQ(s.paramNames.size(), 1u);
    EXPECT_EQ(s.paramNames[0], "x");
    ASSERT_EQ(s.returnNames.size(), 1u);
    EXPECT_EQ(s.returnNames[0], "y");
}

TEST_F(ParserFunctionTest, FunctionNoReturn)
{
    auto ast = parseSource(R"(
        function greet(name)
            disp(name);
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::FUNCTION_DEF);
    EXPECT_EQ(s.strValue, "greet");
    EXPECT_EQ(s.returnNames.size(), 0u);
    ASSERT_EQ(s.paramNames.size(), 1u);
    EXPECT_EQ(s.paramNames[0], "name");
}

TEST_F(ParserFunctionTest, FunctionMultipleReturns)
{
    auto ast = parseSource(R"(
        function [a, b] = swap(x, y)
            a = y;
            b = x;
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::FUNCTION_DEF);
    EXPECT_EQ(s.strValue, "swap");
    ASSERT_EQ(s.returnNames.size(), 2u);
    EXPECT_EQ(s.returnNames[0], "a");
    EXPECT_EQ(s.returnNames[1], "b");
    ASSERT_EQ(s.paramNames.size(), 2u);
    EXPECT_EQ(s.paramNames[0], "x");
    EXPECT_EQ(s.paramNames[1], "y");
}

TEST_F(ParserFunctionTest, FunctionNoArgs)
{
    auto ast = parseSource(R"(
        function x = getval()
            x = 42;
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::FUNCTION_DEF);
    EXPECT_EQ(s.paramNames.size(), 0u);
    EXPECT_EQ(s.returnNames.size(), 1u);
}

TEST_F(ParserFunctionTest, FunctionNoArgsNoParens)
{
    // В MATLAB: function x = getval  — допустимо без скобок
    auto ast = parseSource(R"(
        function x = getval
            x = 42;
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::FUNCTION_DEF);
    EXPECT_EQ(s.strValue, "getval");
    EXPECT_EQ(s.paramNames.size(), 0u);
}

TEST_F(ParserFunctionTest, FunctionThreeReturns)
{
    auto ast = parseSource(R"(
        function [r, g, b] = splitRGB(image)
            r = image(:,:,1);
            g = image(:,:,2);
            b = image(:,:,3);
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::FUNCTION_DEF);
    EXPECT_EQ(s.strValue, "splitRGB");
    EXPECT_EQ(s.returnNames.size(), 3u);
    EXPECT_EQ(s.paramNames.size(), 1u);
}

// ============================================================
// Тесты: Анонимные функции
// ============================================================
class ParserAnonFuncTest : public ::testing::Test
{};

TEST_F(ParserAnonFuncTest, SimpleAnon)
{
    auto ast = parseSource("f = @(x) x^2;");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::ASSIGN);
    EXPECT_EQ(s.children[1]->type, NodeType::ANON_FUNC);
    ASSERT_EQ(s.children[1]->paramNames.size(), 1u);
    EXPECT_EQ(s.children[1]->paramNames[0], "x");
}

TEST_F(ParserAnonFuncTest, AnonMultiParam)
{
    auto ast = parseSource("f = @(x, y) x + y;");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::ASSIGN);
    const auto &anon = *s.children[1];
    EXPECT_EQ(anon.type, NodeType::ANON_FUNC);
    ASSERT_EQ(anon.paramNames.size(), 2u);
    EXPECT_EQ(anon.paramNames[0], "x");
    EXPECT_EQ(anon.paramNames[1], "y");
    // Тело — выражение x + y
    ASSERT_EQ(anon.children.size(), 1u);
    EXPECT_EQ(anon.children[0]->type, NodeType::BINARY_OP);
}

TEST_F(ParserAnonFuncTest, FunctionHandle)
{
    // @sin — хэндл на существующую функцию
    auto ast = parseSource("f = @sin;");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::ASSIGN);
    const auto &rhs = *s.children[1];
    EXPECT_EQ(rhs.type, NodeType::ANON_FUNC);
    EXPECT_EQ(rhs.strValue, "sin");
    EXPECT_EQ(rhs.paramNames.size(), 0u);
    EXPECT_EQ(rhs.children.size(), 0u);
}

TEST_F(ParserAnonFuncTest, AnonNoParams)
{
    // @() 42 — анонимная функция без параметров
    auto ast = parseSource("f = @() 42;");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::ASSIGN);
    const auto &anon = *s.children[1];
    EXPECT_EQ(anon.type, NodeType::ANON_FUNC);
    EXPECT_EQ(anon.paramNames.size(), 0u);
    ASSERT_EQ(anon.children.size(), 1u);
    EXPECT_EQ(anon.children[0]->type, NodeType::NUMBER_LITERAL);
}

// ============================================================
// Тесты: try / catch
// ============================================================
class ParserTryCatchTest : public ::testing::Test
{};

TEST_F(ParserTryCatchTest, SimpleTryCatch)
{
    auto ast = parseSource(R"(
        try
            x = 1/0;
        catch e
            disp(e);
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::TRY_STMT);
    EXPECT_EQ(s.strValue, "e");       // имя переменной исключения
    ASSERT_EQ(s.children.size(), 2u); // [try_block, catch_block]
}

TEST_F(ParserTryCatchTest, TryWithoutCatchVar)
{
    auto ast = parseSource(R"(
        try
            x = 1;
        catch
            x = 0;
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::TRY_STMT);
    EXPECT_EQ(s.strValue, ""); // нет переменной
    ASSERT_EQ(s.children.size(), 2u);
}

// ============================================================
// Тесты: global / persistent
// ============================================================
class ParserGlobalPersistentTest : public ::testing::Test
{};

TEST_F(ParserGlobalPersistentTest, GlobalStatement)
{
    auto ast = parseSource("global x y z;");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::GLOBAL_STMT);
    ASSERT_EQ(s.paramNames.size(), 3u);
    EXPECT_EQ(s.paramNames[0], "x");
    EXPECT_EQ(s.paramNames[1], "y");
    EXPECT_EQ(s.paramNames[2], "z");
}

TEST_F(ParserGlobalPersistentTest, PersistentStatement)
{
    auto ast = parseSource("persistent count;");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::PERSISTENT_STMT);
    ASSERT_EQ(s.paramNames.size(), 1u);
    EXPECT_EQ(s.paramNames[0], "count");
}

// ============================================================
// Тесты: end как значение
// ============================================================
class ParserEndValTest : public ::testing::Test
{};

TEST_F(ParserEndValTest, EndInIndex)
{
    // x(end) — end как индекс последнего элемента
    auto ast = parseSource("x(end);");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::CALL);
    ASSERT_EQ(expr.children.size(), 2u);
    EXPECT_EQ(expr.children[1]->type, NodeType::END_VAL);
}

TEST_F(ParserEndValTest, EndMinusOne)
{
    // x(end-1) — end в арифметическом выражении
    auto ast = parseSource("x(end-1);");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::CALL);
    ASSERT_EQ(expr.children.size(), 2u);
    // Аргумент — бинарная операция end-1
    EXPECT_EQ(expr.children[1]->type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.children[1]->strValue, "-");
    EXPECT_EQ(expr.children[1]->children[0]->type, NodeType::END_VAL);
}

TEST_F(ParserEndValTest, EndInMultiDimIndex)
{
    // A(1:end, end-1:end) — end в нескольких измерениях
    auto ast = parseSource("A(1:end, end-1:end);");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::CALL);
    ASSERT_EQ(expr.children.size(), 3u); // [A, 1:end, end-1:end]
}

// ============================================================
// Тесты: Подавление вывода (semicolon)
// ============================================================
class ParserSuppressOutputTest : public ::testing::Test
{};

TEST_F(ParserSuppressOutputTest, WithSemicolon)
{
    auto ast = parseSource("x = 5;");
    const auto &s = stmt(*ast, 0);
    EXPECT_TRUE(s.suppressOutput);
}

TEST_F(ParserSuppressOutputTest, WithoutSemicolon)
{
    auto ast = parseSource("x = 5\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_FALSE(s.suppressOutput);
}

TEST_F(ParserSuppressOutputTest, ExprWithSemicolon)
{
    auto ast = parseSource("42;");
    const auto &s = stmt(*ast, 0);
    EXPECT_TRUE(s.suppressOutput);
}

TEST_F(ParserSuppressOutputTest, ExprWithoutSemicolon)
{
    auto ast = parseSource("42\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_FALSE(s.suppressOutput);
}

// ============================================================
// Тесты: Множественные statements
// ============================================================
class ParserMultiStatementTest : public ::testing::Test
{};

TEST_F(ParserMultiStatementTest, TwoStatements)
{
    auto ast = parseSource("x = 1;\ny = 2;\n");
    EXPECT_EQ(ast->type, NodeType::BLOCK);
    EXPECT_EQ(ast->children.size(), 2u);
}

TEST_F(ParserMultiStatementTest, StatementsOnSameLine)
{
    // В MATLAB: ; разделяет стейтменты на одной строке
    auto ast = parseSource("x = 1; y = 2;");
    EXPECT_EQ(ast->type, NodeType::BLOCK);
    EXPECT_EQ(ast->children.size(), 2u);
}

TEST_F(ParserMultiStatementTest, EmptyInput)
{
    auto ast = parseSource("");
    EXPECT_EQ(ast->type, NodeType::BLOCK);
    EXPECT_EQ(ast->children.size(), 0u);
}

TEST_F(ParserMultiStatementTest, OnlyNewlines)
{
    auto ast = parseSource("\n\n\n");
    EXPECT_EQ(ast->type, NodeType::BLOCK);
    EXPECT_EQ(ast->children.size(), 0u);
}

// ============================================================
// Тесты: Сложные выражения
// ============================================================
class ParserComplexExprTest : public ::testing::Test
{};

TEST_F(ParserComplexExprTest, NestedParentheses)
{
    auto ast = parseSource("((1 + 2) * (3 - 4));");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "*");
    EXPECT_EQ(expr.children[0]->strValue, "+");
    EXPECT_EQ(expr.children[1]->strValue, "-");
}

TEST_F(ParserComplexExprTest, TransposeAfterParen)
{
    // (A * B)' — транспонирование результата выражения
    auto ast = parseSource("(A * B)';");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.strValue, "'");
    EXPECT_EQ(expr.children[0]->type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.children[0]->strValue, "*");
}

TEST_F(ParserComplexExprTest, FunctionCallInExpression)
{
    auto ast = parseSource("y = sin(x) + cos(x);");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::ASSIGN);
    EXPECT_EQ(s.children[1]->type, NodeType::BINARY_OP);
    EXPECT_EQ(s.children[1]->strValue, "+");
}

TEST_F(ParserComplexExprTest, MatrixTranspose)
{
    auto ast = parseSource("A';");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.strValue, "'");
    EXPECT_EQ(expr.children[0]->type, NodeType::IDENTIFIER);
    EXPECT_EQ(expr.children[0]->strValue, "A");
}

TEST_F(ParserComplexExprTest, ColonInIndex)
{
    // A(:, 1) — colon как «все элементы»
    auto ast = parseSource("A(:, 1);");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::CALL);
    ASSERT_EQ(expr.children.size(), 3u); // [A, :, 1]
    EXPECT_EQ(expr.children[1]->type, NodeType::COLON_EXPR);
}

// ============================================================
// Тесты: Полные программы
// ============================================================
class ParserFullProgramTest : public ::testing::Test
{};

TEST_F(ParserFullProgramTest, Fibonacci)
{
    auto ast = parseSource(R"(
        function f = fib(n)
            if n <= 1
                f = n;
            else
                f = fib(n-1) + fib(n-2);
            end
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::FUNCTION_DEF);
    EXPECT_EQ(s.strValue, "fib");
    EXPECT_EQ(s.paramNames.size(), 1u);
    EXPECT_EQ(s.returnNames.size(), 1u);
}

TEST_F(ParserFullProgramTest, BubbleSort)
{
    auto ast = parseSource(R"(
        function A = bubsort(A)
            n = length(A);
            for i = 1:n-1
                for j = 1:n-i
                    if A(j) > A(j+1)
                        tmp = A(j);
                        A(j) = A(j+1);
                        A(j+1) = tmp;
                    end
                end
            end
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::FUNCTION_DEF);
    EXPECT_EQ(s.strValue, "bubsort");
    EXPECT_EQ(s.paramNames.size(), 1u);
    EXPECT_EQ(s.paramNames[0], "A");
    EXPECT_EQ(s.returnNames.size(), 1u);
    EXPECT_EQ(s.returnNames[0], "A");
}

TEST_F(ParserFullProgramTest, ScriptWithMultipleConstructs)
{
    auto ast = parseSource(R"(
        x = 10;
        y = 20;
        if x > y
            z = x;
        else
            z = y;
        end
        for i = 1:z
            disp(i);
        end
    )");
    EXPECT_EQ(ast->type, NodeType::BLOCK);
    ASSERT_EQ(ast->children.size(), 4u); // x=10, y=20, if, for
    EXPECT_EQ(ast->children[0]->type, NodeType::ASSIGN);
    EXPECT_EQ(ast->children[1]->type, NodeType::ASSIGN);
    EXPECT_EQ(ast->children[2]->type, NodeType::IF_STMT);
    EXPECT_EQ(ast->children[3]->type, NodeType::FOR_STMT);
}

TEST_F(ParserFullProgramTest, MultipleFunctions)
{
    auto ast = parseSource(R"(
        function y = double_it(x)
            y = x * 2;
        end

        function y = triple_it(x)
            y = x * 3;
        end
    )");
    EXPECT_EQ(ast->type, NodeType::BLOCK);
    ASSERT_EQ(ast->children.size(), 2u);
    EXPECT_EQ(ast->children[0]->type, NodeType::FUNCTION_DEF);
    EXPECT_EQ(ast->children[0]->strValue, "double_it");
    EXPECT_EQ(ast->children[1]->type, NodeType::FUNCTION_DEF);
    EXPECT_EQ(ast->children[1]->strValue, "triple_it");
}

// ============================================================
// Тесты: Ошибки парсера (должны бросать исключения)
// ============================================================
class ParserErrorTest : public ::testing::Test
{};

TEST_F(ParserErrorTest, UnmatchedParen)
{
    EXPECT_ANY_THROW(parseSource("(1 + 2;"));
}

TEST_F(ParserErrorTest, UnmatchedBracket)
{
    EXPECT_ANY_THROW(parseSource("[1, 2, 3;"));
}

TEST_F(ParserErrorTest, MissingEnd)
{
    EXPECT_ANY_THROW(parseSource("if x > 0\n    y = 1;\n"));
}

TEST_F(ParserErrorTest, UnexpectedToken)
{
    EXPECT_ANY_THROW(parseSource("+ + +;"));
}

TEST_F(ParserErrorTest, MissingFunctionEnd)
{
    // В MATLAB функция без end в конце файла допустима —
    // парсер трактует EOF как неявный end (поведение single-function файлов).
    // Проверяем, что парсер не падает и корректно создаёт FUNCTION_DEF.
    auto ast = parseSource("function y = foo(x)\n    y = x;\n");
    ASSERT_EQ(ast->children.size(), 1u);
    EXPECT_EQ(ast->children[0]->type, NodeType::FUNCTION_DEF);
    EXPECT_EQ(ast->children[0]->strValue, "foo");
}

TEST_F(ParserErrorTest, ForMissingEnd)
{
    EXPECT_ANY_THROW(parseSource("for i = 1:10\n    x = i;\n"));
}

TEST_F(ParserErrorTest, WhileMissingEnd)
{
    EXPECT_ANY_THROW(parseSource("while true\n    break;\n"));
}

TEST_F(ParserErrorTest, MismatchedBrackets)
{
    EXPECT_ANY_THROW(parseSource("(1 + 2]"));
}

// ============================================================
// Тесты: Крайние случаи
// ============================================================
class ParserEdgeCaseTest : public ::testing::Test
{};

TEST_F(ParserEdgeCaseTest, SingleNumber)
{
    auto ast = parseSource("42\n");
    ASSERT_EQ(ast->children.size(), 1u);
    EXPECT_EQ(ast->children[0]->type, NodeType::EXPR_STMT);
}

TEST_F(ParserEdgeCaseTest, SingleString)
{
    auto ast = parseSource("'test'\n");
    ASSERT_EQ(ast->children.size(), 1u);
    EXPECT_EQ(ast->children[0]->type, NodeType::EXPR_STMT);
    EXPECT_EQ(ast->children[0]->children[0]->type, NodeType::STRING_LITERAL);
}

TEST_F(ParserEdgeCaseTest, NegativeNumber)
{
    auto ast = parseSource("-42;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.strValue, "-");
    EXPECT_EQ(expr.children[0]->type, NodeType::NUMBER_LITERAL);
    EXPECT_DOUBLE_EQ(expr.children[0]->numValue, 42.0);
}

TEST_F(ParserEdgeCaseTest, DeeplyNestedExpressions)
{
    auto ast = parseSource("((((((1))))));");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.children[0]->type, NodeType::NUMBER_LITERAL);
    EXPECT_DOUBLE_EQ(s.children[0]->numValue, 1.0);
}

TEST_F(ParserEdgeCaseTest, MultipleTransposes)
{
    // A'' — двойная транспозиция (identity для вещественных)
    auto ast = parseSource("A'';");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.strValue, "'");
    EXPECT_EQ(expr.children[0]->type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.children[0]->strValue, "'");
    EXPECT_EQ(expr.children[0]->children[0]->type, NodeType::IDENTIFIER);
}

TEST_F(ParserEdgeCaseTest, EmptyFunctionBody)
{
    auto ast = parseSource(R"(
        function nothing()
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::FUNCTION_DEF);
    EXPECT_EQ(s.strValue, "nothing");
    // Тело — пустой блок
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->type, NodeType::BLOCK);
    EXPECT_EQ(s.children[0]->children.size(), 0u);
}

TEST_F(ParserEdgeCaseTest, AssignFieldAccess)
{
    auto ast = parseSource("s.x = 10;");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::ASSIGN);
    EXPECT_EQ(s.children[0]->type, NodeType::FIELD_ACCESS);
    EXPECT_EQ(s.children[0]->strValue, "x");
}

TEST_F(ParserEdgeCaseTest, CellIndexAssign)
{
    auto ast = parseSource("c{1} = 42;");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::ASSIGN);
    EXPECT_EQ(s.children[0]->type, NodeType::CELL_INDEX);
}

TEST_F(ParserEdgeCaseTest, LineContinuation)
{
    // В MATLAB: ... продолжение строки
    auto ast = parseSource("x = 1 + ...\n    2;");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::ASSIGN);
    EXPECT_EQ(s.children[1]->type, NodeType::BINARY_OP);
    EXPECT_EQ(s.children[1]->strValue, "+");
}

TEST_F(ParserEdgeCaseTest, NestedCellFieldAccess)
{
    // s.data{2}.name — доступ к полю элемента cell array
    auto ast = parseSource("s.data{2}.name;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::FIELD_ACCESS);
    EXPECT_EQ(expr.strValue, "name");
}

TEST_F(ParserEdgeCaseTest, MatrixTransposeInExpression)
{
    // A' * B — транспонирование как часть выражения
    auto ast = parseSource("A' * B;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "*");
    EXPECT_EQ(expr.children[0]->type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.children[0]->strValue, "'");
}
