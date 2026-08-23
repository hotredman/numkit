// src/math/tests/maxmin_complex_test.cpp
//
// Regression guard for bugs/math/maxmin-complex.md (FIXED): binary (elementwise)
// max(A,B) / min(A,B) — and hence clamp-style max(lo,min(hi,z)) — now accept
// complex operands instead of throwing "Not a double array". MATLAB R2025b
// rule (validated): compare by |z| (modulus), ties broken by angle(z); a
// complex value with a NaN component is "missing" (omitnan default → the other
// wins; includenan → it propagates). NO all-real fallback in the binary form
// (unlike the single-arg reduction): max(complex(-3,0),1) = -3, not 1.
//
// The single-arg reduction max(z)/min(z) already worked (stats_test.cpp
// ReductionDimTest.*ComplexByModulus); this covers the binary form on BOTH
// backends (the prior blocker was a VM-dispatch red herring — max/min default
// to 'omitnan', so the live path is maxOmitNanBinary/minOmitNanBinary).

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>

#include <gtest/gtest.h>

#include <string>

namespace {

class MaxMinComplexTest : public ::testing::TestWithParam<numkit::Engine::Backend> {
protected:
    numkit::StandardEngine e;
    void SetUp() override { e.setBackend(GetParam()); }
    double re(const char *expr) { return e.eval(std::string("real(") + expr + ")").toScalar(); }
    double im(const char *expr) { return e.eval(std::string("imag(") + expr + ")").toScalar(); }
};

// max/min of a complex array against a real scalar — compare by |z|, broadcast.
// |3+4i|=5>1, |0.2-0.1i|≈0.22<1.
TEST_P(MaxMinComplexTest, BinaryByModulusBroadcast) {
    e.eval("mx = max([3+4i, 0.2-0.1i], 1); mn = min([3+4i, 0.2-0.1i], 1);");
    EXPECT_EQ(re("mx(1)"), 3.0);  EXPECT_EQ(im("mx(1)"), 4.0);   // 3+4i wins
    EXPECT_EQ(re("mx(2)"), 1.0);  EXPECT_EQ(im("mx(2)"), 0.0);   // 1 wins
    EXPECT_EQ(re("mn(1)"), 1.0);  EXPECT_EQ(im("mn(1)"), 0.0);   // 1 wins
    EXPECT_NEAR(re("mn(2)"), 0.2, 1e-12);
    EXPECT_NEAR(im("mn(2)"), -0.1, 1e-12);                       // 0.2-0.1i wins
}

// Tie on |z| → broken by angle(z): max picks the larger angle, min the smaller.
TEST_P(MaxMinComplexTest, AngleTieBreak) {
    // |1+0i| == |0+1i| == 1; angle(0+1i)=pi/2 > angle(1+0i)=0 → max = 0+1i.
    EXPECT_EQ(re("max(1+0i, 0+1i)"), 0.0);  EXPECT_EQ(im("max(1+0i, 0+1i)"), 1.0);
    EXPECT_EQ(re("min(1+0i, 0+1i)"), 1.0);  EXPECT_EQ(im("min(1+0i, 0+1i)"), 0.0);
    // |1+1i| == |1-1i|; angle(1+1i)=pi/4 > angle(1-1i)=-pi/4 → max = 1+1i.
    EXPECT_EQ(re("max(1+1i, 1-1i)"), 1.0);  EXPECT_EQ(im("max(1+1i, 1-1i)"), 1.0);
    EXPECT_EQ(re("min(1+1i, 1-1i)"), 1.0);  EXPECT_EQ(im("min(1+1i, 1-1i)"), -1.0);
}

// A NaN-component operand is "missing" under the default (omitnan): the other
// operand wins. A single NaN component (real OR imag) is enough.
TEST_P(MaxMinComplexTest, OmitNanDefault) {
    EXPECT_EQ(re("max(complex(NaN,2), 3+4i)"), 3.0);
    EXPECT_EQ(im("max(complex(NaN,2), 3+4i)"), 4.0);
    EXPECT_EQ(re("min(complex(1,NaN), 2+2i)"), 2.0);  // partial-NaN a → omit
    EXPECT_EQ(im("min(complex(1,NaN), 2+2i)"), 2.0);
}

// 'includenan' → a NaN component propagates and wins (first operand first).
TEST_P(MaxMinComplexTest, IncludeNan) {
    e.eval("r = max(complex(NaN,2), 3+4i, 'includenan');");
    EXPECT_TRUE(std::isnan(re("r")));
    EXPECT_EQ(im("r"), 2.0);                                     // NaN+2i (operand a)
}

// No all-real fallback in the binary form: |−3|=3 > 1 → max picks -3 even
// though both have zero imaginary part. (The reduction form differs.)
TEST_P(MaxMinComplexTest, NoAllRealFallback) {
    EXPECT_EQ(re("max(complex(-3,0), 1)"), -3.0);
    EXPECT_EQ(im("max(complex(-3,0), 1)"), 0.0);
    EXPECT_EQ(re("min(complex(-3,0), 1)"), 1.0);                 // |1|<3 → 1
}

// Two complex arrays, elementwise. |1+1i|=√2<|2|=2; |2-2i|=√8>|1+1i|=√2.
TEST_P(MaxMinComplexTest, TwoComplexArrays) {
    e.eval("mx = max([1+1i 2-2i], [2+0i 1+1i]); mn = min([1+1i 2-2i], [2+0i 1+1i]);");
    EXPECT_EQ(re("mx(1)"), 2.0);  EXPECT_EQ(im("mx(1)"), 0.0);
    EXPECT_EQ(re("mx(2)"), 2.0);  EXPECT_EQ(im("mx(2)"), -2.0);
    EXPECT_EQ(re("mn(1)"), 1.0);  EXPECT_EQ(im("mn(1)"), 1.0);
    EXPECT_EQ(re("mn(2)"), 1.0);  EXPECT_EQ(im("mn(2)"), 1.0);
}

// clamp = max(lo, min(hi, z)) composition now works for complex (an in-range
// value passes through; this is the idiom the fusion clamp rule needs).
TEST_P(MaxMinComplexTest, ClampComposition) {
    e.eval("c = max(0, min(1, 0.3+0.4i));");   // |0.3+0.4i|=0.5 in [0,1]
    EXPECT_NEAR(re("c"), 0.3, 1e-12);
    EXPECT_NEAR(im("c"), 0.4, 1e-12);
}

// Mixed-type edges. (a) integer + complex errors — MATLAB errors here too
// ("integers combine only with same-class integers or scalar doubles"); numkit
// can't promote an integer to complex. (b) a real array vs a GENUINELY complex
// scalar compares by |z| (MATLAB R2025b). (c) complex(x,0) (FORCED complex) vs a
// real scalar also uses |z| (max(complex(-3),2) = -3; the all-real result then
// narrows back to real, so re() of each element is what matters). The `2+0i`
// literal case is no longer divergent — arithmetic now narrows it to real (see
// ComplexMathTest.NarrowsArithmeticAllReal), so max([1 -3 2],2+0i) == [2 2 2].
TEST_P(MaxMinComplexTest, MixedTypeEdges) {
    EXPECT_ANY_THROW(e.eval("q = max(complex(0.5,0), int8(1));"));        // (a)
    e.eval("g = max([1 -3 2], 2+1i);");                                   // (b) genuine
    EXPECT_EQ(re("g(1)"), 2.0);  EXPECT_EQ(im("g(1)"), 1.0);   // |2+1i|>|1|
    EXPECT_EQ(re("g(2)"), -3.0); EXPECT_EQ(im("g(2)"), 0.0);   // |-3|>|2+1i|
    EXPECT_EQ(re("g(3)"), 2.0);  EXPECT_EQ(im("g(3)"), 1.0);
    e.eval("h = max(complex([1 -3 2]), 2);");                             // (c) forced
    EXPECT_EQ(re("h(1)"), 2.0);  EXPECT_EQ(re("h(2)"), -3.0);  EXPECT_EQ(re("h(3)"), 2.0);
}

INSTANTIATE_TEST_SUITE_P(Backends, MaxMinComplexTest,
                         ::testing::Values(numkit::Engine::Backend::TreeWalker,
                                           numkit::Engine::Backend::VM));

} // namespace
