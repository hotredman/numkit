// libs/builtin/tests/rng_test.cpp
//
// Phase 4: rng() seed control + randi + randperm. The shared engine
// is process-wide, so reproducibility tests must seed before each
// generating call. The dual-engine fixture's two backends (TW + VM)
// share the RNG too — that's by design (the underlying mt19937 is
// global), so a TW test that calls rng(seed) followed by rand() and
// a VM test doing the same should agree on the produced values.

#include "dual_engine_fixture.hpp"

using namespace m_test;

class RngTest : public DualEngineTest
{};

// ── rng(seed) reproducibility ───────────────────────────────────────

TEST_P(RngTest, RandReproducibleAfterSeed)
{
    eval("rng(42); a = rand();");
    const double a1 = getVar("a");
    eval("rng(42); a = rand();");
    const double a2 = getVar("a");
    EXPECT_DOUBLE_EQ(a1, a2);
}

TEST_P(RngTest, RandnReproducibleAfterSeed)
{
    eval("rng(7); a = randn();");
    const double a1 = getVar("a");
    eval("rng(7); a = randn();");
    const double a2 = getVar("a");
    EXPECT_DOUBLE_EQ(a1, a2);
}

TEST_P(RngTest, RandAndRandnShareSequence)
{
    // After rng(seed), a sequence of rand+randn must reproduce when
    // we re-seed and rerun the same sequence — proves the streams
    // share one engine.
    eval("rng(13); a1 = rand(); a2 = randn(); a3 = rand();");
    const double r1a = getVar("a1"), r1b = getVar("a2"), r1c = getVar("a3");
    eval("rng(13); a1 = rand(); a2 = randn(); a3 = rand();");
    EXPECT_DOUBLE_EQ(getVar("a1"), r1a);
    EXPECT_DOUBLE_EQ(getVar("a2"), r1b);
    EXPECT_DOUBLE_EQ(getVar("a3"), r1c);
}

TEST_P(RngTest, RngDefaultIsRng0)
{
    eval("rng('default'); a = rand();");
    const double a1 = getVar("a");
    eval("rng(0); a = rand();");
    EXPECT_DOUBLE_EQ(getVar("a"), a1);
}

TEST_P(RngTest, RngStateRoundTrip)
{
    // Snapshot, advance, restore — must reproduce.
    eval("rng(99); s = rng(); a1 = rand(); a2 = rand(); rng(s); b1 = rand(); b2 = rand();");
    EXPECT_DOUBLE_EQ(getVar("a1"), getVar("b1"));
    EXPECT_DOUBLE_EQ(getVar("a2"), getVar("b2"));
}

TEST_P(RngTest, RngWithNargoutReturnsPriorState)
{
    // `prev = rng(123)` snapshots the engine's CURRENT state, then
    // seeds to 123. Restoring `prev` and calling rand() must produce
    // the SAME number that the next rand() would have produced before
    // the rng(123) call (= the rand() that follows the snapshot point).
    eval("rng(100); a0 = rand();"        // snapshot point: just after a0
         "expect_next = rand();"          // what rand() would produce next
         "rng(100); a0 = rand();"         // re-arrive at the snapshot point
         "prev = rng(123);"               // snapshot here, then seed 123
         "rng(prev);"                     // restore snapshot
         "b0 = rand();");                 // should match expect_next
    EXPECT_DOUBLE_EQ(getVar("expect_next"), getVar("b0"));
}

TEST_P(RngTest, RngShuffleProducesNonDeterministic)
{
    // Two shuffles in a row should (almost certainly) give different rand()s.
    // Statistical chance of collision = 2^-32 since we use random_device.
    eval("rng('shuffle'); a = rand(); rng('shuffle'); b = rand();");
    EXPECT_NE(getVar("a"), getVar("b"));
}

// ── randi ───────────────────────────────────────────────────────────

TEST_P(RngTest, RandiScalarInRange)
{
    eval("rng(1); for k = 1:50, v(k) = randi(10); end");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 50u);
    for (size_t i = 0; i < v->numel(); ++i) {
        const double x = v->doubleData()[i];
        EXPECT_GE(x, 1.0);
        EXPECT_LE(x, 10.0);
        EXPECT_DOUBLE_EQ(x, std::floor(x));  // integer
    }
}

