// core/tests/object_model_test.cpp
//
// P1 of the engine object model (OBJECT_MODEL.md): the OBJECT value
// type, the class registry, and the value/handle COW clone rule. These
// are unit tests of the core infrastructure via the C++ API — script-
// level construct / dispatch arrives in P2/P3.

#include <numkit/core/engine.hpp>
#include <numkit/core/object.hpp>
#include <numkit/builtin/library.hpp>
#include <gtest/gtest.h>
#include <memory>

using namespace numkit;

namespace {

// Minimal opaque payload holding one int — stands in for a real class's
// native state (a Map hash, table columns, …).
struct IntBox : NativePayload
{
    int v;
    explicit IntBox(int x) : v(x) {}
    std::shared_ptr<NativePayload> clone() const override
    {
        return std::make_shared<IntBox>(v);
    }
};

int boxVal(const Value &obj)
{
    const ObjectState *st = obj.objectStateConst();
    return static_cast<const IntBox *>(st->native.get())->v;
}
void setBox(Value &obj, int x)
{
    ObjectState *st = obj.objectStateMut(); // detach (COW) → value/handle rule
    static_cast<IntBox *>(st->native.get())->v = x;
}

} // namespace

class ObjectModelTest : public ::testing::Test
{
public:
    Engine engine;

    void SetUp() override
    {
        BuiltinClass valCls;
        valCls.name = "ValBox";
        valCls.isHandle = false;
        engine.registerClass(valCls);

        BuiltinClass hCls;
        hCls.name = "HandleBox";
        hCls.isHandle = true;
        engine.registerClass(hCls);
    }

    Value makeBox(const std::string &cls, int x, bool isHandle)
    {
        auto st = std::make_shared<ObjectState>(engine.resource());
        st->native = std::make_shared<IntBox>(x);
        return Value::object(cls, st, isHandle, engine.resource());
    }
};

TEST_F(ObjectModelTest, RegistryLookup)
{
    EXPECT_NE(engine.findClass("ValBox"), nullptr);
    EXPECT_NE(engine.findClass("HandleBox"), nullptr);
    EXPECT_EQ(engine.findClass("Nope"), nullptr);
    EXPECT_FALSE(engine.findClass("ValBox")->isHandle);
    EXPECT_TRUE(engine.findClass("HandleBox")->isHandle);
}

TEST_F(ObjectModelTest, DuplicateRegistrationThrows)
{
    BuiltinClass dup;
    dup.name = "ValBox";
    EXPECT_THROW(engine.registerClass(dup), std::runtime_error);
}

TEST_F(ObjectModelTest, ClassNameAndType)
{
    Value o = makeBox("ValBox", 7, false);
    EXPECT_TRUE(o.isObject());
    EXPECT_EQ(o.type(), ValueType::OBJECT);
    EXPECT_TRUE(o.isScalar());
    EXPECT_FALSE(o.isEmpty());
    EXPECT_EQ(o.objectClassName(), "ValBox");
    EXPECT_FALSE(o.objectIsHandle());
    EXPECT_EQ(boxVal(o), 7);
    // Non-objects report "" as class name.
    EXPECT_EQ(Value::scalar(3.0, engine.resource()).objectClassName(), "");
}

// Value class: copy is independent — mutating one must not touch the other.
TEST_F(ObjectModelTest, ValueClassCopiesOnMutate)
{
    Value a = makeBox("ValBox", 7, false);
    Value b = a;     // COW-shared heap
    setBox(a, 99);   // detach → deep-copies ObjectState
    EXPECT_EQ(boxVal(a), 99);
    EXPECT_EQ(boxVal(b), 7) << "value-class copy must be independent";
}

// Handle class: copy aliases — mutating one is visible through the other.
TEST_F(ObjectModelTest, HandleClassSharesState)
{
    Value a = makeBox("HandleBox", 7, true);
    Value b = a;     // shares the same ObjectState
    setBox(a, 99);
    EXPECT_EQ(boxVal(a), 99);
    EXPECT_EQ(boxVal(b), 99) << "handle-class copy must share state";
}

// A second independent handle (not a copy) does NOT alias.
TEST_F(ObjectModelTest, DistinctHandlesDoNotAlias)
{
    Value a = makeBox("HandleBox", 1, true);
    Value b = makeBox("HandleBox", 2, true);
    setBox(a, 50);
    EXPECT_EQ(boxVal(a), 50);
    EXPECT_EQ(boxVal(b), 2);
}
