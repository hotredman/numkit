// toolboxes/comm/tests/biterr_symerr_test.cpp
//
// Regression guard for biterr / symerr.

#include <numkit/comm/eq/errors.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BitErrTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(BitErrTest, BiterrIdentical)
{
    eval("[n, r] = biterr([1 2 3], [1 2 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("n"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("r"), 0.0);
}

TEST_F(BitErrTest, BiterrSingleDiff)
{
    // 7 = 111, 5 = 101 -> 1 bit difference. k=3 by default (max=7).
    eval("[n, r] = biterr(7, 5);");
    EXPECT_DOUBLE_EQ(evalScalar("n"), 1.0);
    EXPECT_NEAR(evalScalar("r"), 1.0/3.0, 1e-12);
}

TEST_F(BitErrTest, BiterrBinaryArrays)
{
    eval("[n, r] = biterr([0 1 0 1 1 0 1], [0 0 0 1 1 1 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("n"), 2.0);
    EXPECT_NEAR(evalScalar("r"), 2.0/7.0, 1e-12);
}

TEST_F(BitErrTest, BiterrAllDifferent)
{
    // [15, 7, 3] vs [0, 0, 0]: 4+3+2 = 9 bits, k=4 (since max=15).
    eval("[n, r] = biterr([15 7 3], [0 0 0]);");
    EXPECT_DOUBLE_EQ(evalScalar("n"), 9.0);
    EXPECT_NEAR(evalScalar("r"), 9.0/12.0, 1e-12);
}

TEST_F(BitErrTest, BiterrSizeMismatchThrows)
{
    EXPECT_THROW(eval("biterr([1 2 3], [1 2]);"), std::exception);
}

TEST_F(BitErrTest, SymerrIdentical)
{
    eval("[n, r] = symerr([1 2 3 4], [1 2 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("n"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("r"), 0.0);
}

TEST_F(BitErrTest, SymerrCount)
{
    eval("[n, r] = symerr([0 1 2 3 4 5 6 7], [0 1 2 3 4 5 6 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("n"), 1.0);
    EXPECT_NEAR(evalScalar("r"), 1.0/8.0, 1e-12);
}

TEST_F(BitErrTest, SymerrAllDifferent)
{
    eval("[n, r] = symerr([1 2 3], [4 5 6]);");
    EXPECT_DOUBLE_EQ(evalScalar("n"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("r"), 1.0);
}

TEST_F(BitErrTest, SymerrSizeMismatchThrows)
{
    EXPECT_THROW(eval("symerr([1 2 3], [1 2]);"), std::exception);
}

// ── Public C++ API (numkit::comm::biterr / symerr) ────────────────────
namespace {
Value bv(Engine &e, const char *expr, const char *name)
{
    e.eval(std::string(name) + " = " + expr + ";");
    return *e.getVariable(name);
}
} // namespace

TEST_F(BitErrTest, PublicApiBiterr)
{
    // auto k (k=3 for max 7), mr defaulted: 7=111 vs 5=101 -> 1 bit / 3
    auto [n, r] = comm::biterr(bv(engine, "7", "a"), bv(engine, "5", "b"));
    EXPECT_DOUBLE_EQ(n.toScalar(), 1.0);
    EXPECT_NEAR(r.toScalar(), 1.0 / 3.0, 1e-12);
    // array, auto k=4 (max 15): 9 bits / 12
    auto [n2, r2] = comm::biterr(bv(engine, "[15 7 3]", "x"),
                                 bv(engine, "[0 0 0]", "y"), 0, engine.resource());
    EXPECT_DOUBLE_EQ(n2.toScalar(), 9.0);
    EXPECT_NEAR(r2.toScalar(), 9.0 / 12.0, 1e-12);
    // explicit k=4: 1 bit / 4 = 0.25
    auto [n3, r3] = comm::biterr(bv(engine, "7", "a3"), bv(engine, "5", "b3"), 4,
                                 engine.resource());
    EXPECT_DOUBLE_EQ(n3.toScalar(), 1.0);
    EXPECT_NEAR(r3.toScalar(), 0.25, 1e-12);
    // numel mismatch throws
    EXPECT_ANY_THROW(
        comm::biterr(bv(engine, "[1 2 3]", "m1"), bv(engine, "[1 2]", "m2")));
}

TEST_F(BitErrTest, PublicApiSymerr)
{
    auto [n, r] = comm::symerr(bv(engine, "[1 2 3]", "x"), bv(engine, "[4 5 6]", "y"));
    EXPECT_DOUBLE_EQ(n.toScalar(), 3.0);
    EXPECT_DOUBLE_EQ(r.toScalar(), 1.0);
    auto [n2, r2] = comm::symerr(bv(engine, "[0 1 2]", "a"),
                                 bv(engine, "[0 1 5]", "b"), engine.resource());
    EXPECT_DOUBLE_EQ(n2.toScalar(), 1.0);
    EXPECT_NEAR(r2.toScalar(), 1.0 / 3.0, 1e-12);
    EXPECT_ANY_THROW(
        comm::symerr(bv(engine, "[1 2 3]", "m1"), bv(engine, "[1 2]", "m2")));
}
