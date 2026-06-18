// codegen/tests/transfer_diff_test.cpp
//
// Differential validator for the transfer functions: for each case, run
// the REAL builtin through the engine and check that the transfer
// function's prediction matches the actual result's type and shape.
//
// This is the mechanism described in DESIGN.md §4 — transfer rules are
// pinned to ground truth (the real implementation), not hand-asserted.
// It lives in tests (not the codegen lib) because it must EXECUTE
// builtins, which needs the engine; the codegen lib itself stays
// engine-free.

#include <numkit/codegen/transfer.hpp>

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>

#include <gtest/gtest.h>

#include <vector>

using numkit::ValueType;
using namespace numkit::codegen;

class TransferDiff : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    TransferRegistry       reg;

    void SetUp() override
    {
        engine.eval("import compat.*;");
        registerStandardTransfers(reg);
    }

    numkit::Value eval(const std::string &c) { return engine.eval(c); }

    // Assert the registered transfer for `name` predicts exactly the
    // type/shape the engine produces for `expr`.
    void expectMatch(const std::string &name,
                     const std::vector<ArgInfo> &args,
                     const std::string &expr)
    {
        const numkit::Value actual = eval(expr);
        const InferredType  pred   = reg.apply(name, args);

        ASSERT_TRUE(pred.isConcrete()) << name << ": predicted non-concrete";
        EXPECT_EQ(pred.dtype, actual.type()) << name << ": dtype mismatch";

        // Shape: a concrete prediction must match the actual dims. (An
        // Unknown-shape prediction is a sound under-approximation and is
        // not checked here — those cases are covered by the unit test.)
        if (pred.shape.kind == ShapeKind::Scalar) {
            EXPECT_EQ(actual.dims().rows(), 1u) << name;
            EXPECT_EQ(actual.dims().cols(), 1u) << name;
        } else if (pred.shape.kind == ShapeKind::KnownDims) {
            EXPECT_EQ(pred.shape.rows, actual.dims().rows()) << name << ": rows";
            EXPECT_EQ(pred.shape.cols, actual.dims().cols()) << name << ": cols";
        }
    }
};

TEST_F(TransferDiff, Linspace3Arg)
{
    expectMatch("linspace",
                {ArgInfo::scalarConst(ValueType::DOUBLE, 0.0),
                 ArgInfo::scalarConst(ValueType::DOUBLE, 1.0),
                 ArgInfo::scalarConst(ValueType::DOUBLE, 64.0)},
                "linspace(0, 1, 64)");
}

TEST_F(TransferDiff, Linspace2ArgDefault)
{
    expectMatch("linspace",
                {ArgInfo::scalarConst(ValueType::DOUBLE, 0.0),
                 ArgInfo::scalarConst(ValueType::DOUBLE, 1.0)},
                "linspace(0, 1)");
}

TEST_F(TransferDiff, ZerosSquare)
{
    expectMatch("zeros",
                {ArgInfo::scalarConst(ValueType::DOUBLE, 5.0)},
                "zeros(5)");
}

TEST_F(TransferDiff, ZerosRectangular)
{
    expectMatch("zeros",
                {ArgInfo::scalarConst(ValueType::DOUBLE, 3.0),
                 ArgInfo::scalarConst(ValueType::DOUBLE, 4.0)},
                "zeros(3, 4)");
}

TEST_F(TransferDiff, OnesRectangular)
{
    expectMatch("ones",
                {ArgInfo::scalarConst(ValueType::DOUBLE, 2.0),
                 ArgInfo::scalarConst(ValueType::DOUBLE, 7.0)},
                "ones(2, 7)");
}

// ── elementwise family vs the runtime ────────────────────────────────

TEST_F(TransferDiff, ScalarPlus)
{
    expectMatch("plus",
                {ArgInfo::scalarConst(ValueType::DOUBLE, 3.0),
                 ArgInfo::scalarConst(ValueType::DOUBLE, 4.0)},
                "3 + 4");
}

TEST_F(TransferDiff, ArrayPlusScalarBroadcast)
{
    expectMatch("plus",
                {ArgInfo::of(InferredType::concrete(ValueType::DOUBLE, Shape::dims(1, 3))),
                 ArgInfo::scalarConst(ValueType::DOUBLE, 1.0)},
                "[1 2 3] + 1");
}

TEST_F(TransferDiff, ComparisonIsLogical)
{
    expectMatch("lt",
                {ArgInfo::scalarConst(ValueType::DOUBLE, 2.0),
                 ArgInfo::scalarConst(ValueType::DOUBLE, 3.0)},
                "2 < 3");
}

TEST_F(TransferDiff, SinScalar)
{
    expectMatch("sin",
                {ArgInfo::scalarConst(ValueType::DOUBLE, 0.5)},
                "sin(0.5)");
}

TEST_F(TransferDiff, AbsScalar)
{
    expectMatch("abs",
                {ArgInfo::scalarConst(ValueType::DOUBLE, -5.0)},
                "abs(-5)");
}

// |complex| is real double — the abs dtype-narrowing rule.
TEST_F(TransferDiff, AbsComplexIsReal)
{
    expectMatch("abs",
                {ArgInfo::of(InferredType::scalar(ValueType::COMPLEX))},
                "abs(3 + 4i)");
}

// single + single stays single (promotion).
TEST_F(TransferDiff, SinglePlusSingle)
{
    expectMatch("plus",
                {ArgInfo::of(InferredType::scalar(ValueType::SINGLE)),
                 ArgInfo::of(InferredType::scalar(ValueType::SINGLE))},
                "single(1) + single(2)");
}
