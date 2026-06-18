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
    // type/shape the engine produces for `expr` (precision check).
    void expectMatch(const std::string &name,
                     const std::vector<ArgInfo> &args,
                     const std::string &expr)
    {
        const numkit::Value actual = eval(expr);
        const InferredType  pred   = reg.apply(name, args);

        ASSERT_TRUE(pred.isConcrete()) << name << ": predicted non-concrete";
        EXPECT_EQ(pred.dtype, actual.type()) << name << ": dtype mismatch";

        if (pred.shape.kind == ShapeKind::Scalar) {
            EXPECT_EQ(actual.dims().rows(), 1u) << name;
            EXPECT_EQ(actual.dims().cols(), 1u) << name;
        } else if (pred.shape.kind == ShapeKind::KnownDims) {
            EXPECT_EQ(pred.shape.rows, actual.dims().rows()) << name << ": rows";
            EXPECT_EQ(pred.shape.cols, actual.dims().cols()) << name << ": cols";
        }
    }

    // SOUNDNESS check (Contract 1): the runtime type must be ⊑ the
    // prediction (over-approximation). Dynamic over-approximates anything;
    // a concrete prediction must have the actual dtype and either an
    // Unknown shape or the exact dims. A concrete prediction with the
    // WRONG dtype is unsound (the failure mode `power` had).
    void expectSound(const std::string &name,
                     const std::vector<ArgInfo> &args,
                     const std::string &expr)
    {
        const numkit::Value actual = eval(expr);
        const InferredType  pred   = reg.apply(name, args);

        if (pred.isDynamic()) return;  // top — over-approximates everything
        ASSERT_TRUE(pred.isConcrete()) << name << ": Bottom is never sound here";
        EXPECT_EQ(pred.dtype, actual.type())
            << name << " UNSOUND: predicted " << pred.str()
            << " but runtime produced " << numkit::mtypeName(actual.type());
        if (pred.shape.kind == ShapeKind::KnownDims) {
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

// ── soundness (Contract 1): runtime type must be ⊑ prediction ─────────
// These hit the adversarial corner that pure precision tests miss.

// power is NOT closed over reals: (-2)^0.5 is complex. With a non-integer
// exponent the transfer must NOT claim double — it returns Dynamic, which
// soundly over-approximates the complex result. (Before the fix it
// claimed double here -> unsound -> this test would fail.)
TEST_F(TransferDiff, PowerNegativeBaseFractionalExponentIsSound)
{
    expectSound("mpower",
                {ArgInfo::scalarConst(ValueType::DOUBLE, -2.0),
                 ArgInfo::scalarConst(ValueType::DOUBLE, 0.5)},
                "(-2)^0.5");          // runtime: complex
    expectSound("mpower",
                {ArgInfo::scalarConst(ValueType::DOUBLE, -8.0),
                 ArgInfo::scalarConst(ValueType::DOUBLE, 1.0 / 3.0)},
                "(-8)^(1/3)");        // runtime: complex
}

// With an integer exponent power stays real — precise (and sound).
TEST_F(TransferDiff, PowerIntegerExponentIsReal)
{
    expectMatch("mpower",
                {ArgInfo::scalarConst(ValueType::DOUBLE, -8.0),
                 ArgInfo::scalarConst(ValueType::DOUBLE, 2.0)},
                "(-8)^2");            // runtime: double (64)
    expectMatch("mpower",
                {ArgInfo::scalarConst(ValueType::DOUBLE, 2.0),
                 ArgInfo::scalarConst(ValueType::DOUBLE, 3.0)},
                "2^3");               // runtime: double (8)
}

// Positive base, fractional exponent: runtime is double; transfer returns
// Dynamic (it can't prove the base is non-negative) — sound, just
// imprecise. Asserting soundness (not precision) is the honest contract.
TEST_F(TransferDiff, PowerPositiveBaseFractionalIsSound)
{
    expectSound("mpower",
                {ArgInfo::scalarConst(ValueType::DOUBLE, 2.0),
                 ArgInfo::scalarConst(ValueType::DOUBLE, 0.5)},
                "2^0.5");             // runtime: double; pred: Dynamic (sound)
}
