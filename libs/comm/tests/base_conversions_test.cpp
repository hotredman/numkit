// libs/comm/tests/base_conversions_test.cpp
//
// Regression guard for the Communications Toolbox base-conversion
// utilities: bit2int / int2bit / bi2de / de2bi / vec2mat.

#include <numkit/comm/source/base_conversions.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BaseConversionsTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── bit2int ───────────────────────────────────────────────────────────
TEST_F(BaseConversionsTest, Bit2intMsbFirstDefault)
{
    eval("b = [1 0 1 0 1 1 0 0]'; r = bit2int(b, 4);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(r)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 10.0);  // 1010 = 10
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 12.0);  // 1100 = 12
}

TEST_F(BaseConversionsTest, Bit2intLsbFirst)
{
    eval("b = [1 0 1 0 1 1 0 0]'; r = bit2int(b, 4, false);");
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 5.0);  // 0101 reversed → 5
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 3.0);
}

TEST_F(BaseConversionsTest, Bit2intRejectsBadLength)
{
    bool threw = false;
    try { eval("bit2int([1 0 1 0 1]', 4);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

// ── int2bit ───────────────────────────────────────────────────────────
TEST_F(BaseConversionsTest, Int2bitInverseOfBit2int)
{
    eval("b = int2bit([10 12], 4);");
    // 4×2: column k = bits of input k, MSB on top.
    EXPECT_EQ(static_cast<int>(evalScalar("size(b, 1)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(b, 2)")), 2);
    // 10 = 1010
    EXPECT_DOUBLE_EQ(evalScalar("b(1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(2, 1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(3, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(4, 1)"), 0.0);
    // 12 = 1100
    EXPECT_DOUBLE_EQ(evalScalar("b(1, 2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(4, 2)"), 0.0);
}

TEST_F(BaseConversionsTest, RoundTripBit2intInt2bit)
{
    eval("orig = [3 7 1 5 2 6 0 4]'; "
         "round = bit2int(int2bit(orig, 4), 4);"
         "diff = max(abs(round - orig));");
    EXPECT_DOUBLE_EQ(evalScalar("diff"), 0.0);
}

// ── bi2de ─────────────────────────────────────────────────────────────
TEST_F(BaseConversionsTest, Bi2deDefaultLsbFirst)
{
    // bi2de default is 'right-msb' = LSB-first within row.
    // Row [1 0 1 0]: 1*1 + 0*2 + 1*4 + 0*8 = 5
    // Row [0 0 1 1]: 0 + 0 + 4 + 8 = 12
    eval("d = bi2de([1 0 1 0; 0 0 1 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("d(1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("d(2)"), 12.0);
}

TEST_F(BaseConversionsTest, Bi2deLeftMsb)
{
    // Row [1 0 1 0] left-msb: 1*8 + 0*4 + 1*2 + 0*1 = 10
    eval("d = bi2de([1 0 1 0; 0 0 1 1], 'left-msb');");
    EXPECT_DOUBLE_EQ(evalScalar("d(1)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("d(2)"), 3.0);
}

TEST_F(BaseConversionsTest, Bi2deCustomBase)
{
    // Row [4 3] base 10 LSB: 4*1 + 3*10 = 34
    eval("d = bi2de([4 3; 1 2], 10);");
    EXPECT_DOUBLE_EQ(evalScalar("d(1)"), 34.0);
    EXPECT_DOUBLE_EQ(evalScalar("d(2)"), 21.0);
}

// ── de2bi ─────────────────────────────────────────────────────────────
TEST_F(BaseConversionsTest, De2biAutoWidthLsb)
{
    eval("b = de2bi([5 12]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(b, 2)")), 4);
    // Row 1 = bits of 5 LSB: [1 0 1 0]
    EXPECT_DOUBLE_EQ(evalScalar("b(1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(1, 3)"), 1.0);
}

TEST_F(BaseConversionsTest, De2biExplicitWidthPads)
{
    eval("b = de2bi([5 12], 5);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(b, 2)")), 5);
    EXPECT_DOUBLE_EQ(evalScalar("b(1, 5)"), 0.0);  // padded zero
}

TEST_F(BaseConversionsTest, De2biLeftMsb)
{
    eval("b = de2bi([5 12], 5, 'left-msb');");
    // 5 = 00101 left-msb in 5 bits
    EXPECT_DOUBLE_EQ(evalScalar("b(1, 5)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(1, 4)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(1, 3)"), 1.0);
}

TEST_F(BaseConversionsTest, De2biEmptyWidthWithBase)
{
    // de2bi(d, [], base): empty width = auto, with a custom base. MATLAB:
    // de2bi(10,[],3) -> [1 0 1] (base-3, LSB-first, min width). Used to throw
    // "Cannot convert double to scalar" on the empty [] width arg.
    eval("b = de2bi(10, [], 3);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(b, 2)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("b(1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(1, 2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(1, 3)"), 1.0);
}

TEST_F(BaseConversionsTest, De2biExplicitWidthWithBase)
{
    // de2bi(10, 4, 3) -> [1 0 1 0] (base-3 padded to 4 digits).
    eval("b = de2bi(10, 4, 3);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(b, 2)")), 4);
    EXPECT_DOUBLE_EQ(evalScalar("b(1, 4)"), 0.0);
}

TEST_F(BaseConversionsTest, De2biEmptyWidthBaseLeftMsb)
{
    // Empty width + base + flag together.  6 base-2 left-msb = [1 1 0].
    eval("b = de2bi(6, [], 2, 'left-msb');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(b, 2)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("b(1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(1, 3)"), 0.0);
}

TEST_F(BaseConversionsTest, Bi2deEmptyBaseDefaults)
{
    // bi2de(b, []) keeps the default base 2 (empty arg must not throw).
    eval("d = bi2de([0 1 1], []);");
    EXPECT_DOUBLE_EQ(evalScalar("d"), 6.0);
}

// ── vec2mat ───────────────────────────────────────────────────────────
TEST_F(BaseConversionsTest, Vec2matRowMajorFillWithPadCount)
{
    eval("[m, p] = vec2mat([1 2 3 4 5 6 7 8 9 10], 4);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(m, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(m, 2)")), 4);
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 4)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2, 1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(3, 2)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(3, 3)"), 0.0);  // padded
    EXPECT_DOUBLE_EQ(evalScalar("m(3, 4)"), 0.0);  // padded
    EXPECT_DOUBLE_EQ(evalScalar("p"), 2.0);
}

TEST_F(BaseConversionsTest, Vec2matCustomPad)
{
    eval("m = vec2mat([1 2 3 4 5], 3, 99);");
    EXPECT_DOUBLE_EQ(evalScalar("m(2, 2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2, 3)"), 99.0);  // custom pad
}

TEST_F(BaseConversionsTest, Vec2matExactFit)
{
    // 6 elements, n=3 → 2×3 matrix, no padding.
    eval("[m, p] = vec2mat([1 2 3 4 5 6], 3);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(m, 1)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("p"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2, 3)"), 6.0);
}

// ── Public C++ API (numkit::comm::*) ──────────────────────────────────
// Exercise the typed entry points directly (not via the engine), guarding
// the lift from adapter-only to a public header with the LIBRARY_API
// signature (mr last, default nullptr; native scalar option args).
namespace {
Value cvar(Engine &e, const char *expr, const char *name)
{
    e.eval(std::string(name) + " = " + expr + ";");
    return *e.getVariable(name);
}
} // namespace

TEST_F(BaseConversionsTest, PublicApiBit2intInt2bit)
{
    Value b = cvar(engine, "[1 0 1 0 1 1 0 0]'", "b");
    // defaults: msbfirst = true, mr = nullptr (process default)
    Value r = comm::bit2int(b, 4);
    ASSERT_EQ(r.numel(), 2u);
    EXPECT_DOUBLE_EQ(r.doubleData()[0], 10.0); // 1010
    EXPECT_DOUBLE_EQ(r.doubleData()[1], 12.0); // 1100
    // explicit LSB-first + explicit mr
    Value rl = comm::bit2int(b, 4, false, engine.resource());
    EXPECT_DOUBLE_EQ(rl.doubleData()[0], 5.0);
    // int2bit inverse -> 4x2 bit matrix
    Value bb = comm::int2bit(cvar(engine, "[10 12]", "d"), 4);
    EXPECT_EQ(bb.dims().rows(), 4u);
    EXPECT_EQ(bb.dims().cols(), 2u);
    EXPECT_DOUBLE_EQ(bb.doubleData()[0], 1.0); // (1,1) = MSB of 10
}

TEST_F(BaseConversionsTest, PublicApiBi2deDe2bi)
{
    Value m = cvar(engine, "[1 0 1 0; 0 0 1 1]", "m");
    // defaults: base 2, LSB-first ('right-msb')
    Value d = comm::bi2de(m);
    EXPECT_DOUBLE_EQ(d.doubleData()[0], 5.0);
    EXPECT_DOUBLE_EQ(d.doubleData()[1], 12.0);
    // left-msb via the bool flag
    Value dl = comm::bi2de(m, 2, true, engine.resource());
    EXPECT_DOUBLE_EQ(dl.doubleData()[0], 10.0);
    // de2bi auto width (n = -1 default) -> 2x4
    Value bb = comm::de2bi(cvar(engine, "[5 12]", "v"));
    EXPECT_EQ(bb.dims().cols(), 4u);
    EXPECT_DOUBLE_EQ(bb.doubleData()[0], 1.0); // (1,1) = LSB of 5
}

TEST_F(BaseConversionsTest, PublicApiVec2mat)
{
    auto [mat, pad] = comm::vec2mat(
        cvar(engine, "[1 2 3 4 5 6 7 8 9 10]", "v"), 4);
    EXPECT_EQ(mat.dims().rows(), 3u);
    EXPECT_EQ(mat.dims().cols(), 4u);
    EXPECT_EQ(pad, 2);
    // custom pad + explicit mr; 5 elems, n=3 -> 2x3, one pad at (2,3)
    auto [mat2, pad2] =
        comm::vec2mat(cvar(engine, "[1 2 3 4 5]", "v2"), 3, 99.0, engine.resource());
    EXPECT_EQ(pad2, 1);
    EXPECT_DOUBLE_EQ(mat2.doubleData()[1 + 2 * 2], 99.0); // (2,3) col-major
}
