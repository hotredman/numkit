// toolboxes/wavelet/tests/qmf_test.cpp
//
// Backfill gtest for toolboxes/wavelet/src/filter/qmf.cpp::qmf.
// Reference values captured from MATLAB R2025b probe.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class QmfTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// y(k) = (-1)^(k-1+p) * x(N-k+1).  Default p = 0.

TEST_F(QmfTest, DefaultRowVec)
{
    eval("y = qmf([1 2 3 4 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"),  5);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), -4);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"),  3);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), -2);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"),  1);
}

TEST_F(QmfTest, ParityFlipP1)
{
    eval("y = qmf([1 2 3 4 5], 1);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), -5);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"),  4);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), -3);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"),  2);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), -1);
}

TEST_F(QmfTest, ColumnPreservesShape)
{
    eval("y = qmf([1; 2; 3]);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 1)")), 3u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 2)")), 1u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"),  3);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), -2);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"),  1);
}

TEST_F(QmfTest, Scalar)
{
    eval("y = qmf(7);");
    EXPECT_DOUBLE_EQ(evalScalar("y"), 7);   // (-1)^0 * x(1) = x(1) = 7
}

TEST_F(QmfTest, Empty)
{
    eval("y = qmf([]);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 0u);
}

TEST_F(QmfTest, Negatives)
{
    eval("y = qmf([-1 -2 3 -4]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), -4);   // (-1)^0 * x(4) = -4
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), -3);   // (-1)^1 * x(3) = -3
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), -2);   // (-1)^2 * x(2) = -2
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"),  1);   // (-1)^3 * x(1) = 1
}

TEST_F(QmfTest, EvenP1FlipsSign)
{
    // qmf(qmf(x)) with p=0 then p=1 should yield -x for any vector.
    eval("x = [1 2 3 4 5 6 7]; y = qmf(qmf(x), 1);");
    // qmf(qmf(x), 0+1) ≠ -x because the second call also reverses; but
    // qmf(x, 1) == -qmf(x, 0) by definition (sign flip).
    eval("y_ref = -qmf(x);");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(qmf(x, 1) - y_ref))"), 0.0);
}

TEST_F(QmfTest, EvenLength8)
{
    // qmf(1:8) = [8 -7 6 -5 4 -3 2 -1]
    eval("y = qmf(1:8);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"),  8);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), -7);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), -5);
    EXPECT_DOUBLE_EQ(evalScalar("y(8)"), -1);
}

TEST_F(QmfTest, Length4DefaultAndP1)
{
    // qmf([1 2 3 4]) = [4 -3 2 -1]
    eval("y0 = qmf([1 2 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("y0(1)"),  4);
    EXPECT_DOUBLE_EQ(evalScalar("y0(2)"), -3);
    EXPECT_DOUBLE_EQ(evalScalar("y0(3)"),  2);
    EXPECT_DOUBLE_EQ(evalScalar("y0(4)"), -1);
    // qmf([1 2 3 4], 1) = [-4 3 -2 1]
    eval("y1 = qmf([1 2 3 4], 1);");
    EXPECT_DOUBLE_EQ(evalScalar("y1(1)"), -4);
    EXPECT_DOUBLE_EQ(evalScalar("y1(2)"),  3);
}
