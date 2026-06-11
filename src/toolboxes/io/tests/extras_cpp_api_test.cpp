// toolboxes/io/tests/extras_cpp_api_test.cpp
//
// Engine-free C++ API guard for the text-matrix helpers. readmatrixFromString /
// writematrixToString are pure text↔Value (no Engine, no VFS) — usable by a C++
// embedder directly, like csvreadFromString / image::imreadFromBytes. The
// MATLAB-facing readmatrix/writematrix (VFS read/write) are covered by
// io_extras_test.cpp via eval().

#include <numkit/io/text/extras.hpp>
#include <numkit/value/value.hpp>

#include <gtest/gtest.h>

#include <string>

using namespace numkit;

TEST(ExtrasCppApi, ReadMatrixFromStringEngineFree)
{
    // No StandardEngine — pure C++ parse of delimited text.
    Value M = io::readmatrixFromString("1,2,3\n4,5,6\n");
    ASSERT_EQ(M.dims().rows(), 2u);
    ASSERT_EQ(M.dims().cols(), 3u);
    EXPECT_DOUBLE_EQ(M(0, 0), 1.0);
    EXPECT_DOUBLE_EQ(M(0, 2), 3.0);
    EXPECT_DOUBLE_EQ(M(1, 2), 6.0);
}

TEST(ExtrasCppApi, ReadMatrixSkipsHeaderEngineFree)
{
    // Leading all-non-numeric row is treated as a header and skipped.
    Value M = io::readmatrixFromString("name,x,y\n10,20,30\n");
    ASSERT_EQ(M.dims().rows(), 1u);
    ASSERT_EQ(M.dims().cols(), 3u);
    EXPECT_DOUBLE_EQ(M(0, 0), 10.0);
    EXPECT_DOUBLE_EQ(M(0, 2), 30.0);
}

TEST(ExtrasCppApi, ReadMatrixSemicolonAndTabEngineFree)
{
    // ';' and '\t' are accepted delimiters alongside ','.
    Value M = io::readmatrixFromString("1;2\t3\n4;5\t6\n");
    ASSERT_EQ(M.dims().rows(), 2u);
    ASSERT_EQ(M.dims().cols(), 3u);
    EXPECT_DOUBLE_EQ(M(0, 1), 2.0);
    EXPECT_DOUBLE_EQ(M(1, 0), 4.0);
}

TEST(ExtrasCppApi, WriteMatrixToStringEngineFree)
{
    // [1 2; 3 4] column-major storage: {1, 3, 2, 4}. Integers stay integral.
    Value M = Value::matrix(2, 2, ValueType::DOUBLE);
    double *d = M.doubleDataMut();
    d[0] = 1.0; d[1] = 3.0; d[2] = 2.0; d[3] = 4.0;
    EXPECT_EQ(io::writematrixToString(M), "1,2\n3,4\n");
}

TEST(ExtrasCppApi, WriteMatrixKeepsFractionalEngineFree)
{
    // Non-integral values keep their decimals.
    Value M = Value::matrix(1, 2, ValueType::DOUBLE);
    double *d = M.doubleDataMut();
    d[0] = 1.5; d[1] = 2.25;
    EXPECT_EQ(io::writematrixToString(M), "1.5,2.25\n");
}

TEST(ExtrasCppApi, RoundTripEngineFree)
{
    Value M = Value::matrix(2, 2, ValueType::DOUBLE);
    double *d = M.doubleDataMut();
    d[0] = 1.5; d[1] = 3.5; d[2] = 2.5; d[3] = 4.5;
    Value back = io::readmatrixFromString(io::writematrixToString(M));
    ASSERT_EQ(back.dims().rows(), 2u);
    ASSERT_EQ(back.dims().cols(), 2u);
    EXPECT_DOUBLE_EQ(back(0, 0), 1.5);
    EXPECT_DOUBLE_EQ(back(1, 1), 4.5);
}
