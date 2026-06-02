// core/tests/object_model_test.cpp
//
// P1 of the engine object model (OBJECT_MODEL.md): the OBJECT value
// type, the class registry, and the value/handle COW clone rule. These
// are unit tests of the core infrastructure via the C++ API — script-
// level construct / dispatch arrives in P2/P3.

#include <numkit/core/engine.hpp>
#include <numkit/core/object.hpp>
#include <numkit/builtin/containers.hpp>
#include <numkit/builtin/library.hpp>
#include <gtest/gtest.h>
#include <cmath>
#include <memory>
#include <memory_resource>

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

// ============================================================
// P2 + constructor: script-level construct, property get/set, class(),
// value semantics. A "Point" value class stores x/y in ObjectState.props
// via its own hooks. (TreeWalker; VM variant added once P2 lands on VM.)
// ============================================================
class PointObjectTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    Engine engine;

    void SetUp() override
    {
        engine.setBackend(GetParam());

        BuiltinClass pt;
        pt.name = "Point";
        pt.isHandle = false;
        pt.propNames = {"x", "y"};
        pt.construct = [](Span<const Value> args, CallContext &ctx) -> Value {
            auto *mr = ctx.engine->resource();
            auto st = std::make_shared<ObjectState>(mr);
            st->props.emplace("x", args.size() > 0 ? args[0] : Value::scalar(0.0, mr));
            st->props.emplace("y", args.size() > 1 ? args[1] : Value::scalar(0.0, mr));
            return Value::object("Point", st, /*isHandle=*/false, mr);
        };
        pt.propGet = [](const Value &self, const std::string &name, Value &out,
                        CallContext &) -> bool {
            const auto &props = self.objectStateConst()->props;
            auto it = props.find(name);
            if (it == props.end())
                return false;
            out = it->second;
            return true;
        };
        pt.propSet = [](Value &self, const std::string &name, const Value &val,
                        CallContext &) -> bool {
            self.objectStateMut()->props[name] = val;
            return true;
        };
        // mag() → sqrt(x^2 + y^2)
        pt.methods["mag"] = [](Value &self, Span<const Value>, size_t, Span<Value> outs,
                               CallContext &ctx) {
            const auto &props = self.objectStateConst()->props;
            double x = props.at("x").toScalar(), y = props.at("y").toScalar();
            outs[0] = Value::scalar(std::sqrt(x * x + y * y), ctx.engine->resource());
        };
        // scale(f) → a new Point (value-class style: returns a fresh object)
        pt.methods["scale"] = [](Value &self, Span<const Value> args, size_t,
                                 Span<Value> outs, CallContext &ctx) {
            double f = args.size() > 0 ? args[0].toScalar() : 1.0;
            const auto &props = self.objectStateConst()->props;
            auto *mr = ctx.engine->resource();
            auto st = std::make_shared<ObjectState>(mr);
            st->props.emplace("x", Value::scalar(props.at("x").toScalar() * f, mr));
            st->props.emplace("y", Value::scalar(props.at("y").toScalar() * f, mr));
            outs[0] = Value::object("Point", st, /*isHandle=*/false, mr);
        };
        // coords() → [x, y]  (multi-output method; honours nargout)
        pt.methods["coords"] = [](Value &self, Span<const Value>, size_t nargout,
                                  Span<Value> outs, CallContext &ctx) {
            const auto &props = self.objectStateConst()->props;
            auto *mr = ctx.engine->resource();
            outs[0] = Value::scalar(props.at("x").toScalar(), mr);
            if (nargout > 1)
                outs[1] = Value::scalar(props.at("y").toScalar(), mr);
        };
        // subsref: p(1) → x, p(2) → y
        pt.subsref = [](Value &self, Span<const Value> args, size_t, Span<Value> outs,
                        CallContext &ctx) {
            int i = static_cast<int>(args[0].toScalar());
            const auto &props = self.objectStateConst()->props;
            outs[0] = (i == 1) ? props.at("x") : props.at("y");
            (void) ctx;
        };
        // subsasgn: p(1) = v sets x; args = [index, value]; mutates self.
        pt.subsasgn = [](Value &self, Span<const Value> args, size_t, Span<Value>,
                         CallContext &) {
            int i = static_cast<int>(args[0].toScalar());
            self.objectStateMut()->props[i == 1 ? "x" : "y"] = args[1];
        };
        engine.registerClass(std::move(pt));
    }

    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
    std::string evalStr(const std::string &c) { return engine.eval(c).toString(); }
};

