// core/tests/complex_math_test.cpp
//
// Complex-input support for the elementary math that numkit used to reject with
// "Not a double array": floor/ceil/round/fix apply component-wise to the real
// and imaginary parts (MATLAB R2025b), and expm1(z) = exp(z) - 1. Expected
// values verified against MATLAB R2025b. Both backends.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>

#include <gtest/gtest.h>

#include <string>

namespace {

class ComplexMathTest : public ::testing::TestWithParam<numkit::Engine::Backend> {
protected:
    numkit::StandardEngine e;
    void SetUp() override { e.setBackend(GetParam()); }
    double re(const char *expr) {
        return e.eval(std::string("real(") + expr + ")").toScalar();
    }
    double im(const char *expr) {
        return e.eval(std::string("imag(") + expr + ")").toScalar();
    }
};

// floor/ceil/round/fix of a complex array: round each component independently.
// MATLAB: floor(3+4.7i)=3+4i, round(-1.5-2.5i)=-2-3i (half-away), fix(0.5..)=0.
TEST_P(ComplexMathTest, FloorCeilRoundFixComponentWise) {
    e.eval("z = [3+4.7i, -1.5-2.5i, 0.5+0.5i]; "
           "fl = floor(z); ce = ceil(z); ro = round(z); fx = fix(z);");
    EXPECT_EQ(re("fl(1)"), 3.0);   EXPECT_EQ(im("fl(1)"), 4.0);   // floor(3+4.7i)
    EXPECT_EQ(re("fl(2)"), -2.0);  EXPECT_EQ(im("fl(2)"), -3.0);  // floor(-1.5-2.5i)
    EXPECT_EQ(re("ce(1)"), 3.0);   EXPECT_EQ(im("ce(1)"), 5.0);   // ceil(3+4.7i)
    EXPECT_EQ(re("ce(3)"), 1.0);   EXPECT_EQ(im("ce(3)"), 1.0);   // ceil(0.5+0.5i)
    EXPECT_EQ(re("ro(1)"), 3.0);   EXPECT_EQ(im("ro(1)"), 5.0);   // round(3+4.7i)
    EXPECT_EQ(re("ro(2)"), -2.0);  EXPECT_EQ(im("ro(2)"), -3.0);  // round(-1.5-2.5i)
    EXPECT_EQ(re("fx(2)"), -1.0);  EXPECT_EQ(im("fx(2)"), -2.0);  // fix(-1.5-2.5i)
    EXPECT_EQ(re("fx(3)"), 0.0);   EXPECT_EQ(im("fx(3)"), 0.0);   // fix(0.5+0.5i)
}

// Scalar form must work too (the old code rejected complex scalars via toScalar).
TEST_P(ComplexMathTest, FloorRoundComplexScalar) {
    EXPECT_EQ(re("floor(1.5+2.7i)"), 1.0);
    EXPECT_EQ(im("floor(1.5+2.7i)"), 2.0);
    EXPECT_EQ(re("round(1.5+2.5i)"), 2.0);
    EXPECT_EQ(im("round(1.5+2.5i)"), 3.0);
}

// expm1(z) = exp(z) - 1 on complex (MATLAB R2025b).
TEST_P(ComplexMathTest, Expm1Complex) {
    EXPECT_NEAR(re("expm1(2+1i)"), 2.99232404844, 1e-9);
    EXPECT_NEAR(im("expm1(2+1i)"), 6.21767631237, 1e-9);
    e.eval("d = expm1(0.5-0.3i) - (exp(0.5-0.3i) - 1);");
    EXPECT_NEAR(e.eval("abs(d)").toScalar(), 0.0, 1e-15);
    // array form
    e.eval("za = [0.1+0.2i, -1+0.5i]; y = expm1(za); w = exp(za) - 1;");
    EXPECT_NEAR(e.eval("max(abs(y - w))").toScalar(), 0.0, 1e-15);
}

INSTANTIATE_TEST_SUITE_P(Backends, ComplexMathTest,
                         ::testing::Values(numkit::Engine::Backend::TreeWalker,
                                           numkit::Engine::Backend::VM));

} // namespace
