// toolboxes/wavelet/tests/wmaxlev_test.cpp
//
// Backfill gtest for toolboxes/wavelet/src/dwt/dyad.cpp::wmaxlev.
// Reference values from MATLAB R2025b probe.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WmaxlevTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// L = floor(log2(N / (Lf - 1)))

TEST_F(WmaxlevTest, Db2On64)
{
    EXPECT_DOUBLE_EQ(evalScalar("wmaxlev(64, 'db2')"), 4);
}

TEST_F(WmaxlevTest, HaarOn64)
{
    EXPECT_DOUBLE_EQ(evalScalar("wmaxlev(64, 'db1')"), 6);
}

TEST_F(WmaxlevTest, Db4On1024)
{
    EXPECT_DOUBLE_EQ(evalScalar("wmaxlev(1024, 'db4')"), 7);
}

TEST_F(WmaxlevTest, VectorNUsesMin)
{
    EXPECT_DOUBLE_EQ(evalScalar("wmaxlev([8 8], 'db1')"), 3);
}

TEST_F(WmaxlevTest, MinimalSignal)
{
    EXPECT_DOUBLE_EQ(evalScalar("wmaxlev(2, 'db1')"), 1);
}

TEST_F(WmaxlevTest, ScalarSignal)
{
    // N = 1, Lf = 2 → N / (Lf-1) = 1; floor(log2(1)) = 0
    EXPECT_DOUBLE_EQ(evalScalar("wmaxlev(1, 'db1')"), 0);
}

// Coverage extension 2026-05-08: cover all the documented wavelet
// families (haar, dbN, symN, coifN) × small/medium/large N.

TEST_F(WmaxlevTest, AllFamiliesAtN100)
{
    EXPECT_DOUBLE_EQ(evalScalar("wmaxlev(100, 'db4')"),  3);
    EXPECT_DOUBLE_EQ(evalScalar("wmaxlev(100, 'sym4')"), 3);
    EXPECT_DOUBLE_EQ(evalScalar("wmaxlev(100, 'coif2')"), 3);
}

TEST_F(WmaxlevTest, LargerWavelet)
{
    // db10 has Lf=20, so N/(Lf-1) = N/19. wmaxlev(2048, db10) =
    // floor(log2(2048/19)) = floor(log2(107.79)) = 6.
    EXPECT_DOUBLE_EQ(evalScalar("wmaxlev(2048, 'db10')"), 6);
}

TEST_F(WmaxlevTest, ImageDimsTakeMin)
{
    // 2-vector N: MATLAB uses min(N) for 2-D images.
    EXPECT_DOUBLE_EQ(evalScalar("wmaxlev([50 100], 'db4')"), 2);
}

TEST_F(WmaxlevTest, BoundaryShortSignal)
{
    // N=8 with db4 (Lf=8): N/(Lf-1) = 8/7 ≈ 1.14, floor(log2) = 0.
    EXPECT_DOUBLE_EQ(evalScalar("wmaxlev(8, 'db4')"), 0);
}

TEST_F(WmaxlevTest, LargeHaarPowerOfTwo)
{
    // haar Lf=2, so log2(N/1) = log2(N). N=1024 → L=10.
    EXPECT_DOUBLE_EQ(evalScalar("wmaxlev(1024, 'haar')"), 10);
}
