// tests/command_style_test.cpp
//
// Тесты для command-style вызовов: clear all, grid on, cd dir, и т.д.
// В MATLAB command syntax — это сахар для вызова функции со строковыми аргументами:
//   clear all  ≡  clear('all')
//   grid on    ≡  grid('on')
//   load data.mat x y  ≡  load('data.mat','x','y')

#include "MAst.hpp"
#include "MLexer.hpp"
#include "MParser.hpp"
#include <gtest/gtest.h>

using namespace numkit;

static ASTNodePtr parseSource(const std::string &source)
{
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    return parser.parse();
}

static const ASTNode &stmt(const ASTNode &block, size_t i)
{
    EXPECT_EQ(block.type, NodeType::BLOCK);
    EXPECT_GT(block.children.size(), i);
    return *block.children[i];
}

class CommandStyleTest : public ::testing::Test
{};

// ============================================================
// Базовые command-style вызовы
// ============================================================

TEST_F(CommandStyleTest, ClearAll)
{
    auto ast = parseSource("clear all\n");
    ASSERT_EQ(ast->children.size(), 1u);
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "clear");
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->type, NodeType::STRING_LITERAL);
    EXPECT_EQ(s.children[0]->strValue, "all");
    EXPECT_FALSE(s.suppressOutput);
}

TEST_F(CommandStyleTest, GridOn)
{
    auto ast = parseSource("grid on\n");
    ASSERT_EQ(ast->children.size(), 1u);
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "grid");
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->strValue, "on");
}

