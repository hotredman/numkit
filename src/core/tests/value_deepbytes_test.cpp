// Value::deepBytes() — whos-correct deep sizing. A struct/cell has no flat
// buffer, so rawBytes() is 0; deepBytes() must report the sum of everything
// contained (recursively) plus the struct's own field-name schema. This is
// the regression guard for the Workspace / struct-inspector "Bytes" column,
// which previously showed 0 for any struct.
#include <numkit/core/engine.hpp>
#include <numkit/value/value.hpp>
#include <gtest/gtest.h>

class DeepBytesTest : public ::testing::Test {
public:
    numkit::StandardEngine engine;
    // Trailing bare expression (e.g. `...; s`) makes eval return that value
    // rather than the last assignment's RHS.
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
};

// Leaves are unchanged: a plain array's deepBytes() == rawBytes().
TEST_F(DeepBytesTest, LeavesEqualRawBytes) {
    numkit::Value x = eval("3");
    EXPECT_EQ(x.deepBytes(), x.rawBytes());
    EXPECT_EQ(x.deepBytes(), sizeof(double));        // one double
    EXPECT_EQ(eval("ones(1,10)").deepBytes(), 10u * sizeof(double));
}

// A struct's rawBytes() is 0, but deepBytes() sums every field plus a
// positive own-schema term (the field names).
TEST_F(DeepBytesTest, StructIncludesAllFieldsAndOwnSchema) {
    const size_t aBytes = eval("ones(1,100)").deepBytes();   // 800
    const size_t bBytes = eval("'hello'").deepBytes();       // char field
    numkit::Value s = eval("s.a = ones(1,100); s.b = 'hello'; s");
    EXPECT_EQ(s.rawBytes(), 0u);                  // no flat buffer of its own
    EXPECT_GE(s.deepBytes(), aBytes + bBytes);    // every field is counted
    EXPECT_GT(s.deepBytes(), aBytes + bBytes);    // ... plus own schema (names)
}

// Nested struct + cell recurse through every level (not just the top).
TEST_F(DeepBytesTest, NestedStructAndCellRecurse) {
    numkit::Value c = eval("{ones(1,100), 'hi'}");
    EXPECT_GE(c.deepBytes(), 100u * sizeof(double));
    numkit::Value n = eval("outer.inner.data = ones(1,100); outer");
    EXPECT_GE(n.deepBytes(), 100u * sizeof(double));
}

// A struct array sums the fields of every element, not just element 1.
TEST_F(DeepBytesTest, StructArraySumsAcrossElements) {
    numkit::Value sa = eval(
        "arr(1).v = ones(1,100); arr(2).v = ones(1,100); arr(3).v = ones(1,100); arr");
    EXPECT_GE(sa.deepBytes(), 3u * 100u * sizeof(double));
}
