// libs/builtin/tests/geom_public_api_test.cpp
//
// Exercises the PUBLIC C++ entry points of libs/builtin math/geom
// (numkit::builtin::*) directly — no engine dispatch. Guards the lift of
// geom primitives from adapter-only (`*_reg`, script-callable) to typed
// C++ API functions per docs/LIBRARY_API.md. Inputs are built via a
// throwaway Engine; the assertions call the C++ function itself.

#include <numkit/builtin/math/geom/geom.hpp>
#include <numkit/core/engine.hpp>

#include <cmath>
#include <gtest/gtest.h>

using namespace numkit;

namespace {
Value var(Engine &e, const char *expr, const char *name)
{
    e.eval(std::string(name) + " = " + expr + ";");
    return *e.getVariable(name);
}
} // namespace

TEST(GeomPublicApiTest, Polyarea)
{
    Engine e;
    Value x = var(e, "[0 1 1 0]", "x");
    Value y = var(e, "[0 0 1 1]", "y");
    // unit square -> area 1
    EXPECT_DOUBLE_EQ(builtin::polyarea(x, y, e.resource()).toScalar(), 1.0);
    // mr defaults to nullptr -> process default resource
    EXPECT_DOUBLE_EQ(builtin::polyarea(x, y).toScalar(), 1.0);
    // triangle (0,0)-(1,0)-(0,1) -> area 0.5
    Value xt = var(e, "[0 1 0]", "xt");
    Value yt = var(e, "[0 0 1]", "yt");
    EXPECT_DOUBLE_EQ(builtin::polyarea(xt, yt, e.resource()).toScalar(), 0.5);
    // < 3 vertices -> 0
    Value x2 = var(e, "[0 1]", "x2");
    Value y2 = var(e, "[0 1]", "y2");
    EXPECT_DOUBLE_EQ(builtin::polyarea(x2, y2, e.resource()).toScalar(), 0.0);
    // shape mismatch throws
    Value yb = var(e, "[0 0 1]", "yb");
    EXPECT_ANY_THROW(builtin::polyarea(x, yb, e.resource()));
}

TEST(GeomPublicApiTest, Inpolygon)
{
    Engine e;
    Value xv = var(e, "[0 1 1 0]", "xv"); // unit square
    Value yv = var(e, "[0 0 1 1]", "yv");
    Value xq = var(e, "[0.5 2 0.5]", "xq");
    Value yq = var(e, "[0.5 0.5 -1]", "yq");
    Value in = builtin::inpolygon(xq, yq, xv, yv, e.resource());
    ASSERT_EQ(in.numel(), 3u);
    const uint8_t *m = in.logicalData();
    EXPECT_EQ(m[0], 1); // (0.5,0.5) inside
    EXPECT_EQ(m[1], 0); // (2,0.5) outside
    EXPECT_EQ(m[2], 0); // (0.5,-1) outside
    // < 3 polygon vertices -> all false
    Value deg = builtin::inpolygon(xq, yq, var(e, "[0 1]", "xv2"),
                                   var(e, "[0 1]", "yv2"), e.resource());
    EXPECT_EQ(deg.logicalData()[0], 0);
    // query shape mismatch throws
    EXPECT_ANY_THROW(builtin::inpolygon(xq, var(e, "[0 0]", "yqb"), xv, yv, e.resource()));
}

