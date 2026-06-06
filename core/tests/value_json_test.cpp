// core/tests/value_json_test.cpp
//
// Regression tests for integer / single display in the Variable viewer.
//
// Bug: the workspace inspector rendered uint*/int*/single matrices as a
// preview string ("[1x3 uint8]") instead of their actual cell values,
// because the value→JSON serializers (repl_bindings emitMatrixDataArray /
// getVarTileJSON / valuePreview) and computeValueStats only handled
// DOUBLE / LOGICAL / COMPLEX and fell through for every integer class and
// SINGLE.
//
// These tests guard the two natively-testable primitives the fix routes
// through: numericCellJSON (the per-cell JSON token, in value_json.hpp)
// and computeValueStats (value_stats.hpp). The thin JSON-envelope framing
// in the WASM-only repl_bindings methods is verified against the live IDE.

#include <gtest/gtest.h>

#include <numkit/value/value.hpp>
#include <numkit/value/value_type.hpp>
#include <numkit/core/value_stats.hpp>
#include <numkit/core/value_json.hpp>

#include <cstdint>
#include <initializer_list>
#include <limits>

using numkit::Value;
using numkit::ValueType;

namespace {

Value rowU8(std::initializer_list<uint8_t> xs) {
    Value v = Value::matrix(1, xs.size(), ValueType::UINT8);
    uint8_t *p = v.uint8DataMut();
    size_t i = 0;
    for (uint8_t x : xs) p[i++] = x;
    return v;
}

Value rowI32(std::initializer_list<int32_t> xs) {
    Value v = Value::matrix(1, xs.size(), ValueType::INT32);
    int32_t *p = v.int32DataMut();
    size_t i = 0;
    for (int32_t x : xs) p[i++] = x;
    return v;
}

Value rowSingle(std::initializer_list<float> xs) {
    Value v = Value::matrix(1, xs.size(), ValueType::SINGLE);
    float *p = v.singleDataMut();
    size_t i = 0;
    for (float x : xs) p[i++] = x;
    return v;
}

}  // namespace

// ─── computeValueStats over integer / single types ──────────────
//
// Before the fix these returned false (treated as "non-numeric"), so the
// Variable viewer's StatsBar showed nothing for an integer matrix.

TEST(ValueStatsIntegerTest, Uint8MinMaxMean) {
    Value v = rowU8({10, 200, 30});
    numkit::ValueStats s;
    ASSERT_TRUE(numkit::computeValueStats(v, s));
    EXPECT_DOUBLE_EQ(s.min, 10.0);
    EXPECT_DOUBLE_EQ(s.max, 200.0);
    EXPECT_DOUBLE_EQ(s.mean, 80.0);
}

TEST(ValueStatsIntegerTest, Int32Negative) {
    Value v = rowI32({-5, 0, 5});
    numkit::ValueStats s;
    ASSERT_TRUE(numkit::computeValueStats(v, s));
    EXPECT_DOUBLE_EQ(s.min, -5.0);
    EXPECT_DOUBLE_EQ(s.max, 5.0);
    EXPECT_DOUBLE_EQ(s.mean, 0.0);
}

TEST(ValueStatsIntegerTest, SingleMean) {
    Value v = rowSingle({1.0f, 2.0f, 3.0f});
    numkit::ValueStats s;
    ASSERT_TRUE(numkit::computeValueStats(v, s));
    EXPECT_DOUBLE_EQ(s.min, 1.0);
    EXPECT_DOUBLE_EQ(s.max, 3.0);
    EXPECT_DOUBLE_EQ(s.mean, 2.0);
}

// ─── computeValueStatsRange — per-page stats for 3-D / N-D slices ───────
//
// A page p of a 3-D/N-D array is the contiguous block [p*rc, (p+1)*rc); the
// viewer asks for per-slice min/max via this range overload.

TEST(ValueStatsRangeTest, SubRangePerPage) {
    Value v = rowI32({10, 20, 30, 100, 200, 300});  // two "pages" of 3
    numkit::ValueStats s;
    ASSERT_TRUE(numkit::computeValueStatsRange(v, 0, 3, s));
    EXPECT_DOUBLE_EQ(s.min, 10.0);
    EXPECT_DOUBLE_EQ(s.max, 30.0);
    EXPECT_DOUBLE_EQ(s.mean, 20.0);
    ASSERT_TRUE(numkit::computeValueStatsRange(v, 3, 3, s));
    EXPECT_DOUBLE_EQ(s.min, 100.0);
    EXPECT_DOUBLE_EQ(s.max, 300.0);
    EXPECT_DOUBLE_EQ(s.mean, 200.0);
}

