// libs/wavelet/tests/ihaart_test.cpp
//
// Backfill gtest for libs/wavelet/src/dwt/ihaart.cpp::ihaart.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class IhaartTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(IhaartTest, FullReconstructionLevel1)
{
    eval("[a, d] = haart([1 2 3 4 5 6 7 8], 1); xr = ihaart(a, d);");
    for (int k = 1; k <= 8; ++k) {
        EXPECT_NEAR(evalScalar("xr(" + std::to_string(k) + ")"),
                    static_cast<double>(k), 1e-12);
    }
}

TEST_F(IhaartTest, MultiLevelFull)
{
    eval("[a, d] = haart([1 2 3 4 5 6 7 8]); xr = ihaart(a, d);");
    for (int k = 1; k <= 8; ++k) {
        EXPECT_NEAR(evalScalar("xr(" + std::to_string(k) + ")"),
                    static_cast<double>(k), 1e-12);
    }
}

TEST_F(IhaartTest, PartialZeroOut1)
{
    // level=1 zeroes the finest (d{1}); xrec stays full-length but smoother
    eval("[a, d] = haart([1 2 3 4 5 6 7 8]); xr = ihaart(a, d, 1);");
    EXPECT_NEAR(evalScalar("xr(1)"), 1.5, 1e-12);
    EXPECT_NEAR(evalScalar("xr(2)"), 1.5, 1e-12);
    EXPECT_NEAR(evalScalar("xr(8)"), 7.5, 1e-12);
}

TEST_F(IhaartTest, IntegerRoundtrip)
{
    eval("[a, d] = haart([10 20 30 40 50 60 70 80], 3, 'integer');");
    eval("xr = ihaart(a, d, 'integer');");
    for (int k = 1; k <= 8; ++k) {
        EXPECT_DOUBLE_EQ(evalScalar("xr(" + std::to_string(k) + ")"),
                         static_cast<double>(k * 10));
    }
}

TEST_F(IhaartTest, MatrixRoundtrip)
{
    eval("M = [1 2 3 4; 5 6 7 8; 9 10 11 12; 13 14 15 16];");
    eval("[a, d] = haart(M, 1); xr = ihaart(a, d);");
    EXPECT_DOUBLE_EQ(evalScalar("xr(1, 1)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("xr(4, 4)"), 16);
}

TEST_F(IhaartTest, ComplexDRejected)
{
    EXPECT_THROW(eval("ihaart([1 2], [1+1i 1+1i]);"), numkit::Error);
}