TEST(GeomPublicApiTest, Convhull)
{
    Engine e;
    Value x = var(e, "[0 1 1 0]", "x"); // unit-square corners
    Value y = var(e, "[0 0 1 1]", "y");
    Value K = builtin::convhull(x, y, e.resource());
    EXPECT_EQ(K.numel(), 5u); // 4 hull vertices + wrap
    EXPECT_DOUBLE_EQ(K.doubleData()[0], K.doubleData()[4]); // first == last
    // interior point (index 5) is excluded from the hull
    Value xi = var(e, "[0 1 1 0 0.5]", "xi");
    Value yi = var(e, "[0 0 1 1 0.5]", "yi");
    Value K2 = builtin::convhull(xi, yi, e.resource());
    EXPECT_EQ(K2.numel(), 5u);
    for (std::size_t i = 0; i < K2.numel(); ++i)
        EXPECT_NE(K2.doubleData()[i], 5.0);
    // < 3 points -> [1; 2; 1]
    Value K3 = builtin::convhull(var(e, "[0 1]", "x3"), var(e, "[0 1]", "y3"), e.resource());
    EXPECT_EQ(K3.numel(), 3u);
    // shape mismatch throws
    EXPECT_ANY_THROW(builtin::convhull(x, var(e, "[0 0 1]", "yb2"), e.resource()));
}

TEST(GeomPublicApiTest, Delaunay)
{
    Engine e;
    // 3 points -> exactly one triangle, vertices {1,2,3} in some rotation
    Value T1 = builtin::delaunay(var(e, "[0 1 0]", "xt"), var(e, "[0 0 1]", "yt"),
                                 e.resource());
    EXPECT_EQ(T1.dims().rows(), 1u);
    EXPECT_EQ(T1.dims().cols(), 3u);
    EXPECT_DOUBLE_EQ(T1.doubleData()[0] + T1.doubleData()[1] + T1.doubleData()[2],
                     6.0); // 1 + 2 + 3, any vertex rotation

    // Generic 5-point set (no 4 cocircular): every index in 1..5 and the
    // triangle areas must tile the convex hull exactly.
    Value x = var(e, "[0.1 2.0 3.3 1.2 0.7]", "x");
    Value y = var(e, "[0.2 0.4 2.1 3.0 1.5]", "y");
    Value T = builtin::delaunay(x, y, e.resource());
    EXPECT_EQ(T.dims().cols(), 3u);
    const std::size_t M = T.dims().rows();
    ASSERT_GE(M, 1u);
    const double *xd = x.doubleData();
    const double *yd = y.doubleData();
    const double *td = T.doubleData();
    double triArea = 0.0;
    for (std::size_t i = 0; i < M; ++i) {
        const int a = static_cast<int>(td[0 * M + i]) - 1;
        const int b = static_cast<int>(td[1 * M + i]) - 1;
        const int c = static_cast<int>(td[2 * M + i]) - 1;
        ASSERT_GE(a, 0); ASSERT_LT(a, 5);
        ASSERT_GE(b, 0); ASSERT_LT(b, 5);
        ASSERT_GE(c, 0); ASSERT_LT(c, 5);
        triArea += 0.5 * std::abs((xd[b] - xd[a]) * (yd[c] - yd[a]) -
                                  (xd[c] - xd[a]) * (yd[b] - yd[a]));
    }
    e.eval("kk = convhull(x, y); ha = polyarea(x(kk), y(kk));");
    const double hullArea = e.getVariable("ha")->toScalar();
    EXPECT_NEAR(triArea, hullArea, 1e-9); // triangulation tiles the hull

    // < 3 points -> 0x3
    Value T0 = builtin::delaunay(var(e, "[0 1]", "x2"), var(e, "[0 1]", "y2"),
                                 e.resource());
    EXPECT_EQ(T0.dims().rows(), 0u);
    EXPECT_EQ(T0.dims().cols(), 3u);
    // shape mismatch throws
    EXPECT_ANY_THROW(builtin::delaunay(x, var(e, "[0 0 1]", "yb4"), e.resource()));
}