TEST(ValueStatsRangeTest, ClampsOverrunAndRejectsEmpty) {
    Value v = rowI32({10, 20, 30, 100, 200, 300});
    numkit::ValueStats s;
    // count overruns the end → clamps to [4, 6)
    ASSERT_TRUE(numkit::computeValueStatsRange(v, 4, 100, s));
    EXPECT_DOUBLE_EQ(s.min, 200.0);
    EXPECT_DOUBLE_EQ(s.max, 300.0);
    // start past the end → empty range → false
    EXPECT_FALSE(numkit::computeValueStatsRange(v, 99, 10, s));
    // whole-array overload equals range [0, numel)
    numkit::ValueStats whole;
    ASSERT_TRUE(numkit::computeValueStats(v, whole));
    EXPECT_DOUBLE_EQ(whole.min, 10.0);
    EXPECT_DOUBLE_EQ(whole.max, 300.0);
}

// ─── numericCellJSON — the per-cell token used to render matrix cells ──
//
// This is the primitive that was missing for integer / single classes:
// the viewer used to emit the preview string instead of these tokens.

TEST(NumericCellJsonTest, Uint8ExactDecimal) {
    Value v = rowU8({0, 200, 255});
    EXPECT_EQ(numkit::numericCellJSON(v, 0), "0");
    EXPECT_EQ(numkit::numericCellJSON(v, 1), "200");
    EXPECT_EQ(numkit::numericCellJSON(v, 2), "255");
}

TEST(NumericCellJsonTest, Int32SignedDecimal) {
    Value v = rowI32({INT32_MIN, -1, INT32_MAX});
    EXPECT_EQ(numkit::numericCellJSON(v, 0), "-2147483648");
    EXPECT_EQ(numkit::numericCellJSON(v, 1), "-1");
    EXPECT_EQ(numkit::numericCellJSON(v, 2), "2147483647");
}

TEST(NumericCellJsonTest, Uint64LargeExact) {
    Value v = Value::matrix(1, 1, ValueType::UINT64);
    v.uint64DataMut()[0] = 18446744073709551615ULL;  // 2^64 - 1
    EXPECT_EQ(numkit::numericCellJSON(v, 0), "18446744073709551615");
}

TEST(NumericCellJsonTest, SingleFiniteAndInteger) {
    Value v = rowSingle({2.5f, 4.0f});
    EXPECT_EQ(numkit::numericCellJSON(v, 0), "2.5");
    EXPECT_EQ(numkit::numericCellJSON(v, 1), "4");
}

TEST(NumericCellJsonTest, DoubleNonFiniteTokens) {
    Value v = Value::matrix(1, 3, ValueType::DOUBLE);
    double *p = v.doubleDataMut();
    p[0] = std::numeric_limits<double>::quiet_NaN();
    p[1] = std::numeric_limits<double>::infinity();
    p[2] = -std::numeric_limits<double>::infinity();
    EXPECT_EQ(numkit::numericCellJSON(v, 0), "null");
    EXPECT_EQ(numkit::numericCellJSON(v, 1), "\"Inf\"");
    EXPECT_EQ(numkit::numericCellJSON(v, 2), "\"-Inf\"");
}

TEST(NumericCellJsonTest, LogicalTrueFalse) {
    Value v = Value::matrix(1, 2, ValueType::LOGICAL);
    v.logicalDataMut()[0] = 1;
    v.logicalDataMut()[1] = 0;
    EXPECT_EQ(numkit::numericCellJSON(v, 0), "true");
    EXPECT_EQ(numkit::numericCellJSON(v, 1), "false");
}

TEST(NumericCellJsonTest, ClassPredicate) {
    EXPECT_TRUE(numkit::isRealNumericCell(ValueType::DOUBLE));
    EXPECT_TRUE(numkit::isRealNumericCell(ValueType::SINGLE));
    EXPECT_TRUE(numkit::isRealNumericCell(ValueType::LOGICAL));
    EXPECT_TRUE(numkit::isRealNumericCell(ValueType::UINT8));
    EXPECT_TRUE(numkit::isRealNumericCell(ValueType::INT64));
    EXPECT_FALSE(numkit::isRealNumericCell(ValueType::COMPLEX));
    EXPECT_FALSE(numkit::isRealNumericCell(ValueType::CHAR));
}
