// codegen/tests/inference_test.cpp
//
// End-to-end inference over REAL parsed code: lex+parse a snippet, run
// the straight-line inference driver, and assert the inferred type of a
// variable. This is where the lattice + ConstVal + transfer registry
// come together into actual type inference — including the two wins that
// matter: constant propagation into shape (n=5 -> zeros(n) is 5x5) and
// scalar element access (x(i) is an unboxed scalar).

#include <numkit/codegen/inference.hpp>
#include <numkit/codegen/transfer.hpp>

#include <numkit/core/lexer.hpp>
#include <numkit/core/parser.hpp>

#include <gtest/gtest.h>

using numkit::ValueType;
using namespace numkit::codegen;

namespace {

// Lex + parse + infer a snippet; return the final environment.
TypeEnv inferSrc(const std::string &src)
{
    numkit::Lexer  lex(src);
    auto           tokens = lex.tokenize();
    numkit::Parser parser(tokens);
    auto           root = parser.parse();

    TransferRegistry reg;
    registerStandardTransfers(reg);
    return inferProgram(*root, reg);
}

}  // namespace

// A bare scalar literal binding: scalar double, unboxable, const known.
TEST(Inference, ScalarLiteral)
{
    const auto env = inferSrc("a = 3.0;");
    const auto a = env.get("a");
    EXPECT_TRUE(a.type.isUnboxableScalar());
    EXPECT_EQ(a.type.dtype, ValueType::DOUBLE);
    EXPECT_TRUE(a.constant.isKnown());
    EXPECT_DOUBLE_EQ(a.constant.value, 3.0);
}

// linspace with literal length -> double 1 x 64, inferred through the
// transfer registry on real parsed code.
TEST(Inference, LinspaceLiteral)
{
    const auto env = inferSrc("y = linspace(0, 1, 64);");
    const auto y = env.get("y");
    ASSERT_TRUE(y.type.isConcrete());
    EXPECT_EQ(y.type.dtype, ValueType::DOUBLE);
    EXPECT_EQ(y.type.shape.kind, ShapeKind::KnownDims);
    EXPECT_EQ(y.type.shape.rows, 1u);
    EXPECT_EQ(y.type.shape.cols, 64u);
}

// THE killer demo: a literal constant flows through a variable binding
// into a size constructor's shape. n=5 ; zeros(n) -> 5x5.
TEST(Inference, ConstantPropagatesIntoShape)
{
    const auto env = inferSrc("n = 5; z = zeros(n);");
    const auto z = env.get("z");
    ASSERT_TRUE(z.type.isConcrete());
    EXPECT_EQ(z.type.dtype, ValueType::DOUBLE);
    EXPECT_EQ(z.type.shape.kind, ShapeKind::KnownDims);
    EXPECT_EQ(z.type.shape.rows, 5u);
    EXPECT_EQ(z.type.shape.cols, 5u);
}

// Scalar element access: x(3) on a double array is an unboxed scalar —
// the hot x(i)-in-a-loop case.
TEST(Inference, ScalarIndexIsUnboxableScalar)
{
    const auto env = inferSrc("x = zeros(1, 10); v = x(3);");
    const auto v = env.get("v");
    ASSERT_TRUE(v.type.isConcrete());
    EXPECT_TRUE(v.type.isUnboxableScalar());
    EXPECT_EQ(v.type.dtype, ValueType::DOUBLE);
}

// A use of an unbound variable is conservatively Dynamic.
TEST(Inference, UnknownVariableIsDynamic)
{
    const auto env = inferSrc("q = nope;");
    EXPECT_TRUE(env.get("q").type.isDynamic());
}

// Control flow is not modelled precisely yet: a variable assigned inside
// an `if` is conservatively Dynamic (sound). Precise join/fixpoint is the
// next (CFG) brick.
TEST(Inference, ControlFlowIsConservativelyDynamic)
{
    const auto env = inferSrc("if true\n w = 5;\n end");
    EXPECT_TRUE(env.get("w").type.isDynamic());
}

// Binary operators are not yet registered (the elementwise transfer
// family is the next width step), so they currently infer to Dynamic.
// Pinning this documents the current boundary honestly — it should flip
// to a concrete double once `plus` is registered.
TEST(Inference, BinaryOpNotYetTypedIsDynamic)
{
    const auto env = inferSrc("a = 3; b = a + 1;");
    EXPECT_TRUE(env.get("a").type.isUnboxableScalar());  // a is precise
    EXPECT_TRUE(env.get("b").type.isDynamic());          // a+1 not yet
}
