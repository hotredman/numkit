// libs/comm/tests/cyclgen_test.cpp
//
// Regression guard for cyclgen (Error Correction Codes). Reference
// matrices from the MATLAB R2025b probe (cyclgen(7,[1 0 1 1])).

#include <numkit/comm/coding/blockcoding.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CyclgenTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Systematic (7,4) cyclic code from generator polynomial 1+x^2+x^3.
TEST_F(CyclgenTest, System74)
{
    eval("[h, g, k] = cyclgen(7, [1 0 1 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("k"), 4.0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(h,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(g,1)")), 4);
    EXPECT_DOUBLE_EQ(evalScalar(
        "isequal(h, [1 0 0 1 1 1 0; 0 1 0 0 1 1 1; 0 0 1 1 1 0 1])"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar(
        "isequal(g, [1 0 1 1 0 0 0; 1 1 1 0 1 0 0; "
        "1 1 0 0 0 1 0; 0 1 1 0 0 0 1])"), 1.0);
    // Systematic: last k columns of g form I_k.
    EXPECT_DOUBLE_EQ(evalScalar("isequal(g(:,4:7), eye(4))"), 1.0);
    // Orthogonality.
    EXPECT_DOUBLE_EQ(evalScalar("all(all(mod(g*h',2) == 0))"), 1.0);
}

// Non-systematic form: cyclic shifts of the (reversed) parity / generator
// polynomials.
TEST_F(CyclgenTest, NonSystem74)
{
    eval("[h, g, k] = cyclgen(7, [1 0 1 1], 'nonsystem');");
    EXPECT_DOUBLE_EQ(evalScalar(
        "isequal(h, [1 1 1 0 1 0 0; 0 1 1 1 0 1 0; 0 0 1 1 1 0 1])"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar(
        "isequal(g, [1 0 1 1 0 0 0; 0 1 0 1 1 0 0; "
        "0 0 1 0 1 1 0; 0 0 0 1 0 1 1])"), 1.0);
    // Each generator row is the previous one cyclically shifted right.
    EXPECT_DOUBLE_EQ(evalScalar("all(all(mod(g*h',2) == 0))"), 1.0);
}

TEST_F(CyclgenTest, Errors)
{
    // generator polynomial that does not divide x^7-1
    EXPECT_ANY_THROW(eval("cyclgen(7, [1 1 1]);"));
}

// Direct C++ API.
TEST_F(CyclgenTest, PublicApi)
{
    eval("p = [1 0 1 1];");
    comm::CyclgenResult r = comm::cyclgen(7, *engine.getVariable("p"),
                                          "system", engine.resource());
    EXPECT_EQ(r.k, 4);
    ASSERT_EQ(r.h.dims().rows(), 3u);
    ASSERT_EQ(r.h.dims().cols(), 7u);
    ASSERT_EQ(r.g.dims().rows(), 4u);
    // h(1,1) = 1 (identity block start)
    EXPECT_DOUBLE_EQ(r.h.doubleData()[0], 1.0);
}
