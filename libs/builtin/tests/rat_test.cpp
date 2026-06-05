// libs/builtin/tests/rat_test.cpp
// builtin/rat AND builtin/rats. Closes:
// Pre-fix:
//   - rat() returned a "p / q" string (NOT MATLAB's nested CF format)
//   - [N, D] = rat(...) threw "Undefined function or variable 'D'"
//   - rats() returned the rat string with naive left-padding instead of
//     MATLAB's centred num/denom layout
// Post-fix:
//   - regularized CF expansion using round() (signed coefficients)
//   - 1-output yields nested string `'a0 + 1/(a1 + 1/(...))'`
//   - 2-output yields numeric N, D arrays (vectorised across input)
//   - rats yields fixed-width centred 'numer/denom' field
//   - All bit-identical to MATLAB R2025b probes.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class RatTest : public ::testing::Test
{
public:
    Engine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
    std::string evalString(const std::string &c) { return eval(c).toString(); }
};

// ─── 2-output [N, D] form (the core gap) ──────────────────────────

TEST_F(RatTest, TwoOutputPi)
{
    // probe: [N, D] = rat(pi, 1e-3) → N=355, D=113.
    eval("[N, D] = rat(pi, 1e-3);");
    EXPECT_DOUBLE_EQ(evalScalar("N"), 355.0);
    EXPECT_DOUBLE_EQ(evalScalar("D"), 113.0);
}

TEST_F(RatTest, TwoOutputDefaultTol)
{
    // Default tol = 1e-6·|x| → still resolves pi to 355/113.
    eval("[N, D] = rat(pi);");
    EXPECT_DOUBLE_EQ(evalScalar("N"), 355.0);
    EXPECT_DOUBLE_EQ(evalScalar("D"), 113.0);
}

TEST_F(RatTest, TwoOutputHalf)
{
    eval("[N, D] = rat(0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("N"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("D"), 2.0);
}

TEST_F(RatTest, TwoOutputThird)
{
    eval("[N, D] = rat(1/3);");
    EXPECT_DOUBLE_EQ(evalScalar("N"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("D"), 3.0);
}

TEST_F(RatTest, TwoOutputVectorized)
{
    eval("[N, D] = rat([0.5; 1/3; pi]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(N)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(D)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("N(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("N(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("N(3)"), 355.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(3)"), 113.0);
}

TEST_F(RatTest, TwoOutputDenominatorAlwaysPositive)
{
    eval("[N, D] = rat(-0.5);");
    // N may be negative, D must be positive (MATLAB normalises).
    EXPECT_DOUBLE_EQ(evalScalar("D"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("N"), -1.0);
}

// ─── 1-output continued-fraction string form ─────────────────────────

TEST_F(RatTest, OneOutputPiCFString)
{
    // MATLAB R2025b probe: rat(pi, 1e-3) = '3 + 1/(7 + 1/(16))'
    EXPECT_EQ(evalString("rat(pi, 1e-3)"), "3 + 1/(7 + 1/(16))");
}

TEST_F(RatTest, OneOutputHalfUsesRegularizedCF)
{
    // MATLAB R2025b: rat(0.5) = '1 + 1/(-2)' (NOT '0 + 1/(2)' which
    // would be the simple-CF form). Numkit must use round(), not floor().
    EXPECT_EQ(evalString("rat(0.5)"), "1 + 1/(-2)");
}

TEST_F(RatTest, OneOutputThird)
{
    // MATLAB R2025b: rat(1/3) = '0 + 1/(3)' (round(0.333)=0 → next).
    EXPECT_EQ(evalString("rat(1/3)"), "0 + 1/(3)");
}

TEST_F(RatTest, OneOutputInfNan)
{
    EXPECT_EQ(evalString("rat(Inf)"),  "Inf");
    EXPECT_EQ(evalString("rat(-Inf)"), "-Inf");
    EXPECT_EQ(evalString("rat(NaN)"),  "NaN");
}

// ─── rats: fixed-width centred 'numer/denom' formatting ──────────────

TEST_F(RatTest, RatsDefaultLengthIs14)
{
    // MATLAB R2025b: strlength(rats(0.5)) = 14 (default len=13 internal,
    // field width = len+1 to reserve a leading sign column).
    EXPECT_DOUBLE_EQ(evalScalar("strlength(rats(0.5))"), 14.0);
    EXPECT_DOUBLE_EQ(evalScalar("strlength(rats(pi))"),  14.0);
    EXPECT_DOUBLE_EQ(evalScalar("strlength(rats(1/3))"), 14.0);
}

TEST_F(RatTest, RatsSlashAtMidColumn)
{
    // MATLAB layout: numerator right-justified in cols 1..7, slash in
    // col 8, denominator left-justified in cols 9..14.
    eval("ix = strfind(rats(pi), '/');");
    EXPECT_DOUBLE_EQ(evalScalar("ix(1)"), 8.0);
    eval("ix = strfind(rats(0.5), '/');");
    EXPECT_DOUBLE_EQ(evalScalar("ix(1)"), 8.0);
}

TEST_F(RatTest, RatsCustomLength)
{
    // rats(x, len) honours user-supplied len + 1 sign column.
    EXPECT_DOUBLE_EQ(evalScalar("strlength(rats(0.5, 5))"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("strlength(rats(0.5, 20))"), 21.0);
}

TEST_F(RatTest, RatsVectorConcatenates)
{
    // Vector input produces one row, per-element fields concatenated.
    EXPECT_DOUBLE_EQ(evalScalar("strlength(rats([0.5 1/3 pi]))"), 42.0);  // 3 × 14
}

// ─── existing-call regression: nothing else broke ───────────────────

TEST_F(RatTest, ScalarRatNonzero)
{
    // Ensure rat(2.7183) still completes and returns a non-empty CF.
    EXPECT_GT(evalScalar("strlength(rat(2.7183))"), 0.0);
    // And the 2-output reciprocal: N/D ≈ 2.7183 within tol.
    eval("[N, D] = rat(2.7183);");
    const double approx = evalScalar("N") / evalScalar("D");
    EXPECT_NEAR(approx, 2.7183, 1e-3);
}
