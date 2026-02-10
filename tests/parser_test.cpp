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
    auto ast = parseSource("a || b && c;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    EXPECT_EQ(expr.strValue, "||");
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
    auto ast = parseSource("+x;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.strValue, "+");
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

TEST_F(ParserUnaryOpTest, DoubleNegation)
{
    auto ast = parseSource("--x;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.strValue, "-");
    EXPECT_EQ(expr.children[0]->type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.children[0]->strValue, "-");
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
    EXPECT_EQ(s.type, NodeType::EXPR_STMT);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::ASSIGN);
    ASSERT_EQ(expr.children.size(), 2u);
    EXPECT_EQ(expr.children[0]->type, NodeType::IDENTIFIER);
    EXPECT_EQ(expr.children[0]->strValue, "x");
    EXPECT_EQ(expr.children[1]->type, NodeType::NUMBER_LITERAL);
}

TEST_F(ParserAssignTest, AssignExpression)
{
    auto ast = parseSource("x = 1 + 2;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::ASSIGN);
    EXPECT_EQ(expr.children[1]->type, NodeType::BINARY_OP);
}

TEST_F(ParserAssignTest, IndexedAssign)
{
    auto ast = parseSource("x(1) = 10;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::ASSIGN);
    EXPECT_EQ(expr.children[0]->type, NodeType::INDEX);
}

TEST_F(ParserAssignTest, MultiAssign)
{
    auto ast = parseSource("[a, b] = func();");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::MULTI_ASSIGN);
}

TEST_F(ParserAssignTest, MultiAssignThreeOutputs)
{
    auto ast = parseSource("[a, b, c] = func();");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::MULTI_ASSIGN);
    EXPECT_EQ(s.returnNames.size(), 3u);
    EXPECT_EQ(s.returnNames[0], "a");
    EXPECT_EQ(s.returnNames[1], "b");
    EXPECT_EQ(s.returnNames[2], "c");
}

TEST_F(ParserAssignTest, MultiAssignWithTilde)
{
    auto ast = parseSource("[~, b] = func();");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::MULTI_ASSIGN);
    EXPECT_EQ(s.returnNames[0], "~");
    EXPECT_EQ(s.returnNames[1], "b");
}

// ============================================================
// Тесты: Индексация / Вызов функций
// ============================================================
class ParserIndexCallTest : public ::testing::Test
{};

TEST_F(ParserIndexCallTest, SingleIndex)
{
    auto ast = parseSource("x(1);");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    // В MATLAB x(1) может быть INDEX или CALL — зависит от реализации
    EXPECT_TRUE(expr.type == NodeType::INDEX || expr.type == NodeType::CALL);
}

TEST_F(ParserIndexCallTest, MultiIndex)
{
    auto ast = parseSource("A(1, 2);");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_TRUE(expr.type == NodeType::INDEX || expr.type == NodeType::CALL);
    // Должно быть 3 children: объект + 2 индекса, или объект + аргументы
    EXPECT_GE(expr.children.size(), 2u);
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
    EXPECT_TRUE(expr.type == NodeType::INDEX || expr.type == NodeType::CALL);
}

TEST_F(ParserIndexCallTest, ChainedIndexing)
{
    auto ast = parseSource("x(1)(2);");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    // Внешний — INDEX/CALL, внутренний child[0] тоже INDEX/CALL
    EXPECT_TRUE(expr.type == NodeType::INDEX || expr.type == NodeType::CALL);
}

TEST_F(ParserIndexCallTest, NestedCalls)
{
    auto ast = parseSource("foo(bar(x));");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_TRUE(expr.type == NodeType::INDEX || expr.type == NodeType::CALL);
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
    auto ast = parseSource("a.b.c;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::FIELD_ACCESS);
    EXPECT_EQ(expr.strValue, "c");
    EXPECT_EQ(expr.children[0]->type, NodeType::FIELD_ACCESS);
    EXPECT_EQ(expr.children[0]->strValue, "b");
}

// ============================================================
// Тесты: Colon-выражения
// ============================================================
class ParserColonExprTest : public ::testing::Test
{};

TEST_F(ParserColonExprTest, SimpleRange)
{
    auto ast = parseSource("1:10;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::COLON_EXPR);
    ASSERT_GE(expr.children.size(), 2u);
    EXPECT_DOUBLE_EQ(expr.children[0]->numValue, 1.0);
    EXPECT_DOUBLE_EQ(expr.children[1]->numValue, 10.0);
}

TEST_F(ParserColonExprTest, SteppedRange)
{
    auto ast = parseSource("1:2:10;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::COLON_EXPR);
    ASSERT_EQ(expr.children.size(), 3u);
}

TEST_F(ParserColonExprTest, ColonWithExpressions)
{
    auto ast = parseSource("a:b;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::COLON_EXPR);
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
    auto ast = parseSource("[1, 2, 3];");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::MATRIX_LITERAL);
    EXPECT_GE(expr.children.size(), 1u);
}

TEST_F(ParserMatrixLiteralTest, ColumnVector)
{
    auto ast = parseSource("[1; 2; 3];");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::MATRIX_LITERAL);
}

TEST_F(ParserMatrixLiteralTest, Matrix2x2)
{
    auto ast = parseSource("[1, 2; 3, 4];");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::MATRIX_LITERAL);
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
}

TEST_F(ParserCellLiteralTest, CellWithElements)
{
    auto ast = parseSource("{1, 'hello', true};");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::CELL_LITERAL);
    EXPECT_GE(expr.children.size(), 1u);
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
    EXPECT_GE(s.branches.size(), 1u);
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
    EXPECT_GE(s.branches.size(), 1u);
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
    EXPECT_GE(s.branches.size(), 2u);
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
    // Проверяем что внутри тела первого if есть вложенный if
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
    ASSERT_GE(s.children.size(), 2u);
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
    // Тело содержит вложенный for
    ASSERT_GE(s.children.size(), 2u);
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
    ASSERT_GE(s.children.size(), 2u);
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
    // Тело while — Block, внутри которого break
    const auto &body = *w.children[1]; // или children[1]
    // Ищем BREAK_STMT
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
    EXPECT_GE(s.branches.size(), 2u); // at least 2 cases
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
    EXPECT_EQ(s.paramNames.size(), 1u);
    EXPECT_EQ(s.paramNames[0], "x");
    EXPECT_EQ(s.returnNames.size(), 1u);
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
    EXPECT_EQ(s.paramNames.size(), 1u);
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
    EXPECT_EQ(s.returnNames.size(), 2u);
    EXPECT_EQ(s.returnNames[0], "a");
    EXPECT_EQ(s.returnNames[1], "b");
    EXPECT_EQ(s.paramNames.size(), 2u);
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
}

TEST_F(ParserFunctionTest, FunctionNoArgsNoParens)
{
    auto ast = parseSource(R"(
        function x = getval
            x = 42;
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::FUNCTION_DEF);
    EXPECT_EQ(s.strValue, "getval");
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
    const auto &assign = *s.children[0];
    EXPECT_EQ(assign.type, NodeType::ASSIGN);
    EXPECT_EQ(assign.children[1]->type, NodeType::ANON_FUNC);
}

TEST_F(ParserAnonFuncTest, AnonMultiParam)
{
    auto ast = parseSource("f = @(x, y) x + y;");
    const auto &s = stmt(*ast, 0);
    const auto &assign = *s.children[0];
    const auto &anon = *assign.children[1];
    EXPECT_EQ(anon.type, NodeType::ANON_FUNC);
    EXPECT_EQ(anon.paramNames.size(), 2u);
}

TEST_F(ParserAnonFuncTest, FunctionHandle)
{
    auto ast = parseSource("f = @sin;");
    const auto &s = stmt(*ast, 0);
    const auto &assign = *s.children[0];
    // Может быть ANON_FUNC или другой тип — зависит от реализации
    EXPECT_NE(assign.children[1], nullptr);
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
}

TEST_F(ParserGlobalPersistentTest, PersistentStatement)
{
    auto ast = parseSource("persistent count;");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::PERSISTENT_STMT);
}

