// libs/builtin/tests/numbertheory_test.cpp
//
// primes / isprime / factor.

#include "dual_engine_fixture.hpp"

#include <cmath>

using namespace m_test;

class NumberTheoryTest : public DualEngineTest
{};

// ── primes ──────────────────────────────────────────────────────

TEST_P(NumberTheoryTest, PrimesUpTo20)
{
    eval("p = primes(20);");
    auto *p = getVarPtr("p");
    EXPECT_EQ(rows(*p), 1u);
    EXPECT_EQ(cols(*p), 8u);
    const double expected[] = {2, 3, 5, 7, 11, 13, 17, 19};
    for (size_t i = 0; i < 8; ++i)
        EXPECT_DOUBLE_EQ(p->doubleData()[i], expected[i]);
}

TEST_P(NumberTheoryTest, PrimesUpTo2IsJustTwo)
{
    eval("p = primes(2);");
    auto *p = getVarPtr("p");
    EXPECT_EQ(p->numel(), 1u);
    EXPECT_DOUBLE_EQ(p->doubleData()[0], 2.0);
}

TEST_P(NumberTheoryTest, PrimesNLessThanTwoIsEmpty)
{
    eval("p = primes(1);");
    auto *p = getVarPtr("p");
    EXPECT_EQ(p->numel(), 0u);
    EXPECT_EQ(rows(*p), 1u);
    EXPECT_EQ(cols(*p), 0u);
}

TEST_P(NumberTheoryTest, PrimesNegativeIsEmpty)
{
    eval("p = primes(-5);");
    auto *p = getVarPtr("p");
    EXPECT_EQ(p->numel(), 0u);
}

TEST_P(NumberTheoryTest, PrimesUpTo100Count)
{
    // Pi(100) = 25.
    eval("p = primes(100);");
    auto *p = getVarPtr("p");
    EXPECT_EQ(p->numel(), 25u);
    EXPECT_DOUBLE_EQ(p->doubleData()[0],  2.0);
    EXPECT_DOUBLE_EQ(p->doubleData()[24], 97.0);
}

// ── isprime ─────────────────────────────────────────────────────

TEST_P(NumberTheoryTest, IsprimeBasic)
{
    eval("v = isprime([1 2 3 4 5 6 7 8 9 10]);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->type(), MType::LOGICAL);
    EXPECT_EQ(v->numel(), 10u);
    EXPECT_EQ(v->logicalData()[0], 0);  // 1
    EXPECT_NE(v->logicalData()[1], 0);  // 2
    EXPECT_NE(v->logicalData()[2], 0);  // 3
    EXPECT_EQ(v->logicalData()[3], 0);  // 4
    EXPECT_NE(v->logicalData()[4], 0);  // 5
    EXPECT_EQ(v->logicalData()[5], 0);  // 6
    EXPECT_NE(v->logicalData()[6], 0);  // 7
    EXPECT_EQ(v->logicalData()[7], 0);  // 8
    EXPECT_EQ(v->logicalData()[8], 0);  // 9
    EXPECT_EQ(v->logicalData()[9], 0);  // 10
}

TEST_P(NumberTheoryTest, IsprimeMatrixPreservesShape)
{
    eval("M = [2 3 4; 5 6 7];"
         "v = isprime(M);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 3u);
}

TEST_P(NumberTheoryTest, IsprimeNegativeIsFalse)
{
    eval("v = isprime([-3 -2 -1 0]);");
    auto *v = getVarPtr("v");
    for (size_t i = 0; i < 4; ++i)
        EXPECT_EQ(v->logicalData()[i], 0);
}

TEST_P(NumberTheoryTest, IsprimeNonIntegerIsFalse)
{
    eval("v = isprime([2.5, 3.0, 7.5, NaN, Inf]);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->logicalData()[0], 0);  // 2.5
    EXPECT_NE(v->logicalData()[1], 0);  // 3.0 (exact integer)
    EXPECT_EQ(v->logicalData()[2], 0);  // 7.5
    EXPECT_EQ(v->logicalData()[3], 0);  // NaN
    EXPECT_EQ(v->logicalData()[4], 0);  // Inf
}

TEST_P(NumberTheoryTest, IsprimeLargePrime)
{
    // 1000003 is prime.
    EXPECT_DOUBLE_EQ(evalScalar("double(isprime(1000003));"), 1.0);
    // 1000004 is even → not prime.
    EXPECT_DOUBLE_EQ(evalScalar("double(isprime(1000004));"), 0.0);
}

TEST_P(NumberTheoryTest, IsprimeComplexThrows)
{
    EXPECT_THROW(eval("v = isprime([1+2i, 3]);"), std::exception);
}

// ── factor ─────────────────────────────────────────────────────

TEST_P(NumberTheoryTest, FactorOf60)
{
    // 60 = 2*2*3*5
    eval("f = factor(60);");
    auto *f = getVarPtr("f");
    EXPECT_EQ(rows(*f), 1u);
    EXPECT_EQ(cols(*f), 4u);
    EXPECT_DOUBLE_EQ(f->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(f->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(f->doubleData()[2], 3.0);
    EXPECT_DOUBLE_EQ(f->doubleData()[3], 5.0);
}

TEST_P(NumberTheoryTest, FactorOfPrime)
{
    // 17 is prime → returns [17].
    eval("f = factor(17);");
    auto *f = getVarPtr("f");
    EXPECT_EQ(f->numel(), 1u);
    EXPECT_DOUBLE_EQ(f->doubleData()[0], 17.0);
}

TEST_P(NumberTheoryTest, FactorOfPowerOfTwo)
{
    eval("f = factor(64);");
    auto *f = getVarPtr("f");
    EXPECT_EQ(f->numel(), 6u);
    for (size_t i = 0; i < 6; ++i)
        EXPECT_DOUBLE_EQ(f->doubleData()[i], 2.0);
}

TEST_P(NumberTheoryTest, FactorZero)
{
    // MATLAB: factor(0) = 0.
    eval("f = factor(0);");
    auto *f = getVarPtr("f");
    EXPECT_EQ(f->numel(), 1u);
    EXPECT_DOUBLE_EQ(f->doubleData()[0], 0.0);
}

TEST_P(NumberTheoryTest, FactorOne)
{
    // MATLAB: factor(1) = 1.
    eval("f = factor(1);");
    auto *f = getVarPtr("f");
    EXPECT_EQ(f->numel(), 1u);
    EXPECT_DOUBLE_EQ(f->doubleData()[0], 1.0);
}

TEST_P(NumberTheoryTest, FactorNegativeThrows)
{
    EXPECT_THROW(eval("f = factor(-5);"), std::exception);
}

TEST_P(NumberTheoryTest, FactorNonIntegerThrows)
{
    EXPECT_THROW(eval("f = factor(5.5);"), std::exception);
}

TEST_P(NumberTheoryTest, FactorVectorInputThrows)
{
    EXPECT_THROW(eval("f = factor([6 12]);"), std::exception);
}

TEST_P(NumberTheoryTest, FactorProductMatchesInput)
{
    // Sanity: prod(factor(n)) == n for several composites.
    for (int n : {12, 30, 100, 360, 999}) {
        const std::string code = "y = prod(factor(" + std::to_string(n) + "));";
        eval(code);
        EXPECT_DOUBLE_EQ(evalScalar("y;"), static_cast<double>(n))
            << "n=" << n;
    }
}

INSTANTIATE_DUAL(NumberTheoryTest);
