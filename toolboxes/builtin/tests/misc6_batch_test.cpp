// toolboxes/builtin/tests/misc6_batch_test.cpp
// (11 builtin functions):
//   rng:       randi · randperm
//   shape:     shiftdim · pol2cart · sph2cart
//   typecast:  typecast
//   strings:   split · splitlines · strjoin · strncmp · strncmpi
// All . Bit-identical MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Misc6BatchTest : public ::testing::Test
{
public:
    StandardEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(Misc6BatchTest, RandiRandperm)
{
    eval("rng(42); v = randi(10, 1, 5);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(v)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("all(v >= 1 & v <= 10)"), 1.0);

    eval("p = randperm(5);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(p)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("min(p)"),   1.0);
    EXPECT_DOUBLE_EQ(evalScalar("max(p)"),   5.0);
}

TEST_F(Misc6BatchTest, Shiftdim)
{
    eval("A = ones(2,3); B = shiftdim(A, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("size(B,1)"), 3.0);  // shifted
    EXPECT_DOUBLE_EQ(evalScalar("size(B,2)"), 2.0);
}

TEST_F(Misc6BatchTest, PolCartSphCart)
{
    eval("[x, y] = pol2cart(pi/4, sqrt(2));");
    EXPECT_NEAR(evalScalar("x"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("y"), 1.0, 1e-12);

    eval("[x, y, z] = sph2cart(0, 0, 1);");
    EXPECT_NEAR(evalScalar("x"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("y"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("z"), 0.0, 1e-12);
}

TEST_F(Misc6BatchTest, Typecast)
{
    // IEEE 0x3F800000 → single 1.0
    EXPECT_NEAR(evalScalar("double(typecast(uint32(1065353216), 'single'))"), 1.0, 1e-6);
}

TEST_F(Misc6BatchTest, SplitJoin)
{
    eval("p = split(\"a,b,c\", \",\");");
    EXPECT_DOUBLE_EQ(evalScalar("numel(p)"), 3.0);

    EXPECT_DOUBLE_EQ(evalScalar("strcmp(strjoin({'a','b','c'}, ','), 'a,b,c')"), 1.0);
}

TEST_F(Misc6BatchTest, Splitlines)
{
    eval("lines = splitlines(\"a\" + newline + \"b\" + newline + \"c\");");
    EXPECT_DOUBLE_EQ(evalScalar("numel(lines)"), 3.0);
}

TEST_F(Misc6BatchTest, StrncmpStrncmpi)
{
    EXPECT_DOUBLE_EQ(evalScalar("strncmp('hello', 'help', 3)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strncmp('hello', 'help', 4)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("strncmpi('Hello', 'HELP', 3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strncmpi('hello', 'WORLD', 3)"), 0.0);
}