// ============================================================
// Тесты: end как значение
// ============================================================
class ParserEndValTest : public ::testing::Test
{};

TEST_F(ParserEndValTest, EndInIndex)
{
    auto ast = parseSource("x(end);");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    // Внутри аргументов индекса должен быть END_VAL
    EXPECT_TRUE(expr.type == NodeType::INDEX || expr.type == NodeType::CALL);
}

TEST_F(ParserEndValTest, EndMinusOne)
{
    auto ast = parseSource("x(end-1);");
    const auto &s = stmt(*ast, 0);
    EXPECT_TRUE(s.children[0]->type == NodeType::INDEX || s.children[0]->type == NodeType::CALL);
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
}

TEST_F(ParserComplexExprTest, MixedOperators)
{
    auto ast = parseSource("a + b * c - d / e;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::BINARY_OP);
    // Верхний уровень — вычитание (или сложение, зависит от ассоциативности)
}

TEST_F(ParserComplexExprTest, TransposeAfterParen)
{
    auto ast = parseSource("(A * B)';");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.strValue, "'");
}

TEST_F(ParserComplexExprTest, FunctionCallInExpression)
{
    auto ast = parseSource("y = sin(x) + cos(x);");
    const auto &s = stmt(*ast, 0);
    const auto &assign = *s.children[0];
    EXPECT_EQ(assign.type, NodeType::ASSIGN);
    EXPECT_EQ(assign.children[1]->type, NodeType::BINARY_OP);
}