TEST_P(RngTest, RandiMatrixShape)
{
    eval("rng(2); A = randi(100, 3, 4);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 3u);
    EXPECT_EQ(cols(*A), 4u);
    for (size_t i = 0; i < A->numel(); ++i) {
        EXPECT_GE(A->doubleData()[i], 1.0);
        EXPECT_LE(A->doubleData()[i], 100.0);
    }
}

TEST_P(RngTest, RandiRangeForm)
{
    eval("rng(3); for k = 1:50, v(k) = randi([10 20]); end");
    auto *v = getVarPtr("v");
    for (size_t i = 0; i < v->numel(); ++i) {
        EXPECT_GE(v->doubleData()[i], 10.0);
        EXPECT_LE(v->doubleData()[i], 20.0);
    }
}

TEST_P(RngTest, RandiRangeFormWithShape)
{
    eval("rng(4); A = randi([-5 5], 2, 3);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 2u);
    EXPECT_EQ(cols(*A), 3u);
    for (size_t i = 0; i < A->numel(); ++i) {
        EXPECT_GE(A->doubleData()[i], -5.0);
        EXPECT_LE(A->doubleData()[i], 5.0);
    }
}

TEST_P(RngTest, RandiReproducibleAfterSeed)
{
    eval("rng(42); A = randi(1000, 5, 5);");
    auto *A1 = getVarPtr("A");
    std::vector<double> snap(A1->numel());
    std::copy(A1->doubleData(), A1->doubleData() + A1->numel(), snap.data());
    eval("rng(42); A = randi(1000, 5, 5);");
    auto *A2 = getVarPtr("A");
    for (size_t i = 0; i < snap.size(); ++i)
        EXPECT_DOUBLE_EQ(A2->doubleData()[i], snap[i]);
}

// ── randperm ────────────────────────────────────────────────────────

TEST_P(RngTest, RandpermFullPermutation)
{
    eval("rng(5); p = randperm(10);");
    auto *p = getVarPtr("p");
    EXPECT_EQ(p->numel(), 10u);
    // All values in [1..10] and unique.
    std::vector<int> seen(11, 0);
    for (size_t i = 0; i < p->numel(); ++i) {
        const double v = p->doubleData()[i];
        EXPECT_GE(v, 1.0);
        EXPECT_LE(v, 10.0);
        const int iv = static_cast<int>(v);
        EXPECT_EQ(seen[iv], 0) << "duplicate value " << iv;
        seen[iv] = 1;
    }
}

TEST_P(RngTest, RandpermPartialKLessThanN)
{
    eval("rng(6); p = randperm(20, 5);");
    auto *p = getVarPtr("p");
    EXPECT_EQ(p->numel(), 5u);
    std::vector<int> seen(21, 0);
    for (size_t i = 0; i < p->numel(); ++i) {
        const double v = p->doubleData()[i];
        EXPECT_GE(v, 1.0);
        EXPECT_LE(v, 20.0);
        const int iv = static_cast<int>(v);
        EXPECT_EQ(seen[iv], 0) << "duplicate value " << iv;
        seen[iv] = 1;
    }
}

TEST_P(RngTest, RandpermReproducibleAfterSeed)
{
    eval("rng(8); p = randperm(15);");
    auto *p1 = getVarPtr("p");
    std::vector<double> snap(p1->numel());
    std::copy(p1->doubleData(), p1->doubleData() + p1->numel(), snap.data());
    eval("rng(8); p = randperm(15);");
    auto *p2 = getVarPtr("p");
    for (size_t i = 0; i < snap.size(); ++i)
        EXPECT_DOUBLE_EQ(p2->doubleData()[i], snap[i]);
}

TEST_P(RngTest, RandpermKEqualsN)
{
    eval("rng(9); p = randperm(7, 7);");
    auto *p = getVarPtr("p");
    EXPECT_EQ(p->numel(), 7u);
}

TEST_P(RngTest, RandpermKBiggerThanNThrows)
{
    EXPECT_THROW(eval("randperm(3, 5);"), std::runtime_error);
}

TEST_P(RngTest, RandiBadRangeThrows)
{
    EXPECT_THROW(eval("randi([10 5], 3);"), std::runtime_error);
}

INSTANTIATE_DUAL(RngTest);
