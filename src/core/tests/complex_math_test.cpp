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

// numkit has TWO round implementations: the real/SIMD path uses
// Trunc(v+CopySign(0.5,v)); the complex path applies std::round to each
// component. They must agree on tie-prone magnitudes so real-round and
// complex-component-round never silently diverge (the fused kernels mirror
// each path, so this also guards fused real-vs-complex round).
TEST_P(ComplexMathTest, RoundFamilyRealVsComplexComponentAgree) {
    e.eval("xs = [0.5 1.5 2.5 3.5 -0.5 -1.5 -2.5 -3.5 100.5 -100.5 0];");
    EXPECT_TRUE(e.eval("isequaln(round(xs), real(round(xs + 0i)))").toBool());
    EXPECT_TRUE(e.eval("isequaln(floor(xs), real(floor(xs + 0i)))").toBool());
    EXPECT_TRUE(e.eval("isequaln(ceil(xs),  real(ceil(xs + 0i)))").toBool());
    EXPECT_TRUE(e.eval("isequaln(fix(xs),   real(fix(xs + 0i)))").toBool());
}

// MATLAB narrows a complex ARITHMETIC result whose imaginary part is all-zero
// back to a real double (bugs/math/complex-zero-imag-narrowing.md); complex() is
// the exception (forced complex). This is what makes max([1 -3 2], 2+0i) compare
// by value, not |z|.
TEST_P(ComplexMathTest, NarrowsArithmeticAllReal) {
    EXPECT_TRUE(e.eval("isreal(2 + 0i)").toBool());                       // + narrows
    EXPECT_TRUE(e.eval("isreal((1+1i) + (1-1i))").toBool());              // -> 2
    EXPECT_TRUE(e.eval("isreal(complex(1,1) .* complex(1,-1))").toBool());// forced in, real out
    EXPECT_FALSE(e.eval("isreal(complex(2,0))").toBool());               // complex() forced
    EXPECT_FALSE(e.eval("isreal(1 + 2i)").toBool());                     // genuine complex stays
    // headline: 2+0i narrows -> real, so max uses value comparison (= MATLAB [2 2 2]).
    e.eval("m = max([1 -3 2], 2+0i);");
    EXPECT_TRUE(e.eval("isreal(m)").toBool());
    EXPECT_EQ(re("m(1)"), 2.0); EXPECT_EQ(re("m(2)"), 2.0); EXPECT_EQ(re("m(3)"), 2.0);
    // unary (uminus/conj) and matmul of a FORCED-complex all-real value also narrow
    EXPECT_TRUE(e.eval("isreal(-(complex(2,0)))").toBool());            // uminus
    EXPECT_TRUE(e.eval("isreal(conj(complex(2,0)))").toBool());         // conj
    EXPECT_TRUE(e.eval("isreal(complex([1 2;3 4]) * complex([1 0;0 1]))").toBool()); // matmul
}

// Indexing a complex array narrows an all-real slice/element to real (MATLAB
// R2025b: isreal(z(k))==1 when the picked elements have no imaginary part) —
// while the FORCED-complex source itself stays complex.
TEST_P(ComplexMathTest, NarrowsIndexingAllRealSlice) {
    e.eval("zc = complex([1 -3 2]);");                 // forced complex, all imag 0
    EXPECT_FALSE(e.eval("isreal(zc)").toBool());       // source stays complex
    EXPECT_TRUE(e.eval("isreal(zc(2))").toBool());     // scalar index narrows
    EXPECT_TRUE(e.eval("isreal(zc(1:2))").toBool());   // range narrows
    EXPECT_TRUE(e.eval("isreal(zc(:))").toBool());     // colon-linearize narrows (MATLAB)
    e.eval("zg = [1+1i, 2, 3];");                      // genuinely complex
    EXPECT_FALSE(e.eval("isreal(zg(1))").toBool());    // 1+1i element stays complex
    EXPECT_TRUE(e.eval("isreal(zg(2))").toBool());     // real element narrows
}

// Residual structural / reduction ops also narrow an all-real complex result
// (MATLAB R2025b). Forced-complex() sources (arithmetic already narrows at source).
TEST_P(ComplexMathTest, NarrowsResidualOps) {
    e.eval("import compat.*;");  // median resolves through the stats namespace
    // reductions (sum/prod/mean route through the reduction adapter; cumsum /
    // cumprod / diff / median have their own compute paths).
    EXPECT_TRUE(e.eval("isreal(sum(complex([1 2 3])))").toBool());
    EXPECT_TRUE(e.eval("isreal(prod(complex([1 2 3])))").toBool());
    EXPECT_TRUE(e.eval("isreal(mean(complex([1 2 3])))").toBool());
    EXPECT_TRUE(e.eval("isreal(cumsum(complex([1 2 3])))").toBool());
    EXPECT_TRUE(e.eval("isreal(cumprod(complex([1 2 3])))").toBool());
    EXPECT_TRUE(e.eval("isreal(diff(complex([1 2 4])))").toBool());
    EXPECT_TRUE(e.eval("isreal(median(complex([1 2 3])))").toBool());
    // linear algebra (dot / kron / cross / diag).
    EXPECT_TRUE(e.eval("isreal(dot(complex([1 2 3]), [4 5 6]))").toBool());
    EXPECT_TRUE(e.eval("isreal(kron(complex([1 2]), [3 4]))").toBool());
    EXPECT_TRUE(e.eval("isreal(cross(complex([1 2 3]), [4 5 6]))").toBool());
    EXPECT_TRUE(e.eval("isreal(diag(complex([1 2 3])))").toBool());
    // structural reorder.
    EXPECT_TRUE(e.eval("isreal(fliplr(complex([1 2 3])))").toBool());
    EXPECT_TRUE(e.eval("isreal(flipud(complex([1;2;3])))").toBool());
    EXPECT_TRUE(e.eval("isreal(circshift(complex([1 2 3]), 1))").toBool());
    EXPECT_TRUE(e.eval("isreal(repmat(complex([1 2]), 1, 2))").toBool());
    e.eval("zc = complex([1 2 3]);");
    EXPECT_TRUE(e.eval("isreal(zc(:))").toBool());                        // colon-linearize
    e.eval("ca = complex([0 0 0]); ca(2) = 7;");
    EXPECT_TRUE(e.eval("isreal(ca)").toBool());                           // indexed-assign
    // Over-narrow guard: a genuinely complex result of the same ops must STAY
    // complex (narrowing only fires when the imaginary part is entirely zero).
    EXPECT_FALSE(e.eval("isreal(cumsum([1+1i, 2, 3]))").toBool());
    EXPECT_FALSE(e.eval("isreal(diag([1i 2 3]))").toBool());
    EXPECT_FALSE(e.eval("isreal(dot([1i 2], [3 4]))").toBool());
}

INSTANTIATE_TEST_SUITE_P(Backends, ComplexMathTest,
                         ::testing::Values(numkit::Engine::Backend::TreeWalker,
                                           numkit::Engine::Backend::VM));

} // namespace
