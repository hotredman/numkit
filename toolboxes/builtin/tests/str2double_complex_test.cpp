// toolboxes/builtin/tests/str2double_complex_test.cpp
//
// Regression guard for bugs/builtin/str2double-complex.md: str2double now parses
// complex-number strings (was NaN). MATLAB R2025b reference values. Real strings
// stay real double (zero regression).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Str2doubleComplexTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Standard a+bi / a-bi.
TEST_F(Str2doubleComplexTest, RealPlusImag)
{
    EXPECT_DOUBLE_EQ(evalScalar("real(str2double('1+2i'))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double('1+2i'))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double('1-2i'))"), -2.0);
    EXPECT_DOUBLE_EQ(evalScalar("real(str2double('3.5+1.5i'))"), 3.5);
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double('3.5+1.5i'))"), 1.5);
    EXPECT_DOUBLE_EQ(evalScalar("isreal(str2double('1+2i'))"), 0.0);
}

// Pure imaginary, bare i, signed i.
TEST_F(Str2doubleComplexTest, PureImaginary)
{
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double('2i'))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("real(str2double('2i'))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double('-3i'))"), -3.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double('i'))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double('-i'))"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double('+i'))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double('1+i'))"), 1.0);   // coeff-less i
    EXPECT_DOUBLE_EQ(evalScalar("real(str2double('1+i'))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double('.5i'))"), 0.5);
}

// j accepted as imaginary; exponent-sign edge; Inf imaginary.
TEST_F(Str2doubleComplexTest, JformExponentInf)
{
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double('1+2j'))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("real(str2double('1+2j'))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double('2j'))"), 2.0);
    EXPECT_NEAR(evalScalar("real(str2double('1e-3+2i'))"), 0.001, 1e-12);  // exponent '-' not a split
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double('1e-3+2i'))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double('1e3i'))"), 1000.0);
    EXPECT_DOUBLE_EQ(evalScalar("isinf(imag(str2double('Infi')))"), 1.0);
}

// Spaces inside the literal; negative real + negative imag.
TEST_F(Str2doubleComplexTest, SpacesAndSigns)
{
    EXPECT_DOUBLE_EQ(evalScalar("real(str2double(' 2 + 3i '))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double(' 2 + 3i '))"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("real(str2double('-2-3i'))"), -2.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double('-2-3i'))"), -3.0);
}

// Real strings unchanged (DOUBLE, real); capital I is NOT imaginary.
TEST_F(Str2doubleComplexTest, RealStringsUnaffected)
{
    EXPECT_DOUBLE_EQ(evalScalar("str2double('5')"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("isreal(str2double('5'))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("str2double('1.5')"), 1.5);
    EXPECT_DOUBLE_EQ(evalScalar("isinf(str2double('Inf'))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isnan(str2double('NaN'))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isnan(str2double('1+2I'))"), 1.0);  // capital I -> NaN
}

// Cell mix of real + complex -> COMPLEX array of the input shape.
TEST_F(Str2doubleComplexTest, CellMix)
{
    eval("c = str2double({'1+2i','3','4-1i'});");
    EXPECT_DOUBLE_EQ(evalScalar("isreal(c)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("real(c(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(c(1))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("real(c(2))"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(c(2))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(c(3))"), -1.0);
    // all-real cell stays real double.
    eval("d = str2double({'1.5','2.5'});");
    EXPECT_DOUBLE_EQ(evalScalar("isreal(d)"), 1.0);
}
