// toolboxes/io/tests/csv_cpp_api_test.cpp
//
// Engine-free C++ API guard for CSV. csvreadFromString / csvwriteToString are
// pure text↔Value (no Engine, no VFS) — usable by a C++ embedder directly, like
// image::imreadFromBytes. The MATLAB-facing csvread/csvwrite (VFS read/write) are
// covered separately by csv_test.cpp via eval().

#include <numkit/io/text/csv.hpp>
#include <numkit/value/value.hpp>

#include <gtest/gtest.h>

#include <string>

using namespace numkit;

TEST(CsvCppApi, ReadFromStringEngineFree)
{
    // No StandardEngine — pure C++ parse of CSV text.
    Value M = io::csvreadFromString("1,2,3\n4,5,6\n");
    ASSERT_EQ(M.dims().rows(), 2u);
    ASSERT_EQ(M.dims().cols(), 3u);
    EXPECT_DOUBLE_EQ(M(0, 0), 1.0);
    EXPECT_DOUBLE_EQ(M(0, 2), 3.0);
    EXPECT_DOUBLE_EQ(M(1, 2), 6.0);
}

TEST(CsvCppApi, ReadFromStringSkipEngineFree)
{
    // Skip the first row and column (0-based origin), like csvread(file, 1, 1).
    Value M = io::csvreadFromString("0,0,0\n0,5,6\n", /*skipRows=*/1, /*skipCols=*/1);
    ASSERT_EQ(M.dims().rows(), 1u);
    ASSERT_EQ(M.dims().cols(), 2u);
    EXPECT_DOUBLE_EQ(M(0, 0), 5.0);
    EXPECT_DOUBLE_EQ(M(0, 1), 6.0);
}

TEST(CsvCppApi, WriteToStringEngineFree)
{
    // [1 2; 3 4] in column-major storage: {1, 3, 2, 4}.
    Value M = Value::matrix(2, 2, ValueType::DOUBLE);
    double *d = M.doubleDataMut();
    d[0] = 1.0; d[1] = 3.0; d[2] = 2.0; d[3] = 4.0;
    EXPECT_EQ(io::csvwriteToString(M), "1,2\n3,4\n");
}

TEST(CsvCppApi, RoundTripEngineFree)
{
    Value M = Value::matrix(2, 2, ValueType::DOUBLE);
    double *d = M.doubleDataMut();
    d[0] = 1.5; d[1] = 3.5; d[2] = 2.5; d[3] = 4.5;
    Value back = io::csvreadFromString(io::csvwriteToString(M));
    ASSERT_EQ(back.dims().rows(), 2u);
    ASSERT_EQ(back.dims().cols(), 2u);
    EXPECT_DOUBLE_EQ(back(0, 0), 1.5);
    EXPECT_DOUBLE_EQ(back(1, 1), 4.5);
}
