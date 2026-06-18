// codegen/tests/emitter_test.cpp
//
// Unit tests for the C++ emitter — sub-brick 3a: unboxed-scalar
// expressions. Parse a scalar expression, emit C++, assert the string.
// (String-level verification; end-to-end compile+run+diff arrives with
// the AOT harness, brick 4.)

#include <numkit/codegen/emitter.hpp>

#include <numkit/core/lexer.hpp>
#include <numkit/core/parser.hpp>

#include <gtest/gtest.h>

using numkit::ValueType;
using namespace numkit::codegen;

namespace {

// Parse "__e = (<expr>);" and emit the right-hand side expression.
std::string emit(const std::string &expr)
{
    numkit::Lexer  lex("__e = (" + expr + ");");
    auto           toks = lex.tokenize();
    numkit::Parser parser(toks);
    auto           root = parser.parse();
    const numkit::ASTNode &assign = *root->children.back();  // ASSIGN
    return emitScalarExpr(*assign.children[1]);               // rhs
}

}  // namespace

TEST(Emitter, ScalarType)
{
    EXPECT_EQ(cppScalarType(ValueType::DOUBLE), "double");
    EXPECT_EQ(cppScalarType(ValueType::SINGLE), "float");
    EXPECT_EQ(cppScalarType(ValueType::COMPLEX), "std::complex<double>");
    EXPECT_EQ(cppScalarType(ValueType::LOGICAL), "bool");
    EXPECT_EQ(cppScalarType(ValueType::INT32), "std::int32_t");
    EXPECT_THROW(cppScalarType(ValueType::CELL), std::runtime_error);
}

TEST(Emitter, DoubleLiteralFormat)
{
    EXPECT_EQ(formatDoubleLiteral(0.0675), "0.0675");
    EXPECT_EQ(formatDoubleLiteral(2.0), "2.0");      // integer-valued -> .0
    EXPECT_EQ(formatDoubleLiteral(-1.143), "-1.143");
    EXPECT_EQ(formatDoubleLiteral(0.0), "0.0");
}

TEST(Emitter, NumberAndIdentifier)
{
    EXPECT_EQ(emit("0.0675"), "0.0675");
    EXPECT_EQ(emit("xn"), "xn");
}

TEST(Emitter, ScalarArithmetic)
{
    EXPECT_EQ(emit("b0*xn"), "(b0 * xn)");
    EXPECT_EQ(emit("b0*xn + b1*x1"), "((b0 * xn) + (b1 * x1))");
}

// The biquad inner update emits as a plain nested C++ double expression.
TEST(Emitter, BiquadBody)
{
    // left-associative: ((((b0*xn + b1*x1) + b2*x2) - a1*y1) - a2*y2)
    EXPECT_EQ(emit("b0*xn + b1*x1 + b2*x2 - a1*y1 - a2*y2"),
              "(((((b0 * xn) + (b1 * x1)) + (b2 * x2)) - (a1 * y1)) - (a2 * y2))");
}

TEST(Emitter, PowerLowersToStdPow)
{
    EXPECT_EQ(emit("2^3"), "std::pow(2.0, 3.0)");
    EXPECT_EQ(emit("x.^2"), "std::pow(x, 2.0)");
}

TEST(Emitter, ComparisonAndLogical)
{
    EXPECT_EQ(emit("a < b"), "(a < b)");
    EXPECT_EQ(emit("a == b"), "(a == b)");
    EXPECT_EQ(emit("a ~= b"), "(a != b)");
}

TEST(Emitter, Unary)
{
    EXPECT_EQ(emit("-x"), "(-x)");
    EXPECT_EQ(emit("~b"), "(!b)");
}

TEST(Emitter, ImaginaryLiteral)
{
    EXPECT_EQ(emit("2i"), "std::complex<double>(0.0, 2.0)");
}

// A construct outside this sub-brick (a call) throws rather than emitting
// something wrong — the boundary is explicit.
TEST(Emitter, UnsupportedThrows)
{
    EXPECT_THROW(emit("foo(x)"), std::runtime_error);
}