TEST(GeomPublicApiTest, Griddata)
{
    Engine e;
    // Sample the plane v = 2x + 3y + 1 at the unit-square corners; linear
    // (barycentric) interpolation of a plane is exact in every triangle.
    Value x = var(e, "[0 1 0 1]", "x");
    Value y = var(e, "[0 0 1 1]", "y");
    Value v = var(e, "[1 3 4 6]", "v"); // f at each corner
    Value xq = var(e, "[0.5 0.25 2.0]", "xq");
    Value yq = var(e, "[0.5 0.25 2.0]", "yq");
    Value vq = builtin::griddata(x, y, v, xq, yq, e.resource());
    ASSERT_EQ(vq.numel(), 3u);
    EXPECT_NEAR(vq.doubleData()[0], 3.5, 1e-12);  // f(0.5, 0.5)
    EXPECT_NEAR(vq.doubleData()[1], 2.25, 1e-12); // f(0.25, 0.25)
    EXPECT_TRUE(std::isnan(vq.doubleData()[2]));  // (2, 2) outside the hull
    // result takes the shape of xq
    EXPECT_EQ(vq.dims().rows(), xq.dims().rows());
    EXPECT_EQ(vq.dims().cols(), xq.dims().cols());
    // < 3 samples -> all NaN
    Value vq0 = builtin::griddata(var(e, "[0 1]", "x2"), var(e, "[0 1]", "y2"),
                                  var(e, "[1 2]", "v2"), xq, yq, e.resource());
    EXPECT_TRUE(std::isnan(vq0.doubleData()[0]));
    // x/y/v numel mismatch throws
    EXPECT_ANY_THROW(
        builtin::griddata(x, y, var(e, "[1 2 3]", "vb"), xq, yq, e.resource()));
    // xq/yq numel mismatch throws
    EXPECT_ANY_THROW(
        builtin::griddata(x, y, v, xq, var(e, "[0 0]", "yqb"), e.resource()));
}

TEST(GeomPublicApiTest, Boundary)
{
    Engine e;
    Value x = var(e, "[0 1 1 0 0.5]", "x"); // square corners + interior point
    Value y = var(e, "[0 0 1 1 0.5]", "y");
    // shrink == 0 -> convex hull
    Value b0 = builtin::boundary(x, y, 0.0, e.resource());
    Value k = builtin::convhull(x, y, e.resource());
    EXPECT_EQ(b0.numel(), k.numel());
    // shrink > 0 -> closed polygon (first index repeated at the end)
    Value b = builtin::boundary(x, y, 1.0, e.resource());
    ASSERT_GE(b.numel(), 2u);
    EXPECT_DOUBLE_EQ(b.doubleData()[0], b.doubleData()[b.numel() - 1]);
    // shape mismatch throws
    EXPECT_ANY_THROW(builtin::boundary(x, var(e, "[0 0 1]", "yb3"), 0.5, e.resource()));
}

TEST(GeomPublicApiTest, Histcounts2)
{
    Engine e;
    // 4 points into a 3x2 grid (explicit edges); mr defaulted
    Value x = var(e, "[0.5 1.5 2.5 0.5]", "x");
    Value y = var(e, "[0.5 0.5 1.5 0.5]", "y");
    Value xe = var(e, "[0 1 2 3]", "xe");
    Value ye = var(e, "[0 1 2]", "ye");
    Value N = builtin::histcounts2(x, y, xe, ye);
    EXPECT_EQ(N.dims().rows(), 3u);
    EXPECT_EQ(N.dims().cols(), 2u);
    EXPECT_DOUBLE_EQ(N.doubleData()[0], 2.0); // (1,1): two (0.5,0.5)
    EXPECT_DOUBLE_EQ(N.doubleData()[1], 1.0); // (2,1)
    EXPECT_DOUBLE_EQ(N.doubleData()[5], 1.0); // (3,2)
    double tot = 0.0;
    for (std::size_t i = 0; i < N.numel(); ++i) tot += N.doubleData()[i];
    EXPECT_DOUBLE_EQ(tot, 4.0);
    // out-of-range points are dropped
    Value N2 = builtin::histcounts2(var(e, "[5 0.5]", "x2"),
                                    var(e, "[5 0.5]", "y2"), xe, ye, e.resource());
    double tot2 = 0.0;
    for (std::size_t i = 0; i < N2.numel(); ++i) tot2 += N2.doubleData()[i];
    EXPECT_DOUBLE_EQ(tot2, 1.0); // (5,5) out of range, (0.5,0.5) counted
}
