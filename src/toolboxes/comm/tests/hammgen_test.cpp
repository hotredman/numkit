// toolboxes/comm/tests/hammgen_test.cpp
//
// Regression guard for hammgen (Error Correction Codes). Reference H/G
// matrices and code dimensions from the MATLAB R2025b probe:
//   hammgen(3) -> (7,4) Hamming code; hammgen(4) -> (15,11).

#include <numkit/comm/coding/blockcoding.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class HammgenTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// hammgen(3): (7,4) Hamming code, full H and G matrices.
TEST_F(HammgenTest, M3FullMatrices)
{
    eval("[h, g, n, k] = hammgen(3);");
    EXPECT_DOUBLE_EQ(evalScalar("n"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("k"), 4.0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(h,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(h,2)")), 7);
    EXPECT_EQ(static_cast<int>(evalScalar("size(g,1)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(g,2)")), 7);
    EXPECT_DOUBLE_EQ(evalScalar(
        "isequal(h, [1 0 0 1 0 1 1; 0 1 0 1 1 1 0; 0 0 1 0 1 1 1])"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar(
        "isequal(g, [1 1 0 1 0 0 0; 0 1 1 0 1 0 0; "
        "1 1 1 0 0 1 0; 1 0 1 0 0 0 1])"), 1.0);
    // First m columns of H form I_m (systematic).
    EXPECT_DOUBLE_EQ(evalScalar("isequal(h(:,1:3), eye(3))"), 1.0);
}

// hammgen(4): (15,11) Hamming code.
TEST_F(HammgenTest, M4Dimensions)
{
    eval("[h4, g4, n4, k4] = hammgen(4);");
    EXPECT_DOUBLE_EQ(evalScalar("n4"), 15.0);
    EXPECT_DOUBLE_EQ(evalScalar("k4"), 11.0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(h4,1)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(h4,2)")), 15);
    EXPECT_DOUBLE_EQ(evalScalar(
        "isequal(h4, [1 0 0 0 1 0 0 1 1 0 1 0 1 1 1; "
        "0 1 0 0 1 1 0 1 0 1 1 1 1 0 0; "
        "0 0 1 0 0 1 1 0 1 0 1 1 1 1 0; "
        "0 0 0 1 0 0 1 1 0 1 0 1 1 1 1])"), 1.0);
}

// G * H' == 0 (mod 2): the generator and parity matrices are orthogonal.
TEST_F(HammgenTest, OrthogonalCheck)
{
    eval("[h, g] = hammgen(3); P = mod(g * h', 2);");
    EXPECT_DOUBLE_EQ(evalScalar("all(P(:) == 0)"), 1.0);
}

// Single-output form returns just the parity-check matrix.
TEST_F(HammgenTest, SingleOutput)
{
    eval("h = hammgen(3);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(h,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(h,2)")), 7);
}

TEST_F(HammgenTest, Errors)
{
    EXPECT_ANY_THROW(eval("hammgen(1);"));   // M < 2
}

// Direct C++ API — inspect the HammgenResult struct.
TEST_F(HammgenTest, PublicApi)
{
    comm::HammgenResult r = comm::hammgen(3, Value::Empty, engine.resource());
    EXPECT_EQ(r.n, 7);
    EXPECT_EQ(r.k, 4);
    ASSERT_EQ(r.h.dims().rows(), 3u);
    ASSERT_EQ(r.h.dims().cols(), 7u);
    ASSERT_EQ(r.g.dims().rows(), 4u);
    // H(1,4) 1-based = col-major index 3*3 + 0 = 9 -> 1.0
    EXPECT_DOUBLE_EQ(r.h.doubleData()[3 * 3 + 0], 1.0);
}
