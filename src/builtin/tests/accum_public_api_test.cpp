// src/builtin/tests/accum_public_api_test.cpp
//
// Direct C++ API test for numkit::builtin::accumarray and numkit::builtin::AccumReducer.

#include <numkit/builtin/datafun.hpp>
#include <numkit/value/value.hpp>
#include <gtest/gtest.h>

using namespace numkit;
using namespace numkit::builtin;

TEST(AccumPublicApi, BasicSumReduction)
{
    // subs = [1; 2; 1; 2; 3], vals = [10; 20; 30; 40; 50]
    double subsData[] = {1.0, 2.0, 1.0, 2.0, 3.0};
    double valsData[] = {10.0, 20.0, 30.0, 40.0, 50.0};

    Value subs = Value::matrix(5, 1, ValueType::DOUBLE);
    std::copy(subsData, subsData + 5, subs.doubleDataMut());

    Value vals = Value::matrix(5, 1, ValueType::DOUBLE);
    std::copy(valsData, valsData + 5, vals.doubleDataMut());

    size_t sz[] = {3, 1};
    Value res = accumarray(subs, vals, Span<const size_t>(sz, 2), AccumReducer::Sum);

    ASSERT_EQ(res.numel(), 3u);
    EXPECT_DOUBLE_EQ(res.doubleData()[0], 40.0); // 10 + 30
    EXPECT_DOUBLE_EQ(res.doubleData()[1], 60.0); // 20 + 40
    EXPECT_DOUBLE_EQ(res.doubleData()[2], 50.0); // 50
}

TEST(AccumPublicApi, 2DMaxAndMeanReduction)
{
    // subs = [1 1; 2 1; 2 2; 1 1], vals = [10; 20; 30; 5]
    double subsData[] = {1.0, 2.0, 2.0, 1.0,  // col 1: rows
                         1.0, 1.0, 2.0, 1.0}; // col 2: cols
    double valsData[] = {10.0, 20.0, 30.0, 5.0};

    Value subs = Value::matrix(4, 2, ValueType::DOUBLE);
    std::copy(subsData, subsData + 8, subs.doubleDataMut());

    Value vals = Value::matrix(4, 1, ValueType::DOUBLE);
    std::copy(valsData, valsData + 4, vals.doubleDataMut());

    size_t sz[] = {2, 2};
    Value resMax = accumarray(subs, vals, Span<const size_t>(sz, 2), AccumReducer::Max, -1.0);
    ASSERT_EQ(resMax.dims().rows(), 2u);
    ASSERT_EQ(resMax.dims().cols(), 2u);
    EXPECT_DOUBLE_EQ(resMax.doubleData()[0], 10.0); // max(10, 5) at (1,1)
    EXPECT_DOUBLE_EQ(resMax.doubleData()[1], 20.0); // (2,1)
    EXPECT_DOUBLE_EQ(resMax.doubleData()[2], -1.0); // (1,2) untouched -> fillVal
    EXPECT_DOUBLE_EQ(resMax.doubleData()[3], 30.0); // (2,2)

    Value resMean = accumarray(subs, vals, Span<const size_t>(sz, 2), AccumReducer::Mean, 0.0);
    EXPECT_DOUBLE_EQ(resMean.doubleData()[0], 7.5);  // mean(10, 5)
    EXPECT_DOUBLE_EQ(resMean.doubleData()[1], 20.0); // 20
    EXPECT_DOUBLE_EQ(resMean.doubleData()[2], 0.0);  // fillVal
    EXPECT_DOUBLE_EQ(resMean.doubleData()[3], 30.0); // 30
}
