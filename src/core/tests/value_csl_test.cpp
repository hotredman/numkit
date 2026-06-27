// First-class CSL (comma-separated list) — Value-level foundation tests.
// A CSL is a transient value-list (ValueType::CSL) reusing cell storage; collapseCsl
// unwraps it in a single-value context (1 elem -> the elem; 0/>1 -> throw). These test
// the Value layer directly, not the interpreter (no producer emits a CSL yet).
#include <numkit/value/value.hpp>
#include <gtest/gtest.h>

using numkit::Value;
using numkit::ValueType;

TEST(CslValue, FactoryShapeAndType) {
    Value c = Value::csl(3);
    EXPECT_TRUE(c.isCsl());
    EXPECT_FALSE(c.isCell());                       // distinct from CELL
    EXPECT_EQ(c.type(), ValueType::CSL);
    EXPECT_EQ(c.cslCount(), 3u);
    EXPECT_STREQ(mtypeName(ValueType::CSL), "comma-separated list");
}

TEST(CslValue, CollapseSingleElementYieldsElement) {
    Value c = Value::csl(1);
    c.cslAt(0) = Value::scalar(42.0);
    Value out = numkit::collapseCsl(std::move(c));
    EXPECT_FALSE(out.isCsl());
    EXPECT_DOUBLE_EQ(out.toScalar(), 42.0);
}

TEST(CslValue, CollapseNonCslPassesThrough) {
    Value x = Value::scalar(7.0);
    Value out = numkit::collapseCsl(std::move(x));
    EXPECT_FALSE(out.isCsl());
    EXPECT_DOUBLE_EQ(out.toScalar(), 7.0);
}

TEST(CslValue, CollapseEmptyThrows) {
    Value c = Value::csl(0);
    EXPECT_THROW(numkit::collapseCsl(std::move(c)), std::runtime_error);
}

TEST(CslValue, CollapseMultipleThrows) {
    Value c = Value::csl(2);
    c.cslAt(0) = Value::scalar(1.0);
    c.cslAt(1) = Value::scalar(2.0);
    EXPECT_THROW(numkit::collapseCsl(std::move(c)), std::runtime_error);
}
