// libs/signal/tests/measurements_test.cpp
//
// Tests for packages A2 (dB conversions) and A3 (signal stats).
//   db / db2mag / mag2db / db2pow / pow2db
//   rms / rssq / peak2peak / peak2rms

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <cmath>
#include <gtest/gtest.h>

using namespace numkit;

class MeasurementsTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &code) { return engine.eval(code); }
    double evalScalar(const std::string &code) { return eval(code).toScalar(); }
};

// ───────────────────────────── dB conversions ──────────────────────────

TEST_F(MeasurementsTest, MagToDbScalar)
{
    EXPECT_NEAR(evalScalar("mag2db(1)"),    0.0,  1e-12);
    EXPECT_NEAR(evalScalar("mag2db(10)"),  20.0,  1e-12);
    EXPECT_NEAR(evalScalar("mag2db(100)"), 40.0,  1e-12);
}

TEST_F(MeasurementsTest, Db2MagInverseOfMag2Db)
{
    eval("d = mag2db([0.1 1 10 100]);");
    eval("m = db2mag(d);");
    for (int i = 1; i <= 4; ++i) {
        const double in = evalScalar("[0.1 1 10 100](" + std::to_string(i) + ")");
        const double rt = evalScalar("m(" + std::to_string(i) + ")");
        EXPECT_NEAR(in, rt, 1e-12);
    }
}

TEST_F(MeasurementsTest, PowToDbScalar)
{
    EXPECT_NEAR(evalScalar("pow2db(1)"),    0.0,  1e-12);
    EXPECT_NEAR(evalScalar("pow2db(10)"),  10.0,  1e-12);
    EXPECT_NEAR(evalScalar("pow2db(1000)"), 30.0, 1e-12);
}

TEST_F(MeasurementsTest, Db2PowInverseOfPow2Db)
{
    eval("p = pow2db([0.5 1 100]); q = db2pow(p);");
    EXPECT_NEAR(evalScalar("q(1)"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("q(2)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("q(3)"), 100.0, 1e-9);
}

TEST_F(MeasurementsTest, DbDefaultIsVoltage)
{
    EXPECT_NEAR(evalScalar("db(10)"),         20.0,  1e-12);
    EXPECT_NEAR(evalScalar("db(10,'voltage')"), 20.0, 1e-12);
}

TEST_F(MeasurementsTest, DbPowerMode)
{
    EXPECT_NEAR(evalScalar("db(100,'power')"), 20.0, 1e-12);
}

TEST_F(MeasurementsTest, DbOnComplexUsesMagnitude)
{
    EXPECT_NEAR(evalScalar("db(complex(3,4))"), 20.0 * std::log10(5.0), 1e-12);
}

// ───────────────────────────── Signal stats ────────────────────────────

TEST_F(MeasurementsTest, RmsConstantVector)
{
    EXPECT_NEAR(evalScalar("rms([2 2 2 2])"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("rms([-3 3 -3 3])"), 3.0, 1e-12);
}

TEST_F(MeasurementsTest, RmsSineWaveApproxOneOverSqrt2)
{
    eval("t = (0:1023)/1024; x = sin(2*pi*5*t);");
    EXPECT_NEAR(evalScalar("rms(x)"), 1.0 / std::sqrt(2.0), 1e-3);
}

TEST_F(MeasurementsTest, RssqMatchesSqrtSumSquares)
{
    EXPECT_NEAR(evalScalar("rssq([3 4])"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("rssq([1 2 2])"), 3.0, 1e-12);
}

TEST_F(MeasurementsTest, Peak2PeakBasic)
{
    EXPECT_NEAR(evalScalar("peak2peak([-1 0 3])"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("peak2peak([5 5 5])"), 0.0, 1e-12);
}

TEST_F(MeasurementsTest, Peak2PeakNanPropagates)
{
    EXPECT_TRUE(std::isnan(evalScalar("peak2peak([1 NaN 2])")));
}

TEST_F(MeasurementsTest, Peak2RmsConstant)
{
    // For a constant ±A vector, peak / rms = 1.
    EXPECT_NEAR(evalScalar("peak2rms([2 2 2])"), 1.0, 1e-12);
}

TEST_F(MeasurementsTest, Peak2RmsSineApproxSqrt2)
{
    eval("t = (0:1023)/1024; x = sin(2*pi*5*t);");
    EXPECT_NEAR(evalScalar("peak2rms(x)"), std::sqrt(2.0), 1e-2);
}

// ── dim argument (matrix → reduce along chosen axis) ────────────────────

TEST_F(MeasurementsTest, RmsAlongRows)
{
    // 2×3 matrix; rms along dim=1 (down columns) gives 1×3 row.
    eval("M = [1 2 3; 1 2 3];");
    eval("r = rms(M, 1);");
    EXPECT_NEAR(evalScalar("r(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("r(2)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("r(3)"), 3.0, 1e-12);
}

TEST_F(MeasurementsTest, RmsAlongCols)
{
    eval("M = [3 4; 0 0];");
    eval("r = rms(M, 2);");                  // result 2×1
    EXPECT_NEAR(evalScalar("r(1)"), std::sqrt((9.0 + 16.0) / 2.0), 1e-12);
    EXPECT_NEAR(evalScalar("r(2)"), 0.0, 1e-12);
}
