// libs/wavelet/tests/qmf_wrev_public_api_test.cpp
//
// Direct C++ API guard for libs/wavelet/src/filter/qmf.cpp after the lift
// from adapter-only to numkit::wavelet::{wrev, qmf}. Calls the public
// functions directly; reference values match the script-level
// wrev_test.cpp / qmf_test.cpp guards (MATLAB R2025b).

#include <numkit/wavelet/filter/qmf.hpp>
#include <numkit/core/engine.hpp>

#include <gtest/gtest.h>

using namespace numkit;

namespace {
Value qvar(Engine &e, const char *expr, const char *name)
{
    e.eval(std::string(name) + " = " + expr + ";");
    return *e.getVariable(name);
}
} // namespace

TEST(QmfWrevPublicApi, Wrev)
{
    StdEngine e;
    // row vector reverse (mr defaulted)
    Value y = wavelet::wrev(qvar(e, "[1 2 3 4 5]", "x"));
    ASSERT_EQ(y.numel(), 5u);
    EXPECT_EQ(y.dims().rows(), 1u);
    EXPECT_DOUBLE_EQ(y.doubleData()[0], 5.0);
    EXPECT_DOUBLE_EQ(y.doubleData()[4], 1.0);
    // column preserves shape
    Value yc = wavelet::wrev(qvar(e, "[1;2;3]", "xc"), e.resource());
    EXPECT_EQ(yc.dims().rows(), 3u);
    EXPECT_EQ(yc.dims().cols(), 1u);
    EXPECT_DOUBLE_EQ(yc.doubleData()[0], 3.0);
    // matrix -> per-column reverse (flipud): [1 2;3 4] -> [3 4;1 2]
    Value ym = wavelet::wrev(qvar(e, "[1 2; 3 4]", "m"), e.resource());
    EXPECT_DOUBLE_EQ(ym.doubleData()[0], 3.0); // (1,1)
    EXPECT_DOUBLE_EQ(ym.doubleData()[1], 1.0); // (2,1)
    // complex preserved
    Value ycx = wavelet::wrev(qvar(e, "[1+2i, 3-4i]", "xx"), e.resource());
    ASSERT_TRUE(ycx.isComplex());
    EXPECT_DOUBLE_EQ(ycx.complexData()[0].real(), 3.0);
    EXPECT_DOUBLE_EQ(ycx.complexData()[0].imag(), -4.0);
}

TEST(QmfWrevPublicApi, Qmf)
{
    StdEngine e;
    // default parity p = 0, mr defaulted
    Value y = wavelet::qmf(qvar(e, "[1 2 3 4 5]", "x"));
    ASSERT_EQ(y.numel(), 5u);
    EXPECT_DOUBLE_EQ(y.doubleData()[0], 5.0);
    EXPECT_DOUBLE_EQ(y.doubleData()[1], -4.0);
    EXPECT_DOUBLE_EQ(y.doubleData()[4], 1.0);
    // parity p = 1 negates the whole result
    Value y1 = wavelet::qmf(qvar(e, "[1 2 3 4 5]", "x1"), 1, e.resource());
    EXPECT_DOUBLE_EQ(y1.doubleData()[0], -5.0);
    EXPECT_DOUBLE_EQ(y1.doubleData()[1], 4.0);
    // column preserves shape
    Value yc = wavelet::qmf(qvar(e, "[1;2;3]", "xc"), 0, e.resource());
    EXPECT_EQ(yc.dims().rows(), 3u);
    EXPECT_EQ(yc.dims().cols(), 1u);
    EXPECT_DOUBLE_EQ(yc.doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(yc.doubleData()[1], -2.0);
}