TEST_F(CommandStyleTest, GridOff)
{
    auto ast = parseSource("grid off\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "grid");
    EXPECT_EQ(s.children[0]->strValue, "off");
}

TEST_F(CommandStyleTest, HoldOn)
{
    auto ast = parseSource("hold on\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "hold");
    EXPECT_EQ(s.children[0]->strValue, "on");
}

TEST_F(CommandStyleTest, FormatLong)
{
    auto ast = parseSource("format long\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "format");
    EXPECT_EQ(s.children[0]->strValue, "long");
}

TEST_F(CommandStyleTest, FormatShortE)
{
    auto ast = parseSource("format shortE\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "format");
    EXPECT_EQ(s.children[0]->strValue, "shortE");
}

TEST_F(CommandStyleTest, DispString)
{
    auto ast = parseSource("disp hello\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "disp");
    EXPECT_EQ(s.children[0]->strValue, "hello");
}

TEST_F(CommandStyleTest, TypeFilename)
{
    auto ast = parseSource("type myfile\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "type");
    EXPECT_EQ(s.children[0]->strValue, "myfile");
}

TEST_F(CommandStyleTest, HelpPlot)
{
    auto ast = parseSource("help plot\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "help");
    EXPECT_EQ(s.children[0]->strValue, "plot");
}

TEST_F(CommandStyleTest, DocSin)
{
    auto ast = parseSource("doc sin\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "doc");
    EXPECT_EQ(s.children[0]->strValue, "sin");
}

// ============================================================
// Несколько аргументов
// ============================================================

TEST_F(CommandStyleTest, ClearMultipleVars)
{
    auto ast = parseSource("clear x y z\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "clear");
    ASSERT_EQ(s.children.size(), 3u);
    EXPECT_EQ(s.children[0]->strValue, "x");
    EXPECT_EQ(s.children[1]->strValue, "y");
    EXPECT_EQ(s.children[2]->strValue, "z");
}

TEST_F(CommandStyleTest, LoadWithMultipleArgs)
{
    // load data.mat x y  →  load('data.mat', 'x', 'y')
    auto ast = parseSource("load data.mat x y\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "load");
    ASSERT_EQ(s.children.size(), 3u);
    // data.mat склеивается в один аргумент
    EXPECT_EQ(s.children[0]->strValue, "data.mat");
    EXPECT_EQ(s.children[1]->strValue, "x");
    EXPECT_EQ(s.children[2]->strValue, "y");
}

TEST_F(CommandStyleTest, SaveFilename)
{
    auto ast = parseSource("save results.mat\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "save");
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->strValue, "results.mat");
}

// ============================================================
// Semicolon / suppress
// ============================================================

TEST_F(CommandStyleTest, WithSemicolon)
{
    auto ast = parseSource("grid on;\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "grid");
    EXPECT_EQ(s.children[0]->strValue, "on");
    EXPECT_TRUE(s.suppressOutput);
}

TEST_F(CommandStyleTest, ClearAllSemicolon)
{
    auto ast = parseSource("clear all;");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "clear");
    EXPECT_TRUE(s.suppressOutput);
}

// ============================================================
// НЕ command-style: нормальные выражения
// ============================================================

TEST_F(CommandStyleTest, NotCommandAssignment)
{
    // x = 5 — обычное присваивание
    auto ast = parseSource("x = 5;\n");
    ASSERT_EQ(ast->children.size(), 1u);
    EXPECT_EQ(stmt(*ast, 0).type, NodeType::ASSIGN);
}

TEST_F(CommandStyleTest, NotCommandFunctionCall)
{
    // plot(x, y) — обычный вызов со скобками
    auto ast = parseSource("plot(x, y);\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::EXPR_STMT);
    EXPECT_EQ(s.children[0]->type, NodeType::CALL);
}

TEST_F(CommandStyleTest, NotCommandBinaryOp)
{
    // a + b — бинарная операция, НЕ command
    auto ast = parseSource("a + b;\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::EXPR_STMT);
    EXPECT_EQ(s.children[0]->type, NodeType::BINARY_OP);
}

TEST_F(CommandStyleTest, NotCommandDotAccess)
{
    // s.field — доступ к полю, НЕ command
    auto ast = parseSource("s.field;\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::EXPR_STMT);
    EXPECT_EQ(s.children[0]->type, NodeType::FIELD_ACCESS);
}

TEST_F(CommandStyleTest, NotCommandTranspose)
{
    // A' — транспонирование, НЕ command
    auto ast = parseSource("A';\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::EXPR_STMT);
    EXPECT_EQ(s.children[0]->type, NodeType::UNARY_OP);
    EXPECT_EQ(s.children[0]->strValue, "'");
}

TEST_F(CommandStyleTest, NotCommandComparison)
{
    // a == b — сравнение
    auto ast = parseSource("a == b;\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::EXPR_STMT);
    EXPECT_EQ(s.children[0]->type, NodeType::BINARY_OP);
}

TEST_F(CommandStyleTest, NotCommandColon)
{
    // a:b — range expression
    auto ast = parseSource("a:b;\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::EXPR_STMT);
    EXPECT_EQ(s.children[0]->type, NodeType::COLON_EXPR);
}

TEST_F(CommandStyleTest, NotCommandPower)
{
    // a ^ b — степень
    auto ast = parseSource("a ^ b;\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::EXPR_STMT);
    EXPECT_EQ(s.children[0]->type, NodeType::BINARY_OP);
    EXPECT_EQ(s.children[0]->strValue, "^");
}

TEST_F(CommandStyleTest, NotCommandMul)
{
    // a * b — умножение
    auto ast = parseSource("a * b;\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::EXPR_STMT);
    EXPECT_EQ(s.children[0]->type, NodeType::BINARY_OP);
}

TEST_F(CommandStyleTest, NotCommandDiv)
{
    // a / b — деление
    auto ast = parseSource("a / b;\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::EXPR_STMT);
    EXPECT_EQ(s.children[0]->type, NodeType::BINARY_OP);
}

TEST_F(CommandStyleTest, NotCommandSingleIdentifier)
{
    // x — просто идентификатор (не command, нет аргументов)
    auto ast = parseSource("x;\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::EXPR_STMT);
    EXPECT_EQ(s.children[0]->type, NodeType::IDENTIFIER);
}

TEST_F(CommandStyleTest, NotCommandCellIndex)
{
    // c{1} — cell indexing
    auto ast = parseSource("c{1};\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::EXPR_STMT);
    EXPECT_EQ(s.children[0]->type, NodeType::CELL_INDEX);
}

TEST_F(CommandStyleTest, NotCommandLogicalAnd)
{
    // a && b
    auto ast = parseSource("a && b;\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::EXPR_STMT);
    EXPECT_EQ(s.children[0]->type, NodeType::BINARY_OP);
}

TEST_F(CommandStyleTest, NotCommandLogicalOr)
{
    // a || b
    auto ast = parseSource("a || b;\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::EXPR_STMT);
    EXPECT_EQ(s.children[0]->type, NodeType::BINARY_OP);
}

// ============================================================
// Внутри control flow
// ============================================================

TEST_F(CommandStyleTest, InsideIfBlock)
{
    std::string src = "if flag\n"
                      "    hold on\n"
                      "    grid on\n"
                      "end\n";
    auto ast = parseSource(src);
    ASSERT_EQ(ast->children.size(), 1u);
    const auto &ifNode = stmt(*ast, 0);
    EXPECT_EQ(ifNode.type, NodeType::IF_STMT);
    // Тело: 2 command call
    auto &body = ifNode.branches[0].second;
    ASSERT_EQ(body->children.size(), 2u);
    EXPECT_EQ(body->children[0]->type, NodeType::COMMAND_CALL);
    EXPECT_EQ(body->children[0]->strValue, "hold");
    EXPECT_EQ(body->children[1]->type, NodeType::COMMAND_CALL);
    EXPECT_EQ(body->children[1]->strValue, "grid");
}

TEST_F(CommandStyleTest, InsideForLoop)
{
    std::string src = "for i = 1:3\n"
                      "    disp hello\n"
                      "end\n";
    auto ast = parseSource(src);
    const auto &forNode = stmt(*ast, 0);
    EXPECT_EQ(forNode.type, NodeType::FOR_STMT);
    auto &body = forNode.children[1]; // children[0] = range, [1] = block
    ASSERT_EQ(body->children.size(), 1u);
    EXPECT_EQ(body->children[0]->type, NodeType::COMMAND_CALL);
    EXPECT_EQ(body->children[0]->strValue, "disp");
}

TEST_F(CommandStyleTest, InsideFunctionDef)
{
    std::string src = "function myplot(x)\n"
                      "    hold on\n"
                      "    grid on\n"
                      "    plot(x)\n"
                      "end\n";
    auto ast = parseSource(src);
    const auto &fn = stmt(*ast, 0);
    EXPECT_EQ(fn.type, NodeType::FUNCTION_DEF);
    auto &body = fn.children[0];
    ASSERT_EQ(body->children.size(), 3u);
    EXPECT_EQ(body->children[0]->type, NodeType::COMMAND_CALL);
    EXPECT_EQ(body->children[0]->strValue, "hold");
    EXPECT_EQ(body->children[1]->type, NodeType::COMMAND_CALL);
    EXPECT_EQ(body->children[1]->strValue, "grid");
    EXPECT_EQ(body->children[2]->type, NodeType::EXPR_STMT); // plot(x)
}

// ============================================================
// Несколько statements в строке (;)
// ============================================================

TEST_F(CommandStyleTest, MultipleOnOneLine)
{
    auto ast = parseSource("hold on; grid on\n");
    ASSERT_EQ(ast->children.size(), 2u);
    EXPECT_EQ(stmt(*ast, 0).type, NodeType::COMMAND_CALL);
    EXPECT_EQ(stmt(*ast, 0).strValue, "hold");
    EXPECT_TRUE(stmt(*ast, 0).suppressOutput);
    EXPECT_EQ(stmt(*ast, 1).type, NodeType::COMMAND_CALL);
    EXPECT_EQ(stmt(*ast, 1).strValue, "grid");
}

// ============================================================
// С комментариями
// ============================================================

TEST_F(CommandStyleTest, WithComment)
{
    auto ast = parseSource("clear all % remove everything\n");
    ASSERT_EQ(ast->children.size(), 1u);
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "clear");
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->strValue, "all");
}

// ============================================================
// Числовые аргументы
// ============================================================

TEST_F(CommandStyleTest, CommandWithNumber)
{
    // Некоторые функции принимают числа: dbstop 42, echo 1, etc.
    auto ast = parseSource("echo 1\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "echo");
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->type, NodeType::STRING_LITERAL);
    EXPECT_EQ(s.children[0]->strValue, "1");
}

// ============================================================
// Строковые аргументы (кавычки)
// ============================================================

TEST_F(CommandStyleTest, CommandWithDoubleQuotedString)
{
    // disp "hello world"  — double-quoted strings work in command context
    auto ast = parseSource("disp \"hello world\"\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "disp");
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->strValue, "hello world");
}

TEST_F(CommandStyleTest, CommandSingleQuoteNeedsParens)
{
    // Примечание: disp 'hello' не работает в command-style из-за
    // того что лексер видит ' после identifier как транспонирование.
    // В реальном MATLAB это решается на уровне лексера.
    // Workaround: disp("hello") или disp "hello"
    auto ast = parseSource("disp('hello')\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::EXPR_STMT);
    EXPECT_EQ(s.children[0]->type, NodeType::CALL);
}

// ============================================================
// Dotted file paths / packages
// ============================================================

TEST_F(CommandStyleTest, DottedFilename)
{
    auto ast = parseSource("edit myfile.m\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "edit");
    ASSERT_EQ(s.children.size(), 1u);
    EXPECT_EQ(s.children[0]->strValue, "myfile.m");
}

TEST_F(CommandStyleTest, LoadMatFile)
{
    auto ast = parseSource("load workspace.mat\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "load");
    EXPECT_EQ(s.children[0]->strValue, "workspace.mat");
}

// ============================================================
// Реалистичный скрипт
// ============================================================

TEST_F(CommandStyleTest, RealisticPlotScript)
{
    std::string src = "x = 0:0.1:10;\n"
                      "y = sin(x);\n"
                      "figure\n"
                      "hold on\n"
                      "plot(x, y)\n"
                      "grid on\n"
                      "title('My Plot')\n"
                      "hold off\n";
    auto ast = parseSource(src);
    ASSERT_EQ(ast->children.size(), 8u);

    EXPECT_EQ(stmt(*ast, 0).type, NodeType::ASSIGN);      // x = ...
    EXPECT_EQ(stmt(*ast, 1).type, NodeType::ASSIGN);      // y = ...
    EXPECT_EQ(stmt(*ast, 2).type, NodeType::EXPR_STMT);   // figure (no args → regular id)
    EXPECT_EQ(stmt(*ast, 3).type, NodeType::COMMAND_CALL); // hold on
    EXPECT_EQ(stmt(*ast, 3).strValue, "hold");
    EXPECT_EQ(stmt(*ast, 4).type, NodeType::EXPR_STMT);   // plot(x,y)
    EXPECT_EQ(stmt(*ast, 5).type, NodeType::COMMAND_CALL); // grid on
    EXPECT_EQ(stmt(*ast, 5).strValue, "grid");
    EXPECT_EQ(stmt(*ast, 6).type, NodeType::EXPR_STMT);   // title(...)
    EXPECT_EQ(stmt(*ast, 7).type, NodeType::COMMAND_CALL); // hold off
    EXPECT_EQ(stmt(*ast, 7).strValue, "hold");
}

// ============================================================
// Edge cases
// ============================================================

TEST_F(CommandStyleTest, CommandNoArgsStaysExprStmt)
{
    // figure без аргументов → обычный EXPR_STMT(IDENTIFIER)
    auto ast = parseSource("figure\n");
    ASSERT_EQ(ast->children.size(), 1u);
    EXPECT_EQ(stmt(*ast, 0).type, NodeType::EXPR_STMT);
    EXPECT_EQ(stmt(*ast, 0).children[0]->type, NodeType::IDENTIFIER);
    EXPECT_EQ(stmt(*ast, 0).children[0]->strValue, "figure");
}

TEST_F(CommandStyleTest, CommandFollowedBySemicolonNoArgs)
{
    // clc; — нет аргументов, обычный expr stmt
    auto ast = parseSource("clc;\n");
    ASSERT_EQ(ast->children.size(), 1u);
    EXPECT_EQ(stmt(*ast, 0).type, NodeType::EXPR_STMT);
    EXPECT_EQ(stmt(*ast, 0).children[0]->strValue, "clc");
}

TEST_F(CommandStyleTest, MultiAssignNotAffected)
{
    // [a, b] = size(x) — multi-assign, не command
    auto ast = parseSource("[a, b] = size(x);\n");
    ASSERT_EQ(ast->children.size(), 1u);
    EXPECT_EQ(stmt(*ast, 0).type, NodeType::MULTI_ASSIGN);
}

TEST_F(CommandStyleTest, WhoseVars)
{
    auto ast = parseSource("whos x y\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "whos");
    ASSERT_EQ(s.children.size(), 2u);
    EXPECT_EQ(s.children[0]->strValue, "x");
    EXPECT_EQ(s.children[1]->strValue, "y");
}

TEST_F(CommandStyleTest, CommandWithTrueArg)
{
    // set vis true  (hypothetical example)
    auto ast = parseSource("set vis true\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "set");
    ASSERT_EQ(s.children.size(), 2u);
    EXPECT_EQ(s.children[0]->strValue, "vis");
    EXPECT_EQ(s.children[1]->strValue, "true");
}

TEST_F(CommandStyleTest, ClearClasses)
{
    auto ast = parseSource("clear classes\n");
    const auto &s = stmt(*ast, 0);
    EXPECT_EQ(s.type, NodeType::COMMAND_CALL);
    EXPECT_EQ(s.strValue, "clear");
    EXPECT_EQ(s.children[0]->strValue, "classes");
}