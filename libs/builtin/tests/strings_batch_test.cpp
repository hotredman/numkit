// libs/builtin/tests/strings_batch_test.cpp
//
// Audit ТЗ batch closure for string ops — 14 functions:
//   lower / upper / strtrim / deblank / blanks
//   strlength / strrep / strfind / strcat / strsplit / strtok
//   contains / startsWith / endsWith
//
// All flagged "no major gap detected" — bit-identical MATLAB R2025b
// on probed inputs.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class StringsBatchTest : public ::testing::Test
{
public:
    Engine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
    std::string evalString(const std::string &c) { return eval(c).toString(); }
};

TEST_F(StringsBatchTest, LowerUpper)
{
    EXPECT_EQ(evalString("lower(\"Hello WORLD\")"), "hello world");
    EXPECT_EQ(evalString("upper(\"abc XYZ\")"),     "ABC XYZ");
    EXPECT_EQ(evalString("lower(\"\")"),            "");
}

TEST_F(StringsBatchTest, TrimDeblank)
{
    EXPECT_EQ(evalString("strtrim(\"  abc  \")"), "abc");
    EXPECT_EQ(evalString("deblank(\"abc   \")"),  "abc");
}

TEST_F(StringsBatchTest, Blanks)
{
    EXPECT_DOUBLE_EQ(evalScalar("strlength(blanks(5))"), 5.0);
}

TEST_F(StringsBatchTest, Strlength)
{
    EXPECT_DOUBLE_EQ(evalScalar("strlength(\"hello\")"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("strlength(\"\")"),      0.0);
}

TEST_F(StringsBatchTest, Strrep)
{
    EXPECT_EQ(evalString("strrep(\"hello world\", \"world\", \"matlab\")"), "hello matlab");
}

TEST_F(StringsBatchTest, Strfind)
{
    eval("ix = strfind(\"hello world\", \"o\");");
    EXPECT_DOUBLE_EQ(evalScalar("ix(1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("ix(2)"), 8.0);
}

TEST_F(StringsBatchTest, Contains)
{
    EXPECT_DOUBLE_EQ(evalScalar("contains(\"hello\", \"ell\")"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("contains(\"hello\", \"xyz\")"), 0.0);
}

TEST_F(StringsBatchTest, StartsEndsWith)
{
    EXPECT_DOUBLE_EQ(evalScalar("startsWith(\"hello\", \"he\")"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("startsWith(\"hello\", \"ell\")"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("endsWith(\"hello\", \"lo\")"),    1.0);
    EXPECT_DOUBLE_EQ(evalScalar("endsWith(\"hello\", \"ell\")"),   0.0);
}

TEST_F(StringsBatchTest, Strcat)
{
    EXPECT_EQ(evalString("strcat(\"ab\", \"cd\")"),       "abcd");
    EXPECT_EQ(evalString("strcat(\"a\", \"b\", \"c\")"),  "abc");
}

TEST_F(StringsBatchTest, Strsplit)
{
    eval("p = strsplit(\"a b c\");");
    EXPECT_DOUBLE_EQ(evalScalar("numel(p)"), 3.0);
}

TEST_F(StringsBatchTest, Strtok)
{
    eval("[t, rem] = strtok(\"hello world\");");
    EXPECT_EQ(evalString("t"),   "hello");
    EXPECT_EQ(evalString("rem"), " world");
}