TEST_P(PointObjectTest, ConstructAndGet)
{
    EXPECT_DOUBLE_EQ(evalScalar("p = Point(3, 4); p.x"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("p.y"), 4.0);
}

TEST_P(PointObjectTest, ClassName)
{
    engine.eval("p = Point(3, 4);");
    EXPECT_EQ(evalStr("class(p)"), "Point");
}

TEST_P(PointObjectTest, PropertySet)
{
    engine.eval("p = Point(3, 4); p.x = 10;");
    EXPECT_DOUBLE_EQ(evalScalar("p.x"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("p.y"), 4.0);
}

TEST_P(PointObjectTest, ValueSemanticsOnCopy)
{
    engine.eval("p = Point(3, 4); q = p; p.x = 99;");
    EXPECT_DOUBLE_EQ(evalScalar("q.x"), 3.0); // value class — copy is independent
    EXPECT_DOUBLE_EQ(evalScalar("p.x"), 99.0);
}

TEST_P(PointObjectTest, UnknownPropertyThrows)
{
    engine.eval("p = Point(3, 4);");
    EXPECT_THROW(engine.eval("p.z"), std::exception);
}

TEST_P(PointObjectTest, DottedMethodCall)
{
    EXPECT_DOUBLE_EQ(evalScalar("p = Point(3, 4); p.mag()"), 5.0);
}

TEST_P(PointObjectTest, DottedNoArgMethod)
{
    // obj.method with no parens invokes a no-arg method.
    EXPECT_DOUBLE_EQ(evalScalar("p = Point(3, 4); p.mag"), 5.0);
}

TEST_P(PointObjectTest, FunctionFormMethod)
{
    // m(obj) dispatches to the class method.
    EXPECT_DOUBLE_EQ(evalScalar("p = Point(3, 4); mag(p)"), 5.0);
}

TEST_P(PointObjectTest, MethodReturningObject)
{
    engine.eval("p = Point(3, 4); q = p.scale(2);");
    EXPECT_DOUBLE_EQ(evalScalar("q.x"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("q.y"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("p.x"), 3.0); // original unchanged
}

TEST_P(PointObjectTest, MultiOutputMethodFunctionForm)
{
    // [a, b] = coords(p) — function-form dispatch to a multi-output method.
    engine.eval("p = Point(3, 4); [a, b] = coords(p);");
    EXPECT_DOUBLE_EQ(evalScalar("a"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("b"), 4.0);
}

TEST_P(PointObjectTest, MultiOutputMethodFunctionFormSingle)
{
    // Asking for one output of a multi-output method (nargout==1).
    EXPECT_DOUBLE_EQ(evalScalar("p = Point(3, 4); a = coords(p)"), 3.0);
}

TEST_P(PointObjectTest, MultiOutputMethodDotted)
{
    // [a, b] = p.coords() — dotted dispatch to a multi-output method.
    engine.eval("p = Point(3, 4); [a, b] = p.coords();");
    EXPECT_DOUBLE_EQ(evalScalar("a"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("b"), 4.0);
}

TEST_P(PointObjectTest, SubsrefIndexing)
{
    engine.eval("p = Point(3, 4);");
    EXPECT_DOUBLE_EQ(evalScalar("p(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("p(2)"), 4.0);
}

TEST_P(PointObjectTest, SubsasgnIndexing)
{
    engine.eval("p = Point(3, 4); p(1) = 10;");
    EXPECT_DOUBLE_EQ(evalScalar("p.x"), 10.0);
}

TEST_P(PointObjectTest, SubsasgnValueSemantics)
{
    engine.eval("p = Point(3, 4); q = p; p(1) = 99;");
    EXPECT_DOUBLE_EQ(evalScalar("q(1)"), 3.0); // value class — q independent
    EXPECT_DOUBLE_EQ(evalScalar("p(1)"), 99.0);
}

INSTANTIATE_TEST_SUITE_P(Backends, PointObjectTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ============================================================
// Public engine-free C++ API for the container objects
// (numkit::containers::map/dictionary/set/get/...). No Engine — just
// Value + memory_resource, like the rest of libs/builtin.
// ============================================================
namespace c = numkit::containers;

static Value S(const char *s) { return Value::fromString(s, std::pmr::get_default_resource()); }
static Value N(double d) { return Value::scalar(d, std::pmr::get_default_resource()); }

TEST(ContainersCxxApi, MapEngineFree)
{
    auto *mr = std::pmr::get_default_resource();
    Value m = c::map(mr); // no Engine
    c::set(m, S("a"), N(1.0));
    c::set(m, S("b"), N(2.0));
    EXPECT_EQ(c::count(m), 2u);
    EXPECT_EQ(m.objectClassName(), "containers.Map");
    EXPECT_TRUE(m.objectIsHandle());
    EXPECT_DOUBLE_EQ(c::get(m, S("a")).toScalar(), 1.0);
    EXPECT_TRUE(c::isKey(m, S("b")));
    EXPECT_FALSE(c::isKey(m, S("z")));
    c::remove(m, S("a"));
    EXPECT_EQ(c::count(m), 1u);
    EXPECT_FALSE(c::isKey(m, S("a")));
}

TEST(ContainersCxxApi, MapHandleAliasing)
{
    auto *mr = std::pmr::get_default_resource();
    Value m = c::map(mr);
    c::set(m, S("a"), N(1.0));
    Value m2 = m; // handle: alias shares state
    c::set(m2, S("a"), N(99.0));
    EXPECT_DOUBLE_EQ(c::get(m, S("a")).toScalar(), 99.0);
}

TEST(ContainersCxxApi, DictionaryValueSemantics)
{
    auto *mr = std::pmr::get_default_resource();
    Value d = c::dictionary(mr);
    c::set(d, S("x"), N(10.0));
    EXPECT_FALSE(d.objectIsHandle());
    Value d2 = d; // value: copy is independent
    c::set(d2, S("x"), N(99.0));
    EXPECT_DOUBLE_EQ(c::get(d, S("x")).toScalar(), 10.0);
    EXPECT_DOUBLE_EQ(c::get(d2, S("x")).toScalar(), 99.0);
}

// A C++-built object round-trips into the interpreter unchanged.
TEST(ContainersCxxApi, RoundTripIntoEngine)
{
    Engine engine;
    Value m = c::map(engine.resource());
    c::set(m, S("k"), N(7.0));
    engine.setVariable("m", m);
    EXPECT_DOUBLE_EQ(engine.eval("m('k')").toScalar(), 7.0);
    EXPECT_DOUBLE_EQ(engine.eval("m.Count").toScalar(), 1.0);
}
