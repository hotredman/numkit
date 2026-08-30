// toolboxes/builtin/tests/gradient_nd_test.cpp
//
// Regression guard for bugs/builtin/gradient-3d.md (FIXED): gradient now
// supports N-D (3-D+) arrays — one gradient per dimension up to nargout,
// central differences interior + one-sided ends, the MATLAB dim-2=x / dim-1=y
// output ordering, single-spacing broadcast, and per-dim spacing. MATLAB
// R2025b reference values.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class GradientNdTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override {}
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Single output of a 3-D array = the dim-2 (x) gradient.
TEST_F(GradientNdTest, SingleOutput3D)
{
    eval("A = reshape(1:8,2,2,2); g = gradient(A);");
    EXPECT_NEAR(evalScalar("g(1,1,1)"), 2.0, 1e-12);   // x-gradient
    // shape preserved
    EXPECT_EQ(static_cast<int>(evalScalar("ndims(g)")), 3);
    EXPECT_NEAR(evalScalar("numel(g)"), 8.0, 1e-12);
    // single output equals gx of the multi-output form
    eval("[gx,~,~] = gradient(A);");
    EXPECT_NEAR(evalScalar("max(abs(g(:)-gx(:)))"), 0.0, 1e-12);
}

// [gx,gy,gz] = gradient(A): out1=dim2(x), out2=dim1(y), out3=dim3(z).
TEST_F(GradientNdTest, ThreeOutputs3D)
{
    eval("A = reshape(1:8,2,2,2); [gx,gy,gz] = gradient(A);");
    EXPECT_NEAR(evalScalar("gx(1,1,1)"), 2.0, 1e-12);   // dim-2
    EXPECT_NEAR(evalScalar("gy(1,1,1)"), 1.0, 1e-12);   // dim-1
    EXPECT_NEAR(evalScalar("gz(1,1,1)"), 4.0, 1e-12);   // dim-3
}

// Central differences interior, one-sided ends, on a 3x3x3 array.
TEST_F(GradientNdTest, CentralAndEnds3D)
{
    eval("B = reshape(1:27,3,3,3); [bx,by,bz] = gradient(B);");
    EXPECT_NEAR(evalScalar("bx(1,2,1)"), 3.0, 1e-12);   // central dim-2: (7-1)/2
    EXPECT_NEAR(evalScalar("bx(1,1,1)"), 3.0, 1e-12);   // one-sided end: 4-1
    EXPECT_NEAR(evalScalar("by(2,1,1)"), 1.0, 1e-12);   // central dim-1: (3-1)/2
    EXPECT_NEAR(evalScalar("bz(1,1,2)"), 9.0, 1e-12);   // central dim-3: (19-1)/2
}

// Per-dimension spacing: gradient(A, hx, hy, hz) scales each output.
TEST_F(GradientNdTest, PerDimSpacing)
{
    eval("A = reshape(1:8,2,2,2); [hx,hy,hz] = gradient(A,2,3,4);");
    EXPECT_NEAR(evalScalar("hx(1,1,1)"), 1.0,            1e-12);  // 2/2
    EXPECT_NEAR(evalScalar("hy(1,1,1)"), 1.0/3.0,        1e-12);  // 1/3
    EXPECT_NEAR(evalScalar("hz(1,1,1)"), 1.0,            1e-12);  // 4/4
}

// Single spacing broadcasts to every dimension.
TEST_F(GradientNdTest, SingleSpacingBroadcast)
{
    eval("A = reshape(1:8,2,2,2); [a,b,c] = gradient(A,2);");
    EXPECT_NEAR(evalScalar("a(1,1,1)"), 1.0, 1e-12);  // 2/2
    EXPECT_NEAR(evalScalar("b(1,1,1)"), 0.5, 1e-12);  // 1/2
    EXPECT_NEAR(evalScalar("c(1,1,1)"), 2.0, 1e-12);  // 4/2
}

// Non-cube 2x3x2 array exercises unequal per-dim strides.
TEST_F(GradientNdTest, NonCube)
{
    eval("C = reshape(1:12,2,3,2); [cx,cy,cz] = gradient(C);");
    EXPECT_NEAR(evalScalar("cx(1,2,1)"), 2.0, 1e-12);   // central dim-2: (5-1)/2
    EXPECT_NEAR(evalScalar("cx(1,3,1)"), 2.0, 1e-12);   // one-sided: 5-3
    EXPECT_NEAR(evalScalar("cy(2,1,1)"), 1.0, 1e-12);   // dim-1 one-sided: 2-1
    EXPECT_NEAR(evalScalar("cz(1,1,2)"), 6.0, 1e-12);   // dim-3 one-sided: 7-1
}

// 4-D array: 4th output along dim-4.
TEST_F(GradientNdTest, FourD)
{
    eval("D = reshape(1:16,2,2,2,2); [d1,d2,d3,d4] = gradient(D);");
    EXPECT_NEAR(evalScalar("d1(1,1,1,1)"), 2.0, 1e-12);  // dim-2 (x)
    EXPECT_NEAR(evalScalar("d4(1,1,1,1)"), 8.0, 1e-12);  // dim-4
}

// Fewer outputs than dimensions is allowed: [fx,fy] = gradient(3-D).
TEST_F(GradientNdTest, FewerOutputsThanDims)
{
    eval("A = reshape(1:8,2,2,2); [fx,fy] = gradient(A);");
    EXPECT_NEAR(evalScalar("fx(1,1,1)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("fy(1,1,1)"), 1.0, 1e-12);
    EXPECT_EQ(static_cast<int>(evalScalar("ndims(fx)")), 3);
}

// Complex 3-D: real and imaginary parts gradiented separately, recombined.
TEST_F(GradientNdTest, Complex3D)
{
    eval("Z = reshape(1:8,2,2,2) + 1i*reshape(8:-1:1,2,2,2); [zx,zy,zz] = gradient(Z);");
    EXPECT_TRUE(eval("~isreal(zx)").toBool());
    EXPECT_NEAR(evalScalar("real(zx(1,1,1))"), 2.0,  1e-12);  // d/dx of real part
    EXPECT_NEAR(evalScalar("imag(zx(1,1,1))"), -2.0, 1e-12);  // d/dx of imag part
}

// Too many outputs for the dimensionality throws.
TEST_F(GradientNdTest, TooManyOutputsThrows)
{
    EXPECT_THROW(eval("A = reshape(1:8,2,2,2); [a,b,c,d] = gradient(A);"),
                 numkit::Error);
}

// 2-D and vector inputs are unaffected by the N-D routing.
TEST_F(GradientNdTest, TwoDUnchanged)
{
    eval("M = [1 2 4; 3 6 8]; [mx,my] = gradient(M);");
    EXPECT_NEAR(evalScalar("mx(1,1)"), 1.0, 1e-12);   // (2-1)/1
    EXPECT_NEAR(evalScalar("mx(1,2)"), 1.5, 1e-12);   // (4-1)/2
    EXPECT_NEAR(evalScalar("my(1,1)"), 2.0, 1e-12);   // (3-1)/1
    eval("v = [1 4 9 16]; gv = gradient(v);");
    EXPECT_NEAR(evalScalar("gv(2)"), 4.0, 1e-12);     // (9-1)/2
}
