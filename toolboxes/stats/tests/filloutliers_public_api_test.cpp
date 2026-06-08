// toolboxes/stats/tests/filloutliers_public_api_test.cpp
//
// Direct C++ API guard for filloutliers after the lift from adapter-only
// to the public typed entry point numkit::stats::filloutliers (typed front
// over filloutliers_of covering median/mean/quartiles find-methods).

#include <numkit/stats/descriptive/descriptive.hpp>
#include <numkit/core/engine.hpp>

#include <limits>
#include <gtest/gtest.h>

using namespace numkit;

namespace {
Value fv(Engine &e, const char *expr, const char *name)
{
    e.eval(std::string(name) + " = " + expr + ";");
    return *e.getVariable(name);
}
} // namespace

TEST(FilloutliersPublicApi, Basic)
{
    StandardEngine e;
    Value A = fv(e, "[1 2 3 4 100]", "A"); // 100 is the lone outlier
    // constant-0 fill, default median detection + NaN (method-default) tf, mr
    Value B = stats::filloutliers(A, fv(e, "0", "f0"));
    ASSERT_EQ(B.numel(), 5u);
    EXPECT_DOUBLE_EQ(B.doubleData()[0], 1.0); // non-outlier unchanged
    EXPECT_DOUBLE_EQ(B.doubleData()[3], 4.0); // non-outlier unchanged
    EXPECT_DOUBLE_EQ(B.doubleData()[4], 0.0); // outlier 100 -> 0
    // quartiles find-method (k = 1.5 default); 100 still flagged
    Value Bq = stats::filloutliers(A, fv(e, "0", "f0b"), "quartiles",
                                   std::numeric_limits<double>::quiet_NaN(),
                                   e.resource());
    EXPECT_DOUBLE_EQ(Bq.doubleData()[4], 0.0);
    EXPECT_DOUBLE_EQ(Bq.doubleData()[0], 1.0);
    // unsupported find-method rejected by the C++ entry
    EXPECT_ANY_THROW(stats::filloutliers(A, fv(e, "0", "f0c"), "grubbs"));
}
