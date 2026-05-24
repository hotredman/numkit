// libs/graph/tests/ast_serialize_test.cpp — AST-JSON round-trip
// coverage for the IDE's AST view.
//
// Strategy: lex+parse a small .m source, run toASTJSON(*root), and
// substring-check key fields are present. We intentionally don't
// embed a JSON parser — the tests assert the wire shape only,
// which is what the frontend consumes.

#include <numkit/core/lexer.hpp>
#include <numkit/core/parser.hpp>
#include <numkit/graph/ast_serialize.hpp>

#include <gtest/gtest.h>

using namespace numkit;

namespace {

std::string serialize(const std::string &source)
{
    Lexer lex(source);
    auto tokens = lex.tokenize();
    Parser parser(tokens);
    auto root = parser.parse();
    return graph::toASTJSON(*root);
}

} // namespace

TEST(AstSerialize, EmptyScriptIsBlockWithNoChildren)
{
    auto j = serialize("");
    // Root is the BLOCK Parser::parse() returns. No children, no
    // branches, just type + line/col.
    EXPECT_NE(j.find("\"type\":\"BLOCK\""), std::string::npos);
    EXPECT_EQ(j.find("\"children\":"), std::string::npos);  // omitted when empty
}

TEST(AstSerialize, NumberLiteralRoundTripsNumValue)
{
    auto j = serialize("3.14;\n");
    EXPECT_NE(j.find("\"type\":\"NUMBER_LITERAL\""), std::string::npos);
    EXPECT_NE(j.find("\"numValue\":3.14"), std::string::npos);
}

TEST(AstSerialize, IdentifierEmitsStrValue)
{
    auto j = serialize("x;\n");
    EXPECT_NE(j.find("\"type\":\"IDENTIFIER\""), std::string::npos);
    EXPECT_NE(j.find("\"strValue\":\"x\""), std::string::npos);
}

TEST(AstSerialize, AssignmentNestingProducesChildren)
{
    auto j = serialize("y = 2 * x;\n");
    EXPECT_NE(j.find("\"type\":\"ASSIGN\""), std::string::npos);
    EXPECT_NE(j.find("\"type\":\"BINARY_OP\""), std::string::npos);
    EXPECT_NE(j.find("\"strValue\":\"*\""), std::string::npos);  // op name
    EXPECT_NE(j.find("\"strValue\":\"y\""), std::string::npos);
    EXPECT_NE(j.find("\"strValue\":\"x\""), std::string::npos);
}

TEST(AstSerialize, IfElseSerializesBranchesAndElseBranch)
{
    auto j = serialize("if x > 0\n  y = 1;\nelse\n  y = 2;\nend\n");
    EXPECT_NE(j.find("\"type\":\"IF_STMT\""),  std::string::npos);
    EXPECT_NE(j.find("\"branches\":"),         std::string::npos);
    EXPECT_NE(j.find("\"cond\":"),             std::string::npos);
    EXPECT_NE(j.find("\"body\":"),             std::string::npos);
    EXPECT_NE(j.find("\"elseBranch\":"),       std::string::npos);
}

TEST(AstSerialize, FunctionDefEmitsParamsAndReturns)
{
    auto j = serialize("function [a, b] = swap(x, y)\n  a = y;\n  b = x;\nend\n");
    EXPECT_NE(j.find("\"type\":\"FUNCTION_DEF\""),       std::string::npos);
    EXPECT_NE(j.find("\"strValue\":\"swap\""),           std::string::npos);
    EXPECT_NE(j.find("\"paramNames\":[\"x\",\"y\"]"),    std::string::npos);
    EXPECT_NE(j.find("\"returnNames\":[\"a\",\"b\"]"),   std::string::npos);
}

TEST(AstSerialize, SuppressOutputOnlyWhenTrue)
{
    auto sup = serialize("x = 1;\n");
    EXPECT_NE(sup.find("\"suppressOutput\":true"), std::string::npos);
    auto vis = serialize("x = 1\n");
    EXPECT_EQ(vis.find("\"suppressOutput\":true"), std::string::npos);  // false ⇒ omitted
}

TEST(AstSerialize, EndPositionEmittedWhenSet)
{
    // FOR has an endLine = line of the closing `end` keyword.
    auto fr = serialize("for k = 1:3\n  a = k;\nend\n");
    EXPECT_NE(fr.find("\"endLine\":3"), std::string::npos);
    // Simple stmts with a `;` terminator now also carry endLine /
    // endCol — the parser records the terminator position via
    // consumeStmtTerminator(). So `x = 1;` has endLine=1 and
    // endCol pointing one past the semicolon.
    auto simple = serialize("x = 1;\n");
    EXPECT_NE(simple.find("\"endLine\":1"), std::string::npos);
    EXPECT_NE(simple.find("\"endCol\":"),   std::string::npos);
}

TEST(AstSerialize, EscapesStringContents)
{
    auto j = serialize("msg = 'a \"b\" c';\n");
    // The strValue inside should be JSON-escaped with backslash quotes.
    EXPECT_NE(j.find("\\\"b\\\""), std::string::npos);
}

TEST(AstSerialize, NaNLiteralEmittedAsString)
{
    // We don't actually evaluate at parse time, but if the user wrote
    // `x = NaN;`, NaN is an IDENTIFIER (builtin constant). However the
    // writer also handles raw NaN doubles defensively for any future
    // code path that puts NaN in numValue. This test exercises the
    // helper indirectly via a hand-rolled ASTNode.
    auto node = std::make_unique<ASTNode>(NodeType::NUMBER_LITERAL);
    node->line = 1; node->col = 1;
    node->numValue = std::nan("");
    auto j = graph::toASTJSON(*node);
    EXPECT_NE(j.find("\"numValue\":\"NaN\""), std::string::npos);
}
