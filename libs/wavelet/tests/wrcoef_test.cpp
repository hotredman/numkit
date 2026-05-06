// libs/wavelet/tests/wrcoef_test.cpp
//
// Backfill gtest for libs/wavelet/src/dwt/wrcoef.cpp::wrcoef.
// Parity verified on Haar (where numkit's wavedec matches MATLAB);
// db/sym/coif use a different boundary convention (BUGS.md #37).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WrcoefTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("v10 = [1:16];");
        engine.eval("[c, l] = wavedec(v10, 3, 'haar');");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(WrcoefTest, ApproximationLevel3)
{
    eval("a3 = wrcoef('a', c, l, 'haar', 3);");
    // Haar level-3 approximation = average of first 8 / last 8
    EXPECT_DOUBLE_EQ(evalScalar("a3(1)"), 4.5);
    EXPECT_DOUBLE_EQ(evalScalar("a3(8)"), 4.5);
    EXPECT_DOUBLE_EQ(evalScalar("a3(9)"), 12.5);
}

TEST_F(WrcoefTest, ApproximationLevel0EqualsOriginal)
{
    eval("a0 = wrcoef('a', c, l, 'haar', 0);");
    for (int k = 1; k <= 16; ++k) {
        EXPECT_NEAR(evalScalar("a0(" + std::to_string(k) + ")"),
                    static_cast<double>(k), 1e-10);
    }
}

TEST_F(WrcoefTest, IdentitySumOfBands)
{
    // a3 + d1 + d2 + d3 == original signal
    eval("recon = wrcoef('a', c, l, 'haar', 3) + wrcoef('d', c, l, 'haar', 1) + "
         "wrcoef('d', c, l, 'haar', 2) + wrcoef('d', c, l, 'haar', 3);");
    eval("a0 = wrcoef('a', c, l, 'haar', 0);");
    EXPECT_NEAR(evalScalar("max(abs(recon - a0))"), 0.0, 1e-10);
}

TEST_F(WrcoefTest, DefaultLevelEqualsMaxLevel)
{
    eval("a_def = wrcoef('a', c, l, 'haar'); a3 = wrcoef('a', c, l, 'haar', 3);");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(a_def - a3))"), 0.0);
}

TEST_F(WrcoefTest, BadTypeRejected)
{
    EXPECT_THROW(eval("wrcoef('x', c, l, 'haar', 1);"), numkit::Error);
}

TEST_F(WrcoefTest, DetailLevelZeroRejected)
{
    EXPECT_THROW(eval("wrcoef('d', c, l, 'haar', 0);"), numkit::Error);
}