TEST_F(ParserComplexExprTest, MatrixTranspose)
{
    auto ast = parseSource("A';");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.strValue, "'");
}

TEST_F(ParserComplexExprTest, ColonInMatrix)
{
    auto ast = parseSource("A(:, 1);");
    const auto &s = stmt(*ast, 0);
    EXPECT_NE(s.children[0], nullptr);
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
    EXPECT_GE(ast->children.size(), 4u);
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
    EXPECT_EQ(ast->children.size(), 2u);
    EXPECT_EQ(ast->children[0]->type, NodeType::FUNCTION_DEF);
    EXPECT_EQ(ast->children[1]->type, NodeType::FUNCTION_DEF);
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
    EXPECT_ANY_THROW(parseSource("function y = foo(x)\n    y = x;\n"));
}

TEST_F(ParserErrorTest, ForMissingEnd)
{
    EXPECT_ANY_THROW(parseSource("for i = 1:10\n    x = i;\n"));
}

TEST_F(ParserErrorTest, WhileMissingEnd)
{
    EXPECT_ANY_THROW(parseSource("while true\n    break;\n"));
}

// ============================================================
// Тесты: Крайние случаи
// ============================================================
class ParserEdgeCaseTest : public ::testing::Test
{};

TEST_F(ParserEdgeCaseTest, SingleNumber)
{
    auto ast = parseSource("42\n");
    EXPECT_EQ(ast->children.size(), 1u);
}

TEST_F(ParserEdgeCaseTest, SingleString)
{
    auto ast = parseSource("'test'\n");
    EXPECT_EQ(ast->children.size(), 1u);
    EXPECT_EQ(ast->children[0]->children[0]->type, NodeType::STRING_LITERAL);
}

TEST_F(ParserEdgeCaseTest, NegativeNumber)
{
    auto ast = parseSource("-42;");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.strValue, "-");
}

TEST_F(ParserEdgeCaseTest, MatrixWithExpressions)
{
    auto ast = parseSource("[1+2, 3*4; 5-6, 7/8];");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.children[0]->type, NodeType::MATRIX_LITERAL);
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
    auto ast = parseSource("A'';");
    const auto &s = stmt(*ast, 0);
    const auto &expr = *s.children[0];
    EXPECT_EQ(expr.type, NodeType::UNARY_OP);
    EXPECT_EQ(expr.strValue, "'");
    EXPECT_EQ(expr.children[0]->type, NodeType::UNARY_OP);
}

TEST_F(ParserEdgeCaseTest, EmptyFunctionBody)
{
    auto ast = parseSource(R"(
        function nothing()
        end
    )");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::FUNCTION_DEF);
}

TEST_F(ParserEdgeCaseTest, AssignFieldAccess)
{
    auto ast = parseSource("s.x = 10;");
    const auto &s = stmt(*ast, 0);
    const auto &assign = *s.children[0];
    EXPECT_EQ(assign.type, NodeType::ASSIGN);
    EXPECT_EQ(assign.children[0]->type, NodeType::FIELD_ACCESS);
}

TEST_F(ParserEdgeCaseTest, CellIndexAssign)
{
    auto ast = parseSource("c{1} = 42;");
    const auto &s = stmt(*ast, 0);
    const auto &assign = *s.children[0];
    EXPECT_EQ(assign.type, NodeType::ASSIGN);
    EXPECT_EQ(assign.children[0]->type, NodeType::CELL_INDEX);
}

TEST_F(ParserEdgeCaseTest, LineContinuation)
{
    auto ast = parseSource("x = 1 + ...\n    2;");
    const auto &s = stmt(*ast, 0);
    const auto &assign = *s.children[0];
    EXPECT_EQ(assign.type, NodeType::ASSIGN);
    EXPECT_EQ(assign.children[1]->type, NodeType::BINARY_OP);
}
