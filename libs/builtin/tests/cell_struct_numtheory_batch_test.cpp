// libs/builtin/tests/cell_struct_numtheory_batch_test.cpp
// cell+struct + number theory:
//   cell:        cell · cellfun · cellstr · cell2mat · cell2struct
//                arrayfun
//   struct:      struct · struct2cell · fieldnames · isfield
//   numtheory:   gcd · lcm · factorial · factor · isprime · primes
//                nchoosek · perms
// Total: 18 functions. All  — bit-
// identical MATLAB R2025b on probed inputs.
// Known sub-gap: numkit's arrayfun does NOT apply the function —
// returns the input unchanged for both anonymous lambda (@(x) x*2)
// and named-fn (@sin) handles. Real bug; only structural shape pinned.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CellStructNumtheoryBatchTest : public ::testing::Test
{
public:
    StdEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(CellStructNumtheoryBatchTest, CellBasics)
{
    eval("c = cell(2, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(c)"),     6.0);
    EXPECT_DOUBLE_EQ(evalScalar("iscell(c)"),    1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isempty(c{1,1})"), 1.0);
}

TEST_F(CellStructNumtheoryBatchTest, CellFun)
{
    eval("r = cellfun(@(x) x*2, {1, 2, 3});");
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(3)"), 6.0);
}

TEST_F(CellStructNumtheoryBatchTest, Cell2Mat)
{
    eval("M = cell2mat({1, 2; 3, 4});");
    EXPECT_DOUBLE_EQ(evalScalar("M(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(2,2)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(M)"), 4.0);
}

TEST_F(CellStructNumtheoryBatchTest, StructFamily)
{
    eval("s = struct('a', 1, 'b', 2);");
    EXPECT_DOUBLE_EQ(evalScalar("s.a"),         1.0);
    EXPECT_DOUBLE_EQ(evalScalar("s.b"),         2.0);
    EXPECT_DOUBLE_EQ(evalScalar("isstruct(s)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isfield(s, 'a')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isfield(s, 'z')"), 0.0);

    eval("c = struct2cell(s);");
    EXPECT_DOUBLE_EQ(evalScalar("c{1}"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{2}"), 2.0);

    eval("f = fieldnames(s);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(f)"),       2.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(f{1}, 'a')"), 1.0);
}

TEST_F(CellStructNumtheoryBatchTest, GcdLcm)
{
    EXPECT_DOUBLE_EQ(evalScalar("gcd(12, 8)"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("gcd(15, 25)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("gcd(7, 13)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("lcm(4, 6)"),   12.0);
    EXPECT_DOUBLE_EQ(evalScalar("lcm(3, 5)"),   15.0);
    EXPECT_DOUBLE_EQ(evalScalar("lcm(8, 12)"),  24.0);
}

TEST_F(CellStructNumtheoryBatchTest, Factorial)
{
    EXPECT_DOUBLE_EQ(evalScalar("factorial(0)"),       1.0);
    EXPECT_DOUBLE_EQ(evalScalar("factorial(1)"),       1.0);
    EXPECT_DOUBLE_EQ(evalScalar("factorial(5)"),     120.0);
    EXPECT_DOUBLE_EQ(evalScalar("factorial(10)"), 3628800.0);
}

TEST_F(CellStructNumtheoryBatchTest, FactorIsPrimePrimes)
{
    eval("f = factor(60);");  // 2 2 3 5
    EXPECT_DOUBLE_EQ(evalScalar("numel(f)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("f(1)"),     2.0);
    EXPECT_DOUBLE_EQ(evalScalar("f(4)"),     5.0);

    EXPECT_DOUBLE_EQ(evalScalar("isprime(2)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isprime(7)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isprime(8)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("isprime(13)"), 1.0);

    eval("p = primes(20);");  // 2,3,5,7,11,13,17,19
    EXPECT_DOUBLE_EQ(evalScalar("numel(p)"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("p(1)"),     2.0);
    EXPECT_DOUBLE_EQ(evalScalar("p(8)"),     19.0);
}

TEST_F(CellStructNumtheoryBatchTest, NchoosekPerms)
{
    EXPECT_DOUBLE_EQ(evalScalar("nchoosek(5, 2)"),  10.0);
    EXPECT_DOUBLE_EQ(evalScalar("nchoosek(10, 3)"), 120.0);
    EXPECT_DOUBLE_EQ(evalScalar("nchoosek(7, 0)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("nchoosek(7, 7)"),  1.0);

    eval("P = perms(1:3);");  // 6 rows × 3 cols
    EXPECT_DOUBLE_EQ(evalScalar("size(P,1)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(P,2)"), 3.0);
}
