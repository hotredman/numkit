// core/tests/object_model_test.cpp
//
// P1 of the engine object model (OBJECT_MODEL.md): the OBJECT value
// type, the class registry, and the value/handle COW clone rule. These
// are unit tests of the core infrastructure via the C++ API — script-
// level construct / dispatch arrives in P2/P3.

#include <numkit/core/engine.hpp>
#include <numkit/value/object.hpp>
#include <numkit/builtin/containers.hpp>
#include <numkit/builtin/library.hpp>
#include <gtest/gtest.h>
#include <cmath>
#include <filesystem>
#include <fstream>
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
    StdEngine engine;

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
    StdEngine engine;

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
        // operator+ overload: Point + Point (or Point + scalar, scalar
        // broadcasts to both components). args = {lhs, rhs} in source order.
        pt.ops["plus"] = [](Value &, Span<const Value> args, size_t, Span<Value> outs,
                            CallContext &ctx) {
            auto *mr = ctx.engine->resource();
            auto comp = [](const Value &v, const char *f) -> double {
                return v.isObject() ? v.objectStateConst()->props.at(f).toScalar()
                                    : v.toScalar();
            };
            auto st = std::make_shared<ObjectState>(mr);
            st->props.emplace("x", Value::scalar(comp(args[0], "x") + comp(args[1], "x"), mr));
            st->props.emplace("y", Value::scalar(comp(args[0], "y") + comp(args[1], "y"), mr));
            outs[0] = Value::object("Point", st, /*isHandle=*/false, mr);
        };
        // unary minus overload: -p → Point(-x, -y). self = the operand.
        pt.ops["uminus"] = [](Value &self, Span<const Value>, size_t, Span<Value> outs,
                              CallContext &ctx) {
            auto *mr = ctx.engine->resource();
            const auto &props = self.objectStateConst()->props;
            auto st = std::make_shared<ObjectState>(mr);
            st->props.emplace("x", Value::scalar(-props.at("x").toScalar(), mr));
            st->props.emplace("y", Value::scalar(-props.at("y").toScalar(), mr));
            outs[0] = Value::object("Point", st, /*isHandle=*/false, mr);
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

TEST_P(PointObjectTest, OperatorPlusObjects)
{
    // p + q dispatches to the class `plus` overload.
    engine.eval("p = Point(3, 4); q = Point(1, 2); r = p + q;");
    EXPECT_DOUBLE_EQ(evalScalar("r.x"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("r.y"), 6.0);
}

TEST_P(PointObjectTest, OperatorPlusScalarRight)
{
    engine.eval("p = Point(3, 4); r = p + 1;");
    EXPECT_DOUBLE_EQ(evalScalar("r.x"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("r.y"), 5.0);
}

TEST_P(PointObjectTest, OperatorPlusScalarLeft)
{
    // Dominant-object detection: lhs is numeric, rhs is the object.
    engine.eval("p = Point(3, 4); r = 10 + p;");
    EXPECT_DOUBLE_EQ(evalScalar("r.x"), 13.0);
    EXPECT_DOUBLE_EQ(evalScalar("r.y"), 14.0);
}

TEST_P(PointObjectTest, UnsupportedOperatorThrows)
{
    // No `mtimes` overload → MATLAB-style "Undefined operator" error.
    engine.eval("p = Point(3, 4); q = Point(1, 2);");
    EXPECT_THROW(engine.eval("p * q"), std::exception);
}

TEST_P(PointObjectTest, OperatorUnaryMinus)
{
    engine.eval("p = Point(3, 4); r = -p;");
    EXPECT_DOUBLE_EQ(evalScalar("r.x"), -3.0);
    EXPECT_DOUBLE_EQ(evalScalar("r.y"), -4.0);
}

TEST_P(PointObjectTest, UnsupportedUnaryOperatorThrows)
{
    // No `not` overload → "Undefined operator '~'".
    engine.eval("p = Point(3, 4);");
    EXPECT_THROW(engine.eval("~p"), std::exception);
}

INSTANTIATE_TEST_SUITE_P(Backends, PointObjectTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ============================================================
// Object arrays (OBJECT_MODEL.md): builtin () indexing / indexed-assign
// with grow for a class that does NOT override subsref/subsasgn. Uses a
// plain "Box" value class (and "HBox" handle variant for aliasing).
// ============================================================
class ObjectArrayTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    StdEngine engine;

    static void registerBox(Engine &e, const std::string &name, bool isHandle)
    {
        BuiltinClass b;
        b.name = name;
        b.isHandle = isHandle;
        b.propNames = {"v"};
        b.construct = [name, isHandle](Span<const Value> args, CallContext &ctx) -> Value {
            auto *mr = ctx.engine->resource();
            auto st = std::make_shared<ObjectState>(mr);
            st->props.emplace("v", args.size() > 0 ? args[0] : Value::scalar(0.0, mr));
            return Value::object(name, st, isHandle, mr);
        };
        b.propGet = [](const Value &self, const std::string &n, Value &out,
                       CallContext &) -> bool {
            const auto &p = self.objectStateConst()->props;
            auto it = p.find(n);
            if (it == p.end())
                return false;
            out = it->second;
            return true;
        };
        b.propSet = [](Value &self, const std::string &n, const Value &val,
                       CallContext &) -> bool {
            self.objectStateMut()->props[n] = val;
            return true;
        };
        // No subsref/subsasgn → builtin object-array indexing applies.
        e.registerClass(std::move(b));
    }

    void SetUp() override
    {
        engine.setBackend(GetParam());
        registerBox(engine, "Box", /*isHandle=*/false);
        registerBox(engine, "HBox", /*isHandle=*/true);
    }

    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
    std::string evalStr(const std::string &c) { return engine.eval(c).toString(); }
};

TEST_P(ObjectArrayTest, BuildByIndexedAssign)
{
    engine.eval("a(1) = Box(10); a(2) = Box(20); a(3) = Box(30);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(a)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2).v"), 20.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(3).v"), 30.0);
}

TEST_P(ObjectArrayTest, ReadElementIsScalarObject)
{
    engine.eval("a(1) = Box(10); a(2) = Box(20); b = a(2);");
    EXPECT_EQ(evalStr("class(b)"), "Box");
    EXPECT_DOUBLE_EQ(evalScalar("b.v"), 20.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(b)"), 1.0);
}

TEST_P(ObjectArrayTest, ValueElementReadIsIndependent)
{
    // value class: mutating a read-out element must not touch the array.
    engine.eval("a(1) = Box(10); a(2) = Box(20); b = a(2); b.v = 99;");
    EXPECT_DOUBLE_EQ(evalScalar("a(2).v"), 20.0) << "value-class element read is a copy";
    EXPECT_DOUBLE_EQ(evalScalar("b.v"), 99.0);
}

TEST_P(ObjectArrayTest, GrowWithGapDefaultFills)
{
    // a(3) on an empty workspace → a(1), a(2) default-constructed (v == 0).
    engine.eval("a(3) = Box(30);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(a)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1).v"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(3).v"), 30.0);
}

TEST_P(ObjectArrayTest, OverwriteElement)
{
    engine.eval("a(1) = Box(10); a(2) = Box(20); a(2) = Box(99);");
    EXPECT_DOUBLE_EQ(evalScalar("a(2).v"), 99.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(a)"), 2.0);
}

TEST_P(ObjectArrayTest, MultiIndexSubArray)
{
    engine.eval("a(1)=Box(10); a(2)=Box(20); a(3)=Box(30); b = a([1 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(b)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(1).v"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(2).v"), 30.0);
}

TEST_P(ObjectArrayTest, EndIndex)
{
    engine.eval("a(1)=Box(10); a(2)=Box(20); a(3)=Box(30);");
    EXPECT_DOUBLE_EQ(evalScalar("a(end).v"), 30.0);
}

TEST_P(ObjectArrayTest, HandleArrayElementAliases)
{
    // handle class: element read aliases the stored object — mutation
    // through the alias is visible in the array.
    engine.eval("h(1) = HBox(1); h(2) = HBox(2); g = h(2); g.v = 77;");
    EXPECT_DOUBLE_EQ(evalScalar("h(2).v"), 77.0) << "handle-class element read aliases";
}

TEST_P(ObjectArrayTest, ConcatRow)
{
    engine.eval("a = [Box(1) Box(2) Box(3)];");
    EXPECT_DOUBLE_EQ(evalScalar("numel(a)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1).v"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(3).v"), 3.0);
}

TEST_P(ObjectArrayTest, ConcatColumn)
{
    engine.eval("a = [Box(1); Box(2)];");
    EXPECT_DOUBLE_EQ(evalScalar("numel(a)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2).v"), 2.0);
}

TEST_P(ObjectArrayTest, ConcatOfArrays)
{
    // Concatenating an existing array with a scalar object.
    engine.eval("a(1)=Box(1); a(2)=Box(2); b = [a Box(3)];");
    EXPECT_DOUBLE_EQ(evalScalar("numel(b)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(3).v"), 3.0);
}

TEST_P(ObjectArrayTest, ConcatMixedClassThrows)
{
    EXPECT_THROW(engine.eval("x = [Box(1) HBox(2)];"), std::exception);
}

TEST_P(ObjectArrayTest, TwoDConcatAndIndex)
{
    engine.eval("a = [Box(1) Box(2); Box(3) Box(4)];");
    EXPECT_DOUBLE_EQ(evalScalar("numel(a)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(a,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(a,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1,1).v"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2,1).v"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1,2).v"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2,2).v"), 4.0);
}

TEST_P(ObjectArrayTest, LinearIndexInto2D)
{
    // Column-major linear index into a 2×2: a(3) == a(1,2).
    engine.eval("a = [Box(1) Box(2); Box(3) Box(4)];");
    EXPECT_DOUBLE_EQ(evalScalar("a(3).v"), 2.0);
}

TEST_P(ObjectArrayTest, TwoDColumnSlice)
{
    engine.eval("a = [Box(1) Box(2); Box(3) Box(4)]; c = a(:,2);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(c)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(1).v"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2).v"), 4.0);
}

TEST_P(ObjectArrayTest, TwoDRowSlice)
{
    engine.eval("a = [Box(1) Box(2); Box(3) Box(4)]; r = a(2,:);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(r)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(1).v"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(2).v"), 4.0);
}

TEST_P(ObjectArrayTest, TwoDAssignAndGrow)
{
    engine.eval("a(1,1) = Box(1); a(2,2) = Box(4); a(1,2) = Box(2); a(2,1) = Box(3);");
    EXPECT_DOUBLE_EQ(evalScalar("size(a,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(a,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1,1).v"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2,1).v"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1,2).v"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2,2).v"), 4.0);
}

TEST_P(ObjectArrayTest, TwoDAssignGapDefault)
{
    engine.eval("a(2,3) = Box(9);"); // grow to 2×3; gaps default-constructed
    EXPECT_DOUBLE_EQ(evalScalar("size(a,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(a,2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2,3).v"), 9.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1,1).v"), 0.0);
}

TEST_P(ObjectArrayTest, TwoDOverwritePreservesShape)
{
    engine.eval("a = [Box(1) Box(2); Box(3) Box(4)]; a(1,2) = Box(99);");
    EXPECT_DOUBLE_EQ(evalScalar("size(a,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(a,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(a)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1,2).v"), 99.0);
}

TEST_P(ObjectArrayTest, ThreeDAssignGrowAndIndex)
{
    // True N-D (3 subscripts): grow to 2×2×2, gap default-fill, read back.
    engine.eval("a(1,1,1) = Box(1); a(2,2,2) = Box(8);");
    EXPECT_DOUBLE_EQ(evalScalar("size(a,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(a,3)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(a)"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1,1,1).v"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2,2,2).v"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1,2,1).v"), 0.0); // a gap → default
}

TEST_P(ObjectArrayTest, LinearAssignInto2DPreservesShape)
{
    // In-bounds linear set on a 2×2 keeps the shape (a(3)==a(1,2)).
    engine.eval("a = [Box(1) Box(2); Box(3) Box(4)]; a(3) = Box(77);");
    EXPECT_DOUBLE_EQ(evalScalar("size(a,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1,2).v"), 77.0);
}

TEST_P(ObjectArrayTest, Reshape)
{
    engine.eval("b = reshape([Box(1) Box(2) Box(3) Box(4)], 2, 2);");
    EXPECT_EQ(evalStr("class(b)"), "Box");
    EXPECT_DOUBLE_EQ(evalScalar("b(2,1).v"), 2.0); // column-major preserved
    EXPECT_DOUBLE_EQ(evalScalar("b(1,2).v"), 3.0);
}
TEST_P(ObjectArrayTest, DeleteLinear)
{
    engine.eval("a=[Box(1) Box(2) Box(3)]; a(2)=[];");
    EXPECT_DOUBLE_EQ(evalScalar("numel(a)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1).v"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2).v"), 3.0);
}
TEST_P(ObjectArrayTest, DeleteColumn)
{
    engine.eval("a=[Box(1) Box(2); Box(3) Box(4)]; a(:,2)=[];");
    EXPECT_DOUBLE_EQ(evalScalar("size(a,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(a,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2,1).v"), 3.0);
}
TEST_P(ObjectArrayTest, DeleteRow)
{
    engine.eval("a=[Box(1) Box(2); Box(3) Box(4)]; a(1,:)=[];");
    EXPECT_DOUBLE_EQ(evalScalar("size(a,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1,1).v"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1,2).v"), 4.0);
}
TEST_P(ObjectArrayTest, TransposeVector)
{
    engine.eval("r = [Box(1) Box(2) Box(3)]; c = r.';");
    EXPECT_DOUBLE_EQ(evalScalar("size(c,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(c,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2).v"), 2.0);
}
TEST_P(ObjectArrayTest, TransposeMatrix)
{
    engine.eval("m = [Box(1) Box(2); Box(3) Box(4)]; t = m';");
    EXPECT_DOUBLE_EQ(evalScalar("t(1,2).v"), 3.0); // t(1,2) = m(2,1) = 3
    EXPECT_DOUBLE_EQ(evalScalar("t(2,1).v"), 2.0); // t(2,1) = m(1,2) = 2
}
TEST_P(ObjectArrayTest, Repmat)
{
    engine.eval("e = repmat(Box(5), 2, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(e)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(e,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(e,2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("e(2,3).v"), 5.0);
}
TEST_P(ObjectArrayTest, Fliplr)
{
    engine.eval("c = fliplr([Box(1) Box(2) Box(3)]);");
    EXPECT_DOUBLE_EQ(evalScalar("c(1).v"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(3).v"), 1.0);
}
TEST_P(ObjectArrayTest, Flipud)
{
    engine.eval("d = flipud([Box(1); Box(2); Box(3)]);");
    EXPECT_DOUBLE_EQ(evalScalar("d(1).v"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("d(3).v"), 1.0);
}
TEST_P(ObjectArrayTest, CatDim1And2)
{
    // cat(1,·)/cat(2,·) ride vertcat/horzcat, which handle objects.
    engine.eval("a=[Box(1) Box(2); Box(3) Box(4)]; v=cat(1,a,a); h=cat(2,a,a);");
    EXPECT_DOUBLE_EQ(evalScalar("size(v,1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3,1).v"), 1.0); // second copy's (1,1)
    EXPECT_DOUBLE_EQ(evalScalar("v(4,2).v"), 4.0); // second copy's (2,2)
    EXPECT_DOUBLE_EQ(evalScalar("size(h,2)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("h(1,3).v"), 1.0); // second copy's (1,1)
    EXPECT_DOUBLE_EQ(evalScalar("h(2,4).v"), 4.0); // second copy's (2,2)
}
TEST_P(ObjectArrayTest, Rot90)
{
    // rot90([1 2;3 4]) → [2 4; 1 3].
    engine.eval("a=[Box(1) Box(2); Box(3) Box(4)]; b=rot90(a);");
    EXPECT_DOUBLE_EQ(evalScalar("b(1,1).v"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(2,1).v"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(1,2).v"), 4.0);
}
TEST_P(ObjectArrayTest, Circshift)
{
    // circshift([1 2;3 4],1) shifts rows down → [3 4; 1 2].
    engine.eval("a=[Box(1) Box(2); Box(3) Box(4)]; b=circshift(a,1);");
    EXPECT_DOUBLE_EQ(evalScalar("b(1,1).v"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(2,1).v"), 1.0);
}
TEST_P(ObjectArrayTest, Permute)
{
    // permute([1 2;3 4],[2 1]) = transpose → [1 3; 2 4].
    engine.eval("a=[Box(1) Box(2); Box(3) Box(4)]; b=permute(a,[2 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("b(1,2).v"), 3.0); // b(1,2)=a(2,1)=3
    EXPECT_DOUBLE_EQ(evalScalar("b(2,1).v"), 2.0); // b(2,1)=a(1,2)=2
}
TEST_P(ObjectArrayTest, FlipDim)
{
    // flip(a,2) reverses columns → [2 1; 4 3].
    engine.eval("a=[Box(1) Box(2); Box(3) Box(4)]; b=flip(a,2);");
    EXPECT_DOUBLE_EQ(evalScalar("b(1,1).v"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(1,2).v"), 1.0);
}
TEST_P(ObjectArrayTest, Cat3)
{
    engine.eval("f = cat(3, [Box(1) Box(2)], [Box(3) Box(4)]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(f,3)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("f(1,2,1).v"), 2.0); // page 1, (1,2)
    EXPECT_DOUBLE_EQ(evalScalar("f(1,1,2).v"), 3.0); // page 2, (1,1)
}

TEST_P(ObjectArrayTest, SliceAssignColumn)
{
    engine.eval("a = [Box(1) Box(2); Box(3) Box(4)];"
                " col = [Box(20); Box(40)]; a(:,2) = col;");
    EXPECT_DOUBLE_EQ(evalScalar("a(1,2).v"), 20.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2,2).v"), 40.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1,1).v"), 1.0); // column 1 untouched
}

TEST_P(ObjectArrayTest, RowSliceAssign)
{
    engine.eval("a = [Box(1) Box(2); Box(3) Box(4)]; a(1,:) = [Box(11) Box(22)];");
    EXPECT_DOUBLE_EQ(evalScalar("a(1,1).v"), 11.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1,2).v"), 22.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2,1).v"), 3.0); // row 2 untouched
}

TEST_P(ObjectArrayTest, SliceAssignBroadcastScalar)
{
    engine.eval("a = [Box(1) Box(2); Box(3) Box(4)]; a(:,1) = Box(7);");
    EXPECT_DOUBLE_EQ(evalScalar("a(1,1).v"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2,1).v"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1,2).v"), 2.0); // column 2 untouched
}

TEST_P(ObjectArrayTest, LinearVectorSliceAssign)
{
    engine.eval("a(1)=Box(1); a(2)=Box(2); a(3)=Box(3); a([1 3]) = [Box(10) Box(30)];");
    EXPECT_DOUBLE_EQ(evalScalar("a(1).v"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2).v"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(3).v"), 30.0);
}

TEST_P(ObjectArrayTest, SliceAssignValueSemantics)
{
    // value class: broadcast RHS is deep-copied into each slice element.
    engine.eval("a = [Box(1) Box(2)]; b = Box(5); a(:) = b; b.v = 99;");
    EXPECT_DOUBLE_EQ(evalScalar("a(1).v"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2).v"), 5.0);
}

TEST_P(ObjectArrayTest, SliceAssignCountMismatchThrows)
{
    EXPECT_THROW(
        engine.eval("a = [Box(1) Box(2) Box(3)]; a([1 2]) = [Box(1) Box(2) Box(3)];"),
        std::exception);
}

TEST_P(ObjectArrayTest, PropertyCSL)
{
    // [arr.prop] expands the property over the whole array → a row vector.
    engine.eval("a(1)=Box(10); a(2)=Box(20); a(3)=Box(30); vs = [a.v];");
    EXPECT_DOUBLE_EQ(evalScalar("numel(vs)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("vs(1)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("vs(3)"), 30.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum([a.v])"), 60.0);
}

INSTANTIATE_TEST_SUITE_P(Backends, ObjectArrayTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ============================================================
// User classdef (Phase 1: value class) — parse + adapter to BuiltinClass.
// The whole object model (construct / props / methods / value-semantics)
// rides the existing dispatch; classdef just feeds the registry.
// ============================================================
class ClassdefTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.setBackend(GetParam());
        engine.eval(
            "classdef Pt\n"
            "  properties\n"
            "    x = 0\n"
            "    y = 0\n"
            "  end\n"
            "  methods\n"
            "    function obj = Pt(a, b)\n"
            "      obj.x = a;\n"
            "      obj.y = b;\n"
            "    end\n"
            "    function d = mag(obj)\n"
            "      d = sqrt(obj.x^2 + obj.y^2);\n"
            "    end\n"
            "    function obj = scale(obj, f)\n"
            "      obj.x = obj.x * f;\n"
            "      obj.y = obj.y * f;\n"
            "    end\n"
            "    function [a, b] = coords(obj)\n"
            "      a = obj.x;\n"
            "      b = obj.y;\n"
            "    end\n"
            "  end\n"
            "end\n");
    }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
    std::string evalStr(const std::string &c) { return engine.eval(c).toString(); }
};

TEST_P(ClassdefTest, ConstructAndProps)
{
    EXPECT_DOUBLE_EQ(evalScalar("p = Pt(3, 4); p.x"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("p.y"), 4.0);
}
TEST_P(ClassdefTest, ClassName)
{
    engine.eval("p = Pt(3, 4);");
    EXPECT_EQ(evalStr("class(p)"), "Pt");
}
TEST_P(ClassdefTest, Method)
{
    EXPECT_DOUBLE_EQ(evalScalar("p = Pt(3, 4); p.mag()"), 5.0);
}
TEST_P(ClassdefTest, FunctionFormMethod)
{
    EXPECT_DOUBLE_EQ(evalScalar("p = Pt(3, 4); mag(p)"), 5.0);
}
TEST_P(ClassdefTest, RvalueReceiverMethodCall)
{
    // Method called directly on a constructor result (rvalue receiver).
    EXPECT_DOUBLE_EQ(evalScalar("Pt(3, 4).mag()"), 5.0);
}
TEST_P(ClassdefTest, MethodChaining)
{
    // scale(2) returns a Pt; .mag() then dispatches on that rvalue.
    EXPECT_DOUBLE_EQ(evalScalar("Pt(3, 4).scale(2).mag()"), 10.0);
}
TEST_P(ClassdefTest, RvalueReceiverMultiOutput)
{
    engine.eval("[xx, yy] = Pt(3, 4).coords();"); // multi-output on rvalue receiver
    EXPECT_DOUBLE_EQ(evalScalar("xx"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("yy"), 4.0);
}
TEST_P(ClassdefTest, MethodReturningObjectValueSemantics)
{
    engine.eval("p = Pt(3, 4); q = p.scale(2);");
    EXPECT_DOUBLE_EQ(evalScalar("q.x"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("q.y"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("p.x"), 3.0) << "value class — original unchanged";
}
TEST_P(ClassdefTest, PropertySet)
{
    engine.eval("p = Pt(3, 4); p.x = 10;");
    EXPECT_DOUBLE_EQ(evalScalar("p.x"), 10.0);
}
TEST_P(ClassdefTest, ValueCopyIsIndependent)
{
    engine.eval("p = Pt(3, 4); q = p; p.x = 99;");
    EXPECT_DOUBLE_EQ(evalScalar("q.x"), 3.0) << "value class — copy independent";
    EXPECT_DOUBLE_EQ(evalScalar("p.x"), 99.0);
}
TEST_P(ClassdefTest, Introspection)
{
    engine.eval("p = Pt(3, 4);");
    EXPECT_DOUBLE_EQ(evalScalar("isobject(p)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isobject(5)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(properties(p))"), 2.0);   // x, y
    EXPECT_DOUBLE_EQ(evalScalar("numel(properties('Pt'))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(methods(p))"), 3.0);      // mag, scale, coords
}
TEST_P(ClassdefTest, ErrorOnUnknownProperty)
{
    engine.eval("p = Pt(3, 4);");
    EXPECT_THROW(engine.eval("y = p.zzz;"), std::exception);
}
TEST_P(ClassdefTest, ErrorOnUnknownMethod)
{
    engine.eval("p = Pt(3, 4);");
    EXPECT_THROW(engine.eval("y = p.nosuch();"), std::exception);
}
INSTANTIATE_TEST_SUITE_P(Backends, ClassdefTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ── classdef handle classes (< handle): reference semantics ──
class HandleClassdefTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.setBackend(GetParam());
        engine.eval(
            "classdef Counter < handle\n"
            "  properties\n"
            "    count = 0\n"
            "  end\n"
            "  methods\n"
            "    function obj = Counter(start)\n"
            "      obj.count = start;\n"
            "    end\n"
            "    function increment(obj)\n"
            "      obj.count = obj.count + 1;\n"
            "    end\n"
            "    function r = get(obj)\n"
            "      r = obj.count;\n"
            "    end\n"
            "  end\n"
            "end\n");
    }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

TEST_P(HandleClassdefTest, MutatingMethodNoReturn)
{
    engine.eval("c = Counter(0); c.increment(); c.increment(); c.increment();");
    EXPECT_DOUBLE_EQ(evalScalar("c.count"), 3.0);
}
TEST_P(HandleClassdefTest, ReferenceSemanticsOnCopy)
{
    engine.eval("c = Counter(5); d = c; c.increment();");
    EXPECT_DOUBLE_EQ(evalScalar("c.count"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("d.count"), 6.0) << "handle: d aliases c";
}
TEST_P(HandleClassdefTest, MethodReadsState)
{
    engine.eval("c = Counter(7);");
    EXPECT_DOUBLE_EQ(evalScalar("c.get()"), 7.0);
}
INSTANTIATE_TEST_SUITE_P(Backends, HandleClassdefTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ── classdef inheritance (< Base): props/methods/ctor inherited ──
class InheritanceClassdefTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.setBackend(GetParam());
        engine.eval(
            "classdef Animal\n"
            "  properties\n    legs = 4\n    weight = 10\n  end\n"
            "  methods\n"
            "    function obj = Animal(w)\n      obj.weight = w;\n    end\n"
            "    function s = baseLegs(obj)\n      s = obj.legs;\n    end\n"
            "  end\n"
            "end\n");
        engine.eval(
            "classdef Dog < Animal\n"
            "  properties\n    barks = 1\n  end\n"
            "  methods\n"
            "    function s = legsPlus(obj)\n      s = obj.legs + obj.barks;\n    end\n"
            "  end\n"
            "end\n");
    }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
    std::string evalStr(const std::string &c) { return engine.eval(c).toString(); }
    bool evalBool(const std::string &c) { return engine.eval(c).toBool(); }
};

TEST_P(InheritanceClassdefTest, InheritedPropertyDefaults)
{
    engine.eval("d = Dog(25);");
    EXPECT_DOUBLE_EQ(evalScalar("d.legs"), 4.0);   // inherited default
    EXPECT_DOUBLE_EQ(evalScalar("d.barks"), 1.0);  // own default
}
TEST_P(InheritanceClassdefTest, InheritedConstructor)
{
    // Dog defines no constructor → inherits Animal's (weight = w).
    engine.eval("d = Dog(25);");
    EXPECT_DOUBLE_EQ(evalScalar("d.weight"), 25.0);
    EXPECT_EQ(evalStr("class(d)"), "Dog");
}
TEST_P(InheritanceClassdefTest, InheritedMethod)
{
    engine.eval("d = Dog(25);");
    EXPECT_DOUBLE_EQ(evalScalar("d.baseLegs()"), 4.0); // method from Animal
}
TEST_P(InheritanceClassdefTest, OwnMethodUsesInheritedProp)
{
    engine.eval("d = Dog(25);");
    EXPECT_DOUBLE_EQ(evalScalar("d.legsPlus()"), 5.0); // 4 (legs) + 1 (barks)
}
TEST_P(InheritanceClassdefTest, IsaHierarchy)
{
    engine.eval("d = Dog(25);");
    EXPECT_TRUE(evalBool("isa(d, 'Dog')"));
    EXPECT_TRUE(evalBool("isa(d, 'Animal')")); // ancestor
    EXPECT_FALSE(evalBool("isa(d, 'Cat')"));
    EXPECT_TRUE(evalBool("isa(3.0, 'numeric')")); // builtin category still works
    EXPECT_TRUE(evalBool("isa(3.0, 'double')"));
}
INSTANTIATE_TEST_SUITE_P(Backends, InheritanceClassdefTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ── classdef attributes: Static methods + Constant properties ──
class AttrClassdefTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.setBackend(GetParam());
        engine.eval(
            "classdef MathUtil\n"
            "  properties (Constant)\n"
            "    Answer = 42\n"
            "    Two = 2\n"
            "  end\n"
            "  methods (Static)\n"
            "    function r = square(x)\n      r = x * x;\n    end\n"
            "    function r = add(a, b)\n      r = a + b;\n    end\n"
            "  end\n"
            "end\n");
        engine.eval("classdef MathUtilSub < MathUtil\nend\n"); // inherits Static + Constant
    }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

TEST_P(AttrClassdefTest, ConstantProperty)
{
    EXPECT_DOUBLE_EQ(evalScalar("MathUtil.Answer"), 42.0);
    EXPECT_DOUBLE_EQ(evalScalar("MathUtil.Two"), 2.0);
}
TEST_P(AttrClassdefTest, StaticMethod)
{
    EXPECT_DOUBLE_EQ(evalScalar("MathUtil.square(5)"), 25.0);
    EXPECT_DOUBLE_EQ(evalScalar("MathUtil.add(3, 4)"), 7.0);
}
TEST_P(AttrClassdefTest, StaticMethodComposed)
{
    EXPECT_DOUBLE_EQ(evalScalar("MathUtil.add(MathUtil.square(2), MathUtil.Two)"), 6.0);
}
TEST_P(AttrClassdefTest, InheritedConstant)
{
    EXPECT_DOUBLE_EQ(evalScalar("MathUtilSub.Answer"), 42.0); // inherited Constant
    EXPECT_DOUBLE_EQ(evalScalar("MathUtilSub.Two"), 2.0);
}
TEST_P(AttrClassdefTest, InheritedStaticMethod)
{
    EXPECT_DOUBLE_EQ(evalScalar("MathUtilSub.square(6)"), 36.0); // inherited Static
    EXPECT_DOUBLE_EQ(evalScalar("MathUtilSub.add(3, 4)"), 7.0);
}
INSTANTIATE_TEST_SUITE_P(Backends, AttrClassdefTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ── classdef redefinition: re-running `classdef Name … end` must REPLACE the
//    class wholesale, not silently keep the old one. The previous idempotent
//    early-return in registerClassDef ignored the new body, so rewriting a
//    method/ctor/static/constant had no effect until a full restart — the
//    "I rewrote the method but behaviour didn't change" bug. Stale compiled
//    method chunks (keyed "Name>method") were the VM-side culprit; the
//    qualified `Name.static` / `Name.CONST` externals (which throw on a
//    duplicate registration) were the other half. ──
class ClassdefRedefineTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    StdEngine engine;
    void SetUp() override { engine.setBackend(GetParam()); }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
    void eval(const std::string &c) { engine.eval(c); }
};

TEST_P(ClassdefRedefineTest, MethodBodyRedefinitionTakesEffect)
{
    eval("classdef RDM\n"
         "  properties\n    seed = 0\n  end\n"
         "  methods\n"
         "    function obj = RDM(s)\n      obj.seed = s;\n    end\n"
         "    function r = val(obj)\n      r = obj.seed + 10;\n    end\n"
         "  end\n"
         "end\n");
    EXPECT_DOUBLE_EQ(evalScalar("c = RDM(5); c.val()"), 15.0);
    // Rewrite the method body and re-run the classdef (REPL / IDE re-run).
    eval("classdef RDM\n"
         "  properties\n    seed = 0\n  end\n"
         "  methods\n"
         "    function obj = RDM(s)\n      obj.seed = s;\n    end\n"
         "    function r = val(obj)\n      r = obj.seed + 20;\n    end\n"
         "  end\n"
         "end\n");
    EXPECT_DOUBLE_EQ(evalScalar("c = RDM(5); c.val()"), 25.0)
        << "redefined method body must take effect (was 15 before the fix)";
}

TEST_P(ClassdefRedefineTest, ConstructorBodyRedefinitionTakesEffect)
{
    eval("classdef RDC\n"
         "  properties\n    seed = 0\n  end\n"
         "  methods\n"
         "    function obj = RDC(s)\n      obj.seed = s;\n    end\n"
         "  end\n"
         "end\n");
    EXPECT_DOUBLE_EQ(evalScalar("c = RDC(5); c.seed"), 5.0);
    eval("classdef RDC\n"
         "  properties\n    seed = 0\n  end\n"
         "  methods\n"
         "    function obj = RDC(s)\n      obj.seed = s * 100;\n    end\n"
         "  end\n"
         "end\n");
    EXPECT_DOUBLE_EQ(evalScalar("c = RDC(5); c.seed"), 500.0)
        << "redefined constructor body must take effect";
}

TEST_P(ClassdefRedefineTest, PropertyDefaultRedefinitionTakesEffect)
{
    eval("classdef RDP\n  properties\n    tag = 1\n  end\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("o = RDP; o.tag"), 1.0);
    eval("classdef RDP\n  properties\n    tag = 7\n  end\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("o = RDP; o.tag"), 7.0)
        << "redefined property default must take effect";
}

TEST_P(ClassdefRedefineTest, StaticMethodRedefinitionTakesEffect)
{
    // Exercises the `Name.static` qualified-external eviction: re-registering
    // the same full name throws on duplicate, so without eviction this CRASHES.
    eval("classdef RDS\n  methods (Static)\n"
         "    function r = s()\n      r = 1;\n    end\n  end\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("RDS.s()"), 1.0);
    eval("classdef RDS\n  methods (Static)\n"
         "    function r = s()\n      r = 2;\n    end\n  end\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("RDS.s()"), 2.0)
        << "redefined static method must take effect";
}

TEST_P(ClassdefRedefineTest, ConstantRedefinitionTakesEffect)
{
    // Exercises the `Name.CONST` qualified-external eviction (same throw risk).
    eval("classdef RDK\n  properties (Constant)\n    K = 10\n  end\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("RDK.K"), 10.0);
    eval("classdef RDK\n  properties (Constant)\n    K = 20\n  end\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("RDK.K"), 20.0)
        << "redefined constant must take effect";
}

TEST_P(ClassdefRedefineTest, RedefineDoesNotDisturbOtherClasses)
{
    // Prefix-isolation guard: evicting "RDA>" chunks must NOT touch "RDAB>".
    eval("classdef RDA\n  methods\n    function r = f(obj)\n      r = 1;\n    end\n  end\nend\n");
    eval("classdef RDAB\n  methods\n    function r = f(obj)\n      r = 99;\n    end\n  end\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("a = RDA; a.f()"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("b = RDAB; b.f()"), 99.0);
    eval("classdef RDA\n  methods\n    function r = f(obj)\n      r = 2;\n    end\n  end\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("a = RDA; a.f()"), 2.0) << "RDA redefined";
    EXPECT_DOUBLE_EQ(evalScalar("b = RDAB; b.f()"), 99.0)
        << "RDAB must be untouched by RDA's redefinition";
}

TEST_P(ClassdefRedefineTest, ClearClassesRemovesUserClass)
{
    // `clear classes` must actually remove an inline user class. Before the fix
    // the class survived (classes_/classDefs_ untouched) so it still constructed.
    eval("classdef RDClr\n  properties\n    v = 7\n  end\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("o = RDClr; o.v"), 7.0);
    eval("clear classes");
    EXPECT_THROW(engine.eval("o = RDClr;"), std::exception)
        << "class must be gone after `clear classes` (no .m on path to reload)";
}

TEST_P(ClassdefRedefineTest, ClearAllRemovesUserClass)
{
    eval("classdef RDClrA\n  properties\n    v = 3\n  end\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("o = RDClrA; o.v"), 3.0);
    eval("clear all");
    EXPECT_THROW(engine.eval("o = RDClrA;"), std::exception)
        << "class must be gone after `clear all`";
}

TEST_P(ClassdefRedefineTest, BaseRedefinitionPropagatesToDerived)
{
    // Redefining ONLY the base must propagate to subclasses, which hold a
    // snapshot of the base's methods. Before the fix the derived kept the old
    // base method body.
    eval("classdef RBase\n  properties\n    tag = 0\n  end\n"
         "  methods\n    function r = baseVal(obj)\n      r = 1;\n    end\n  end\nend\n");
    eval("classdef RDer < RBase\n"
         "  methods\n    function b = ownVal(obj)\n      b = 100;\n    end\n  end\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("d = RDer; d.baseVal()"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("d = RDer; d.ownVal()"), 100.0);
    eval("classdef RBase\n  properties\n    tag = 0\n  end\n"
         "  methods\n    function r = baseVal(obj)\n      r = 2;\n    end\n  end\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("d = RDer; d.baseVal()"), 2.0)
        << "derived must see the redefined base method (was 1 before the fix)";
    EXPECT_DOUBLE_EQ(evalScalar("d = RDer; d.ownVal()"), 100.0) << "own method intact";
}

TEST_P(ClassdefRedefineTest, BaseRedefinitionPropagatesProperty)
{
    eval("classdef RBP\n  properties\n    p = 1\n  end\nend\n");
    eval("classdef RDP2 < RBP\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("o = RDP2; o.p"), 1.0);
    eval("classdef RBP\n  properties\n    p = 9\n  end\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("o = RDP2; o.p"), 9.0)
        << "derived must see the redefined base property default";
}

TEST_P(ClassdefRedefineTest, BaseRedefinitionPropagatesTransitively)
{
    eval("classdef RG1\n  properties\n    tag = 0\n  end\n"
         "  methods\n    function r = gv(obj)\n      r = 1;\n    end\n  end\nend\n");
    eval("classdef RG2 < RG1\nend\n");
    eval("classdef RG3 < RG2\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("x = RG3; x.gv()"), 1.0);
    eval("classdef RG1\n  properties\n    tag = 0\n  end\n"
         "  methods\n    function r = gv(obj)\n      r = 3;\n    end\n  end\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("x = RG3; x.gv()"), 3.0)
        << "grandchild must see the redefined grandparent method";
}

TEST_P(ClassdefRedefineTest, ForwardReferencedBaseBackfillsSubclass)
{
    // Define the subclass BEFORE its base. The base's later definition must
    // back-fill the subclass, which recorded the base name but registered
    // without the not-yet-defined base's members.
    eval("classdef FDog < FAnimal\n"
         "  methods\n    function b = bark(obj)\n      b = 7;\n    end\n  end\nend\n");
    eval("classdef FAnimal\n  properties\n    legs = 4\n  end\n"
         "  methods\n    function s = sound(obj)\n      s = 42;\n    end\n  end\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("d = FDog; d.sound()"), 42.0)
        << "subclass must inherit a base defined AFTER it (forward reference)";
    EXPECT_DOUBLE_EQ(evalScalar("d = FDog; d.legs"), 4.0) << "inherited property too";
    EXPECT_DOUBLE_EQ(evalScalar("d = FDog; d.bark()"), 7.0) << "own method intact";
    engine.eval("d = FDog;");
    EXPECT_TRUE(engine.eval("isa(d, 'FAnimal')").toBool()) << "isa reflects the back-filled base";
}

TEST_P(ClassdefRedefineTest, BaseRedefinitionPropagatesStaticAndConstant)
{
    // Inherited Static methods + Constant properties are re-exposed as
    // `Derived.member` qualified externals at merge time. Redefining the base
    // must refresh those on the derived too (cascade re-registers the derived,
    // which drops the old externals and re-registers from the new base).
    eval("classdef RSBase\n"
         "  properties (Constant)\n    K = 10\n  end\n"
         "  methods (Static)\n    function r = s()\n      r = 1;\n    end\n  end\nend\n");
    eval("classdef RSDer < RSBase\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("RSDer.s()"), 1.0); // inherited static
    EXPECT_DOUBLE_EQ(evalScalar("RSDer.K"), 10.0);  // inherited constant
    eval("classdef RSBase\n"
         "  properties (Constant)\n    K = 20\n  end\n"
         "  methods (Static)\n    function r = s()\n      r = 2;\n    end\n  end\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("RSDer.s()"), 2.0)
        << "derived must see the redefined inherited static method";
    EXPECT_DOUBLE_EQ(evalScalar("RSDer.K"), 20.0)
        << "derived must see the redefined inherited constant";
}

TEST_P(ClassdefRedefineTest, FailedCascadeDoesNotWedgePropagation)
{
    // A cascade that THROWS (here: redefining a base as Sealed makes the
    // re-register of its subclass fail with "cannot subclass sealed") must
    // still restore the re-entrancy guard — otherwise all later propagation
    // goes silently dead. Regression guard for the RAII restore in
    // reregisterDerivedClasses.
    eval("classdef SBaseX\n  methods\n    function r = v(obj)\n      r = 1;\n    end\n  end\nend\n");
    eval("classdef SDerX < SBaseX\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("d = SDerX; d.v()"), 1.0);
    EXPECT_THROW(engine.eval("classdef (Sealed) SBaseX\nend\n"), std::exception)
        << "redefining a base as Sealed under a live subclass must error";
    // An unrelated redefinition must STILL propagate to its derived — proving
    // the guard was restored despite the throw above.
    eval("classdef PBaseX\n  methods\n    function r = g(obj)\n      r = 10;\n    end\n  end\nend\n");
    eval("classdef PDerX < PBaseX\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("p = PDerX; p.g()"), 10.0);
    eval("classdef PBaseX\n  methods\n    function r = g(obj)\n      r = 20;\n    end\n  end\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("p = PDerX; p.g()"), 20.0)
        << "propagation must still work after an earlier cascade threw";
}

INSTANTIATE_TEST_SUITE_P(Backends, ClassdefRedefineTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ── classdef get/set property accessors (Dependent) ──
class AccessorClassdefTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.setBackend(GetParam());
        engine.eval(
            "classdef Circle\n"
            "  properties\n    radius = 1\n  end\n"
            "  properties (Dependent)\n    area\n  end\n"
            "  methods\n"
            "    function obj = Circle(r)\n      obj.radius = r;\n    end\n"
            "    function a = get.area(obj)\n      a = 4 * obj.radius * obj.radius;\n    end\n"
            "    function obj = set.area(obj, v)\n      obj.radius = sqrt(v / 4);\n    end\n"
            "  end\n"
            "end\n");
    }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

TEST_P(AccessorClassdefTest, GetAccessorComputes)
{
    engine.eval("c = Circle(3);");
    EXPECT_DOUBLE_EQ(evalScalar("c.area"), 36.0); // 4 * 3 * 3 (getter)
    EXPECT_DOUBLE_EQ(evalScalar("c.radius"), 3.0);
}
TEST_P(AccessorClassdefTest, SetAccessorUpdatesUnderlying)
{
    engine.eval("c = Circle(1); c.area = 64;");
    EXPECT_DOUBLE_EQ(evalScalar("c.radius"), 4.0);  // sqrt(64/4) = 4 (setter)
    EXPECT_DOUBLE_EQ(evalScalar("c.area"), 64.0);   // getter reflects it
}
INSTANTIATE_TEST_SUITE_P(Backends, AccessorClassdefTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ── isprop / ismethod / superclasses (class introspection builtins) ──
class IntrospectClassdefTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.setBackend(GetParam());
        engine.eval(
            "classdef Animal\n"
            "  properties\n    name = 'a'\n  end\n"
            "  methods\n    function s = speak(obj)\n      s = 'generic';\n    end\n  end\n"
            "end\n");
        engine.eval(
            "classdef Dog < Animal\n"
            "  properties\n    breed = 'mutt'\n  end\n"
            "  methods\n    function s = fetch(obj)\n      s = 'ball';\n    end\n  end\n"
            "end\n");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    bool evalBool(const std::string &c) { return engine.eval(c).toScalar() != 0.0; }
};

TEST_P(IntrospectClassdefTest, IspropOwnInheritedAndMissing)
{
    eval("d = Dog;");
    EXPECT_TRUE(evalBool("isprop(d, 'breed')"));  // own property
    EXPECT_TRUE(evalBool("isprop(d, 'name')"));   // inherited from Animal
    EXPECT_FALSE(evalBool("isprop(d, 'nope')"));  // absent
    EXPECT_FALSE(evalBool("isprop(3, 'x')"));     // non-object → false (not an error)
}

TEST_P(IntrospectClassdefTest, IsmethodOwnInheritedAndClassName)
{
    eval("d = Dog;");
    EXPECT_TRUE(evalBool("ismethod(d, 'fetch')")); // own method
    EXPECT_TRUE(evalBool("ismethod(d, 'speak')")); // inherited
    EXPECT_FALSE(evalBool("ismethod(d, 'nope')"));
    EXPECT_TRUE(evalBool("ismethod('Dog', 'speak')")); // class-name form
    EXPECT_FALSE(evalBool("ismethod(d, 'breed')"));     // a property, not a method
}

TEST_P(IntrospectClassdefTest, SuperclassesList)
{
    Value sc = eval("superclasses('Dog')");
    ASSERT_EQ(sc.numel(), 1u);
    EXPECT_EQ(sc.cellAt(0).toString(), "Animal");
    EXPECT_EQ(eval("superclasses('Animal')").numel(), 0u); // no ancestors
}

TEST_P(IntrospectClassdefTest, IspropAndIsmethodAcceptCellstr)
{
    eval("d = Dog;");
    Value r = eval("isprop(d, {'name', 'nope', 'breed'})");
    ASSERT_EQ(r.numel(), 3u);
    EXPECT_NE(r.logicalData()[0], 0); // name
    EXPECT_EQ(r.logicalData()[1], 0); // nope
    EXPECT_NE(r.logicalData()[2], 0); // breed
    Value m = eval("ismethod(d, {'speak', 'fetch', 'nope'})");
    ASSERT_EQ(m.numel(), 3u);
    EXPECT_NE(m.logicalData()[0], 0); // speak
    EXPECT_NE(m.logicalData()[1], 0); // fetch
    EXPECT_EQ(m.logicalData()[2], 0); // nope
}

TEST_P(IntrospectClassdefTest, MetaclassNestedObjects)
{
    eval("d = Dog;");
    eval("mc = metaclass(d);");
    EXPECT_EQ(eval("class(mc)").toString(), "meta.class");
    EXPECT_TRUE(evalBool("isa(mc, 'meta.class')"));
    EXPECT_EQ(eval("mc.Name").toString(), "Dog");
    EXPECT_FALSE(evalBool("mc.Sealed"));
    EXPECT_FALSE(evalBool("mc.Abstract"));

    // SuperclassList is a meta.class array → nested .Name works.
    EXPECT_EQ(eval("class(mc.SuperclassList)").toString(), "meta.class");
    ASSERT_DOUBLE_EQ(eval("numel(mc.SuperclassList)").toScalar(), 1.0);
    EXPECT_EQ(eval("mc.SuperclassList(1).Name").toString(), "Animal");

    // PropertyList is a meta.property array (not cellstr) — the de-crutched form.
    EXPECT_EQ(eval("class(mc.PropertyList)").toString(), "meta.property");
    ASSERT_DOUBLE_EQ(eval("numel(mc.PropertyList)").toScalar(), 2.0); // name + breed
    EXPECT_EQ(eval("mc.PropertyList(1).Name").toString(), "name");    // base prop first
    EXPECT_EQ(eval("mc.PropertyList(1).GetAccess").toString(), "public");
    EXPECT_EQ(eval("mc.PropertyList(1).SetAccess").toString(), "public");
    EXPECT_FALSE(evalBool("mc.PropertyList(1).Constant"));
    EXPECT_FALSE(evalBool("mc.PropertyList(1).Dependent"));

    // MethodList is a meta.method array; methodMeta is sorted (fetch, speak).
    EXPECT_EQ(eval("class(mc.MethodList)").toString(), "meta.method");
    ASSERT_DOUBLE_EQ(eval("numel(mc.MethodList)").toScalar(), 2.0);
    EXPECT_EQ(eval("mc.MethodList(1).Name").toString(), "fetch");
    EXPECT_FALSE(evalBool("mc.MethodList(1).Static"));
    EXPECT_EQ(eval("mc.MethodList(1).Access").toString(), "public");

    // char class-name form (programmatic ?ClassName equivalent).
    EXPECT_EQ(eval("metaclass('Dog').Name").toString(), "Dog");
    // a root class has an empty SuperclassList.
    EXPECT_TRUE(evalBool("isempty(metaclass('Animal').SuperclassList)"));
}

TEST_P(IntrospectClassdefTest, MetaPropertyDependentFlag)
{
    engine.eval(
        "classdef HasDep\n"
        "  properties\n    r = 1\n  end\n"
        "  properties (Dependent)\n    d\n  end\n"
        "  methods\n    function v = get.d(o)\n      v = o.r * 2;\n    end\n  end\n"
        "end\n");
    Value mc = eval("metaclass('HasDep')");
    ASSERT_DOUBLE_EQ(eval("numel(metaclass('HasDep').PropertyList)").toScalar(), 2.0);
    EXPECT_FALSE(evalBool("metaclass('HasDep').PropertyList(1).Dependent")); // r
    EXPECT_EQ(eval("metaclass('HasDep').PropertyList(2).Name").toString(), "d");
    EXPECT_TRUE(evalBool("metaclass('HasDep').PropertyList(2).Dependent"));  // d
}

INSTANTIATE_TEST_SUITE_P(Backends, IntrospectClassdefTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ── classdef superclass calls: obj@Base(args) + method@Base(obj,...) ──
class SuperCallClassdefTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.setBackend(GetParam());
        engine.eval(
            "classdef Shape\n"
            "  properties\n    area = 0\n  end\n"
            "  methods\n"
            "    function obj = Shape(a)\n      obj.area = a;\n    end\n"
            "    function d = describe(obj)\n      d = obj.area;\n    end\n"
            "    function [a, n] = info(obj)\n      a = obj.area;\n      n = 100;\n    end\n"
            "  end\n"
            "end\n");
        engine.eval(
            "classdef Square < Shape\n"
            "  properties\n    side = 1\n  end\n"
            "  methods\n"
            // super-constructor: initialise the Shape part (area = s*s)
            "    function obj = Square(s)\n"
            "      obj = obj@Shape(s*s);\n"
            "      obj.side = s;\n"
            "    end\n"
            // super-method: extend the inherited describe
            "    function d = describe(obj)\n"
            "      base = describe@Shape(obj);\n"
            "      d = base + obj.side;\n"
            "    end\n"
            // multi-output super-method
            "    function [a, n] = info(obj)\n"
            "      [a0, n0] = info@Shape(obj);\n"
            "      a = a0 + obj.side;\n"
            "      n = n0 + 1;\n"
            "    end\n"
            "  end\n"
            "end\n");
    }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

TEST_P(SuperCallClassdefTest, SuperConstructorInitialisesBase)
{
    engine.eval("sq = Square(3);");
    EXPECT_DOUBLE_EQ(evalScalar("sq.area"), 9.0); // obj@Shape(s*s) set area = 9
    EXPECT_DOUBLE_EQ(evalScalar("sq.side"), 3.0); // own prop set after super-ctor
}
TEST_P(SuperCallClassdefTest, SuperMethodCall)
{
    engine.eval("sq = Square(3);");
    // describe@Shape(obj) returns area (9), derived adds side (3) → 12.
    EXPECT_DOUBLE_EQ(evalScalar("sq.describe()"), 12.0);
}
TEST_P(SuperCallClassdefTest, SuperMethodFunctionForm)
{
    engine.eval("sq = Square(4);");
    EXPECT_DOUBLE_EQ(evalScalar("describe(sq)"), 20.0); // area 16 + side 4
}
TEST_P(SuperCallClassdefTest, MultiOutputSuperMethod)
{
    engine.eval("sq = Square(3);");
    engine.eval("[aa, nn] = sq.info();");
    EXPECT_DOUBLE_EQ(evalScalar("aa"), 12.0);  // info@Shape area 9 + side 3
    EXPECT_DOUBLE_EQ(evalScalar("nn"), 101.0); // info@Shape n 100 + 1
}
TEST_P(SuperCallClassdefTest, IsaAfterSuperCtor)
{
    engine.eval("sq = Square(3);");
    EXPECT_TRUE(engine.eval("isa(sq, 'Shape')").toBool());
    EXPECT_TRUE(engine.eval("isa(sq, 'Square')").toBool());
}
INSTANTIATE_TEST_SUITE_P(Backends, SuperCallClassdefTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ── classdef member-access enforcement: private / protected / SetAccess /
// immutable ──
class AccessClassdefTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.setBackend(GetParam());
        engine.eval(
            "classdef Account\n"
            "  properties (Access = private)\n    balance = 0\n  end\n"
            "  properties (SetAccess = private)\n    owner = 0\n  end\n"
            "  properties (SetAccess = immutable)\n    id = 0\n  end\n"
            // GetAccess overrides the general Access=private on the get side.
            "  properties (Access = private, GetAccess = public)\n    note = 2\n  end\n"
            "  properties\n    label = 1\n  end\n"
            "  methods\n"
            "    function obj = Account(theId, theOwner)\n"
            "      obj.id = theId;\n"        // immutable — set in ctor OK
            "      obj.owner = theOwner;\n"  // SetAccess private — OK in ctor
            "      obj.balance = 100;\n"     // private — OK in ctor
            "    end\n"
            "    function b = getBalance(obj)\n      b = obj.balance;\n    end\n" // priv read OK
            "    function obj = deposit(obj, amt)\n"
            "      obj.balance = obj.balance + amt;\n" // priv write inside OK
            "    end\n"
            "    function r = callSecret(obj)\n      r = obj.secret();\n    end\n" // priv method OK
            "  end\n"
            "  methods (Access = private)\n"
            "    function r = secret(obj)\n      r = 42;\n    end\n"
            "  end\n"
            "end\n");
        engine.eval(
            "classdef Base2\n"
            "  properties (Access = protected)\n    p = 5\n  end\n"
            "  methods (Access = protected)\n"
            "    function r = prot(obj)\n      r = obj.p * 2;\n    end\n"
            "  end\n"
            "end\n");
        engine.eval(
            "classdef Deriv2 < Base2\n"
            "  methods\n"
            "    function r = useProt(obj)\n      r = obj.prot() + obj.p;\n    end\n"
            "  end\n"
            "end\n");
        // #1 — Static methods + Constant properties with access.
        engine.eval(
            "classdef Secret\n"
            "  properties (Constant, Access = private)\n    KEY = 99\n  end\n"
            "  properties (Constant)\n    PUB = 7\n  end\n"
            "  methods (Static, Access = private)\n"
            "    function r = priv()\n      r = 5;\n    end\n"
            "  end\n"
            "  methods (Static)\n"
            "    function r = useKey()\n      r = Secret.KEY;\n    end\n"   // priv const, same class
            "    function r = usePriv()\n      r = Secret.priv();\n    end\n" // priv static, same class
            "  end\n"
            "end\n");
        // #2 — operator method with private access.
        engine.eval(
            "classdef PrivOp\n"
            "  properties\n    x = 0\n  end\n"
            "  methods\n"
            "    function obj = PrivOp(v)\n      obj.x = v;\n    end\n"
            "  end\n"
            "  methods (Access = private)\n"
            "    function r = plus(a, b)\n      r = PrivOp(a.x + b.x);\n    end\n"
            "  end\n"
            "end\n");
        // #3 — super-call into protected (OK) vs private (denied) base methods.
        engine.eval(
            "classdef PBase\n"
            "  methods (Access = protected)\n"
            "    function r = protM(obj)\n      r = 22;\n    end\n"
            "  end\n"
            "  methods (Access = private)\n"
            "    function r = secretM(obj)\n      r = 11;\n    end\n"
            "  end\n"
            "end\n");
        engine.eval(
            "classdef PDeriv < PBase\n"
            "  methods\n"
            "    function r = useProtSuper(obj)\n      r = protM@PBase(obj) + 1;\n    end\n"
            "    function r = trySecretSuper(obj)\n      r = secretM@PBase(obj);\n    end\n"
            "  end\n"
            "end\n");
        // #4 — private constructor reachable only via an in-class factory.
        engine.eval(
            "classdef Singleton\n"
            "  properties\n    val = 0\n  end\n"
            "  methods (Access = private)\n"
            "    function obj = Singleton(v)\n      obj.val = v;\n    end\n"
            "  end\n"
            "  methods (Static)\n"
            "    function obj = create(v)\n      obj = Singleton(v);\n    end\n"
            "  end\n"
            "end\n");
        // Inherited private (no-arg) constructor: a subclass without its own
        // ctor honours the base's private ctor.
        engine.eval(
            "classdef PrivCtorBase\n"
            "  properties\n    tag = 0\n  end\n"
            "  methods (Access = private)\n"
            "    function obj = PrivCtorBase()\n      obj.tag = 1;\n    end\n"
            "  end\n"
            "end\n");
        engine.eval("classdef PrivCtorDeriv < PrivCtorBase\nend\n");
    }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

TEST_P(AccessClassdefTest, PrivatePropReadInsideMethod)
{
    engine.eval("a = Account(7, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("a.getBalance()"), 100.0); // private read via method
}
TEST_P(AccessClassdefTest, PrivatePropReadFromOutsideThrows)
{
    engine.eval("a = Account(7, 3);");
    EXPECT_THROW(engine.eval("a.balance;"), std::exception);
}
TEST_P(AccessClassdefTest, PrivatePropWriteInsideMethod)
{
    engine.eval("a = Account(7, 3);");
    engine.eval("a = a.deposit(50);");
    EXPECT_DOUBLE_EQ(evalScalar("a.getBalance()"), 150.0); // private write via method
}
TEST_P(AccessClassdefTest, PrivatePropWriteFromOutsideThrows)
{
    engine.eval("a = Account(7, 3);");
    EXPECT_THROW(engine.eval("a.balance = 5;"), std::exception);
}
TEST_P(AccessClassdefTest, SetAccessPrivateReadOkWriteThrows)
{
    engine.eval("a = Account(7, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("a.owner"), 3.0);          // GetAccess public
    EXPECT_THROW(engine.eval("a.owner = 9;"), std::exception); // SetAccess private
}
TEST_P(AccessClassdefTest, ImmutableReadOkSetOutsideThrows)
{
    engine.eval("a = Account(7, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("a.id"), 7.0);             // set in ctor, read OK
    EXPECT_THROW(engine.eval("a.id = 9;"), std::exception); // immutable outside ctor
}
TEST_P(AccessClassdefTest, PrivateMethodFromOutsideThrows)
{
    engine.eval("a = Account(7, 3);");
    EXPECT_THROW(engine.eval("a.secret();"), std::exception);
}
TEST_P(AccessClassdefTest, PrivateMethodFromSameClassOk)
{
    engine.eval("a = Account(7, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("a.callSecret()"), 42.0); // private method via public method
}
TEST_P(AccessClassdefTest, GetAccessOverridesGeneralAccess)
{
    engine.eval("a = Account(7, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("a.note"), 2.0);          // GetAccess public wins
    EXPECT_THROW(engine.eval("a.note = 9;"), std::exception); // set side still private
}
TEST_P(AccessClassdefTest, PublicPropertyUnaffected)
{
    engine.eval("a = Account(7, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("a.label"), 1.0);
    engine.eval("a.label = 8;");
    EXPECT_DOUBLE_EQ(evalScalar("a.label"), 8.0);
}
TEST_P(AccessClassdefTest, ProtectedVisibleInSubclass)
{
    engine.eval("d = Deriv2();");
    EXPECT_DOUBLE_EQ(evalScalar("d.useProt()"), 15.0); // prot()=10 + p=5
}
TEST_P(AccessClassdefTest, ProtectedPropFromOutsideThrows)
{
    engine.eval("d = Deriv2();");
    EXPECT_THROW(engine.eval("d.p;"), std::exception);
}
TEST_P(AccessClassdefTest, ProtectedMethodFromOutsideThrows)
{
    engine.eval("d = Deriv2();");
    EXPECT_THROW(engine.eval("d.prot();"), std::exception);
}
// #1 — Static method + Constant property access enforcement.
TEST_P(AccessClassdefTest, PublicConstantReadable)
{
    EXPECT_DOUBLE_EQ(evalScalar("Secret.PUB"), 7.0);
}
TEST_P(AccessClassdefTest, PrivateConstantFromOutsideThrows)
{
    EXPECT_THROW(engine.eval("Secret.KEY;"), std::exception);
}
TEST_P(AccessClassdefTest, PrivateConstantReadableInsideClass)
{
    EXPECT_DOUBLE_EQ(evalScalar("Secret.useKey()"), 99.0); // private const via static method
}
TEST_P(AccessClassdefTest, PrivateStaticFromOutsideThrows)
{
    EXPECT_THROW(engine.eval("Secret.priv();"), std::exception);
}
TEST_P(AccessClassdefTest, PrivateStaticCallableInsideClass)
{
    EXPECT_DOUBLE_EQ(evalScalar("Secret.usePriv()"), 5.0); // private static from same class
}
// #2 — operator method with private access.
TEST_P(AccessClassdefTest, PrivateOperatorFromOutsideThrows)
{
    EXPECT_THROW(engine.eval("PrivOp(2) + PrivOp(3);"), std::exception);
}
// #3 — super-call respects base method access.
TEST_P(AccessClassdefTest, ProtectedSuperMethodOk)
{
    engine.eval("d = PDeriv();");
    EXPECT_DOUBLE_EQ(evalScalar("d.useProtSuper()"), 23.0); // protM@PBase 22 + 1
}
TEST_P(AccessClassdefTest, PrivateSuperMethodThrows)
{
    engine.eval("d = PDeriv();");
    EXPECT_THROW(engine.eval("d.trySecretSuper();"), std::exception); // private base method
}
// #4 — private constructor.
TEST_P(AccessClassdefTest, PrivateConstructorFromOutsideThrows)
{
    EXPECT_THROW(engine.eval("Singleton(5);"), std::exception);
}
TEST_P(AccessClassdefTest, PrivateConstructorViaFactory)
{
    engine.eval("s = Singleton.create(5);"); // factory is in-class → ctor allowed
    EXPECT_DOUBLE_EQ(evalScalar("s.val"), 5.0);
}
TEST_P(AccessClassdefTest, InheritedPrivateConstructorThrows)
{
    EXPECT_THROW(engine.eval("PrivCtorDeriv();"), std::exception); // inherits base private ctor
}
INSTANTIATE_TEST_SUITE_P(Backends, AccessClassdefTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ── classdef operator overloading via methods (plus/minus/.../uminus/eq) ──
class OperatorOverloadClassdefTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.setBackend(GetParam());
        engine.eval(
            "classdef Vec\n"
            "  properties\n    x = 0\n  end\n"
            "  methods\n"
            "    function obj = Vec(v)\n      obj.x = v;\n    end\n"
            "    function r = plus(a, b)\n      r = Vec(a.x + b.x);\n    end\n"
            "    function r = minus(a, b)\n      r = Vec(a.x - b.x);\n    end\n"
            "    function r = mtimes(a, b)\n      r = Vec(a.x * b.x);\n    end\n"
            "    function r = uminus(a)\n      r = Vec(-a.x);\n    end\n"
            "    function r = eq(a, b)\n      r = (a.x == b.x);\n    end\n"
            "  end\n"
            "end\n");
    }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
    bool evalBool(const std::string &c) { return engine.eval(c).toBool(); }
};

TEST_P(OperatorOverloadClassdefTest, BinaryPlus)
{
    engine.eval("c = Vec(2) + Vec(3);");
    EXPECT_DOUBLE_EQ(evalScalar("c.x"), 5.0);
}
TEST_P(OperatorOverloadClassdefTest, BinaryMinus)
{
    engine.eval("c = Vec(10) - Vec(4);");
    EXPECT_DOUBLE_EQ(evalScalar("c.x"), 6.0);
}
TEST_P(OperatorOverloadClassdefTest, BinaryMtimes)
{
    engine.eval("c = Vec(3) * Vec(4);");
    EXPECT_DOUBLE_EQ(evalScalar("c.x"), 12.0);
}
TEST_P(OperatorOverloadClassdefTest, UnaryMinus)
{
    engine.eval("c = -Vec(7);");
    EXPECT_DOUBLE_EQ(evalScalar("c.x"), -7.0);
}
TEST_P(OperatorOverloadClassdefTest, EqualityReturnsLogical)
{
    EXPECT_TRUE(evalBool("Vec(5) == Vec(5)"));
    EXPECT_FALSE(evalBool("Vec(5) == Vec(6)"));
}
TEST_P(OperatorOverloadClassdefTest, ChainedLeftAssociative)
{
    engine.eval("c = Vec(1) + Vec(2) + Vec(3);"); // plus(plus(1,2),3)
    EXPECT_DOUBLE_EQ(evalScalar("c.x"), 6.0);
}
TEST_P(OperatorOverloadClassdefTest, MixedWithScalarProperty)
{
    // Operands keep source order: plus(a=Vec(4), b=Vec(scalar via prop)).
    engine.eval("v = Vec(4); c = v + v;");
    EXPECT_DOUBLE_EQ(evalScalar("c.x"), 8.0);
}
TEST_P(OperatorOverloadClassdefTest, OperatorMethodCallableByName)
{
    // An operator method is also a normal method (lives in both methods + ops),
    // callable by name — here on an rvalue receiver.
    engine.eval("c = Vec(2).plus(Vec(3));");
    EXPECT_DOUBLE_EQ(evalScalar("c.x"), 5.0);
}
INSTANTIATE_TEST_SUITE_P(Backends, OperatorOverloadClassdefTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ── classdef custom disp / display ──
class DisplayClassdefTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    StdEngine engine;
    std::string captured;
    void SetUp() override
    {
        engine.setBackend(GetParam());
        engine.setOutputFunc([this](const std::string &s) { captured += s; });
        engine.eval(
            "classdef Temp\n"
            "  properties\n    celsius = 0\n  end\n"
            "  methods\n"
            "    function obj = Temp(c)\n      obj.celsius = c;\n    end\n"
            "    function disp(obj)\n      fprintf('Temp: %g C\\n', obj.celsius);\n    end\n"
            "  end\n"
            "end\n");
        engine.eval(
            "classdef Gauge\n"
            "  properties\n    v = 0\n  end\n"
            "  methods\n"
            "    function obj = Gauge(x)\n      obj.v = x;\n    end\n"
            "    function display(obj)\n      fprintf('<<%g>>\\n', obj.v);\n    end\n"
            "  end\n"
            "end\n");
    }
};

TEST_P(DisplayClassdefTest, CustomDispNoSemicolon)
{
    engine.eval("t = Temp(25);");
    captured.clear();
    engine.eval("t"); // no semicolon → implicit display routes through disp
    EXPECT_NE(captured.find("Temp: 25 C"), std::string::npos) << captured;
    EXPECT_NE(captured.find("="), std::string::npos) << captured; // default name header wraps disp
}
TEST_P(DisplayClassdefTest, CustomDisplayOwnsOutput)
{
    engine.eval("g = Gauge(7);");
    captured.clear();
    engine.eval("g"); // display() owns the whole output — no default header
    EXPECT_NE(captured.find("<<7>>"), std::string::npos) << captured;
    EXPECT_EQ(captured.find("="), std::string::npos) << captured;
}
TEST_P(DisplayClassdefTest, DispBuiltinUsesMethod)
{
    engine.eval("t = Temp(25);");
    captured.clear();
    engine.eval("disp(t);"); // disp(obj) dispatches to the class method
    EXPECT_NE(captured.find("Temp: 25 C"), std::string::npos) << captured;
}
INSTANTIATE_TEST_SUITE_P(Backends, DisplayClassdefTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ── classdef custom indexing via subsref / subsasgn ──
class SubsrefClassdefTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.setBackend(GetParam());
        // A ring buffer: obj(i) wraps around its data via a custom subsref,
        // and obj(i) = v writes with the same wrap via subsasgn.
        engine.eval(
            "classdef Ring\n"
            "  properties\n    data = [10 20 30]\n  end\n"
            "  methods\n"
            "    function obj = Ring(d)\n      obj.data = d;\n    end\n"
            "    function r = subsref(obj, s)\n"
            "      idx = s.subs{1};\n"
            "      n = numel(obj.data);\n"
            "      r = obj.data(mod(idx - 1, n) + 1);\n"
            "    end\n"
            "    function obj = subsasgn(obj, s, val)\n"
            "      idx = s.subs{1};\n"
            "      n = numel(obj.data);\n"
            "      d = obj.data;\n"
            "      d(mod(idx - 1, n) + 1) = val;\n"
            "      obj.data = d;\n"
            "    end\n"
            "  end\n"
            "end\n");
    }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

TEST_P(SubsrefClassdefTest, CustomSubsrefInRange)
{
    engine.eval("r = Ring([10 20 30]);");
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 20.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(3)"), 30.0);
}
TEST_P(SubsrefClassdefTest, CustomSubsrefWrapsAround)
{
    engine.eval("r = Ring([10 20 30]);");
    EXPECT_DOUBLE_EQ(evalScalar("r(4)"), 10.0); // wraps to index 1
    EXPECT_DOUBLE_EQ(evalScalar("r(6)"), 30.0); // wraps to index 3
}
TEST_P(SubsrefClassdefTest, CustomSubsasgnWrapsAround)
{
    engine.eval("r = Ring([10 20 30]);");
    engine.eval("r(5) = 99;"); // index 5 wraps to 2
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 99.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 10.0); // others unchanged
}
INSTANTIATE_TEST_SUITE_P(Backends, SubsrefClassdefTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ── element assignment into an object property: obj.prop(i) = v ──
class PropElemAssignClassdefTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.setBackend(GetParam());
        engine.eval(
            "classdef Vault\n"
            "  properties\n    items = [1 2 3]\n  end\n"
            "  methods\n"
            "    function obj = Vault(d)\n      obj.items = d;\n    end\n"
            "    function obj = setItem(obj, k, v)\n      obj.items(k) = v;\n    end\n"
            "  end\n"
            "end\n");
    }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

TEST_P(PropElemAssignClassdefTest, ElementAssignInsideMethod)
{
    engine.eval("v = Vault([1 2 3]); v = v.setItem(2, 99);");
    EXPECT_DOUBLE_EQ(evalScalar("v.items(2)"), 99.0);
    EXPECT_DOUBLE_EQ(evalScalar("v.items(1)"), 1.0); // neighbours intact
}
TEST_P(PropElemAssignClassdefTest, ElementAssignAtScriptScope)
{
    engine.eval("v = Vault([1 2 3]); v.items(2) = 88;");
    EXPECT_DOUBLE_EQ(evalScalar("v.items(2)"), 88.0);
}
TEST_P(PropElemAssignClassdefTest, ParamNamedIShadowsImaginaryUnit)
{
    // A parameter named `i`/`j` must shadow the imaginary unit inside a VM
    // function frame (regression: the prologue pre-loaded the imaginary
    // constant over the argument, so the param read as complex).
    engine.eval("function r = useI(i)\n  r = i + 1;\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("useI(5)"), 6.0);
    engine.eval("function r = useJ(j)\n  r = j * 2;\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("useJ(4)"), 8.0);
    // Original repro: element-assign indexed by a param named `i`.
    engine.eval("v = Vault([1 2 3]);");
    engine.eval("function r = seti(o, i, val)\n  o.items(i) = val;\n  r = o;\nend\n"
                "v = seti(v, 3, 88);\n");
    EXPECT_DOUBLE_EQ(evalScalar("v.items(3)"), 88.0);
}
TEST_P(PropElemAssignClassdefTest, ElementAssignLiteralInPlainFunction)
{
    engine.eval("v = Vault([1 2 3]);");
    engine.eval("function r = setlit(o)\n  o.items(2) = 99;\n  r = o;\nend\n"
                "v = setlit(v);\n");
    EXPECT_DOUBLE_EQ(evalScalar("v.items(2)"), 99.0); // literal idx+val in a function
}
TEST_P(PropElemAssignClassdefTest, ReadPropElemInPlainFunction)
{
    engine.eval("v = Vault([10 20 30]);");
    engine.eval("function r = getit3(o)\n  r = o.items(2);\nend\n");
    EXPECT_DOUBLE_EQ(evalScalar("getit3(v)"), 20.0); // read o.items(2) in a function
}
TEST_P(PropElemAssignClassdefTest, ElementAssignInsidePlainFunction)
{
    // `o.prop(i) = v` inside a plain (VM-framed) function with an object
    // parameter — same compound assign as a method body, exercised on a VM
    // call frame regardless of classdef dispatch.
    engine.eval("v = Vault([1 2 3]);");
    engine.eval("function r = setit3(o, k, val)\n  o.items(k) = val;\n  r = o;\nend\n"
                "v = setit3(v, 2, 77);\n");
    EXPECT_DOUBLE_EQ(evalScalar("v.items(2)"), 77.0);
}
TEST_P(PropElemAssignClassdefTest, ElementAssignValueSemantics)
{
    engine.eval("a = Vault([1 2 3]); b = a; a.items(1) = 77;");
    EXPECT_DOUBLE_EQ(evalScalar("a.items(1)"), 77.0);
    EXPECT_DOUBLE_EQ(evalScalar("b.items(1)"), 1.0); // value class — copy independent
}
INSTANTIATE_TEST_SUITE_P(Backends, PropElemAssignClassdefTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ── classdef enumeration classes ──
class EnumClassdefTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.setBackend(GetParam());
        engine.eval(
            "classdef Color\n"
            "  enumeration\n    Red, Green, Blue\n  end\n"
            "end\n");
        // Enumeration with an underlying value via the constructor.
        engine.eval(
            "classdef Weekday\n"
            "  properties\n    num = 0\n  end\n"
            "  methods\n    function obj = Weekday(n)\n      obj.num = n;\n    end\n  end\n"
            "  enumeration\n    Monday(1), Tuesday(2), Friday(5)\n  end\n"
            "end\n");
    }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
    std::string evalStr(const std::string &c) { return engine.eval(c).toString(); }
    bool evalBool(const std::string &c) { return engine.eval(c).toBool(); }
};

TEST_P(EnumClassdefTest, MemberIsInstanceOfClass)
{
    EXPECT_EQ(evalStr("class(Color.Red)"), "Color");
    EXPECT_TRUE(evalBool("isobject(Color.Red)"));
}
TEST_P(EnumClassdefTest, MembersCompareByName)
{
    EXPECT_TRUE(evalBool("Color.Red == Color.Red"));
    EXPECT_FALSE(evalBool("Color.Red == Color.Green"));
    EXPECT_TRUE(evalBool("Color.Blue ~= Color.Red"));
}
TEST_P(EnumClassdefTest, UnderlyingValueFromConstructor)
{
    // Chained field access on the member instance (both engines).
    EXPECT_DOUBLE_EQ(evalScalar("Weekday.Monday.num"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("Weekday.Friday.num"), 5.0);
    // Two-step also works.
    engine.eval("m = Weekday.Tuesday;");
    EXPECT_DOUBLE_EQ(evalScalar("m.num"), 2.0);
}
TEST_P(EnumClassdefTest, ValuedMembersCompareByName)
{
    EXPECT_TRUE(evalBool("Weekday.Tuesday == Weekday.Tuesday"));
    EXPECT_FALSE(evalBool("Weekday.Monday == Weekday.Friday"));
}
INSTANTIATE_TEST_SUITE_P(Backends, EnumClassdefTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ── classdef abstract classes / methods ──
class AbstractClassdefTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.setBackend(GetParam());
        engine.eval(
            "classdef Shape2\n"
            "  methods (Abstract)\n"
            "    a = area(obj)\n" // signature only — no body
            "  end\n"
            "  methods\n"
            "    function d = describe(obj)\n      d = area(obj) * 2;\n    end\n"
            "  end\n"
            "end\n");
        engine.eval(
            "classdef Circle2 < Shape2\n"
            "  properties\n    r = 1\n  end\n"
            "  methods\n"
            "    function obj = Circle2(rr)\n      obj.r = rr;\n    end\n"
            "    function a = area(obj)\n      a = obj.r * obj.r;\n    end\n" // implements abstract
            "  end\n"
            "end\n");
        engine.eval("classdef Square2 < Shape2\nend\n"); // does NOT implement area
    }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
    bool evalBool(const std::string &c) { return engine.eval(c).toBool(); }
};

TEST_P(AbstractClassdefTest, AbstractClassCannotInstantiate)
{
    EXPECT_THROW(engine.eval("Shape2();"), std::exception);
}
TEST_P(AbstractClassdefTest, SubclassWithoutImplStaysAbstract)
{
    EXPECT_THROW(engine.eval("Square2();"), std::exception);
}
TEST_P(AbstractClassdefTest, ConcreteSubclassInstantiates)
{
    engine.eval("c = Circle2(3);");
    EXPECT_DOUBLE_EQ(evalScalar("c.area()"), 9.0); // implemented abstract method
}
TEST_P(AbstractClassdefTest, InheritedMethodCallsAbstractImpl)
{
    engine.eval("c = Circle2(3);");
    EXPECT_DOUBLE_EQ(evalScalar("c.describe()"), 18.0); // describe = area*2 = 9*2
}
TEST_P(AbstractClassdefTest, IsaThroughAbstractBase)
{
    engine.eval("c = Circle2(3);");
    EXPECT_TRUE(evalBool("isa(c, 'Shape2')"));
}
INSTANTIATE_TEST_SUITE_P(Backends, AbstractClassdefTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ── classdef Sealed (no subclassing) + Hidden (omitted from introspection) ──
class SealedHiddenClassdefTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.setBackend(GetParam());
        engine.eval("classdef (Sealed) Locked\n  properties\n    v = 1\n  end\nend\n");
        engine.eval(
            "classdef Widget\n"
            "  properties\n    name = 1\n  end\n"
            "  properties (Hidden)\n    secret = 99\n  end\n"
            "  methods\n    function r = pub(obj)\n      r = 1;\n    end\n  end\n"
            "  methods (Hidden)\n    function r = internal(obj)\n      r = 2;\n    end\n  end\n"
            "end\n");
    }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

TEST_P(SealedHiddenClassdefTest, SealedRejectsSubclassing)
{
    EXPECT_THROW(engine.eval("classdef SubLocked < Locked\nend\n"), std::exception);
}
TEST_P(SealedHiddenClassdefTest, HiddenPropertyOmittedButUsable)
{
    EXPECT_DOUBLE_EQ(evalScalar("numel(properties('Widget'))"), 1.0); // only `name`
    engine.eval("w = Widget();");
    EXPECT_DOUBLE_EQ(evalScalar("w.secret"), 99.0); // still readable
}
TEST_P(SealedHiddenClassdefTest, HiddenMethodOmittedButCallable)
{
    EXPECT_DOUBLE_EQ(evalScalar("numel(methods('Widget'))"), 1.0); // only `pub`
    engine.eval("w = Widget();");
    EXPECT_DOUBLE_EQ(evalScalar("w.internal()"), 2.0); // still callable
}
TEST_P(SealedHiddenClassdefTest, BareClassNameConstructs)
{
    engine.eval("w = Widget;"); // no parens — constructs a default instance
    EXPECT_DOUBLE_EQ(evalScalar("w.name"), 1.0);
}
INSTANTIATE_TEST_SUITE_P(Backends, SealedHiddenClassdefTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ── classdef loaded from a Name.m file on the path ──
TEST(ClassdefMFile, LoadFromFile)
{
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "numkit_classdef_mfile_test";
    fs::create_directories(dir);
    {
        std::ofstream f(dir / "Vec2.m");
        f << "classdef Vec2\n"
             "  properties\n    x = 0\n    y = 0\n  end\n"
             "  methods\n"
             "    function obj = Vec2(a, b)\n      obj.x = a;\n      obj.y = b;\n    end\n"
             "    function s = sumsq(obj)\n      s = obj.x^2 + obj.y^2;\n    end\n"
             "  end\n"
             "end\n";
    }
    for (auto backend : {Engine::Backend::TreeWalker, Engine::Backend::VM}) {
        StdEngine engine;
        engine.setBackend(backend);
        engine.addPath(dir.string());
        EXPECT_DOUBLE_EQ(engine.eval("v = Vec2(3, 4); v.x").toScalar(), 3.0);
        EXPECT_DOUBLE_EQ(engine.eval("v.sumsq()").toScalar(), 25.0);
        EXPECT_EQ(engine.eval("class(v)").toString(), "Vec2");
    }
    fs::remove_all(dir);
}

// Editing a class .m file then calling `rehash` must reload the new
// definition, not keep serving the stale class machinery (registry +
// `Name>method` chunks + bare ctor external). Before the fix rehash only
// dropped the bare-key chunk (a no-op for a class), so the old class lingered.
TEST(ClassdefMFile, RehashReloadsEditedClass)
{
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "numkit_classdef_rehash_test";
    fs::create_directories(dir);
    auto writeVer = [&](int k) {
        std::ofstream f(dir / "RhVec.m", std::ios::trunc);
        f << "classdef RhVec\n"
             "  properties\n    x = 0\n  end\n"
             "  methods\n"
             "    function obj = RhVec(a)\n      obj.x = a;\n    end\n"
             "    function r = bump(obj)\n      r = obj.x + "
          << k << ";\n    end\n"
             "  end\n"
             "end\n";
    };
    for (auto backend : {Engine::Backend::TreeWalker, Engine::Backend::VM}) {
        writeVer(10);
        StdEngine engine;
        engine.setBackend(backend);
        engine.addPath(dir.string());
        EXPECT_DOUBLE_EQ(engine.eval("v = RhVec(5); v.bump()").toScalar(), 15.0);
        // Rewrite the method body, then rehash → next reference reloads it.
        writeVer(20);
        engine.eval("rehash");
        EXPECT_DOUBLE_EQ(engine.eval("v = RhVec(5); v.bump()").toScalar(), 25.0)
            << "edited class file must reload after rehash";
    }
    fs::remove_all(dir);
}

// Packaged classes (`+pkg/Name.m`) are registered under their QUALIFIED name so
// class()/isa() report `pkg.Name` (not the bare leaf), and clear/rehash key on
// the same `pkg.Name` the ctor external + mFileCache_ use — so eviction works.
TEST(ClassdefMFile, PackagedClassQualifiedIdentity)
{
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "numkit_pkgclass_test";
    fs::create_directories(dir / "+geo");
    auto writeVer = [&](int k) {
        std::ofstream f(dir / "+geo" / "Vec.m", std::ios::trunc);
        f << "classdef Vec\n"
             "  properties\n    x = 0\n  end\n"
             "  methods\n"
             "    function obj = Vec(a)\n      obj.x = a;\n    end\n"
             "    function r = bump(obj)\n      r = obj.x + "
          << k << ";\n    end\n"
             "  end\n"
             "end\n";
    };
    for (auto backend : {Engine::Backend::TreeWalker, Engine::Backend::VM}) {
        writeVer(10);
        StdEngine engine;
        engine.setBackend(backend);
        engine.addPath(dir.string());
        EXPECT_DOUBLE_EQ(engine.eval("v = geo.Vec(5); v.x").toScalar(), 5.0);
        EXPECT_DOUBLE_EQ(engine.eval("v.bump()").toScalar(), 15.0);
        EXPECT_EQ(engine.eval("class(v)").toString(), "geo.Vec"); // qualified, not "Vec"
        EXPECT_TRUE(engine.eval("isa(v, 'geo.Vec')").toBool());
        EXPECT_FALSE(engine.eval("isa(v, 'Vec')").toBool());
        // rehash reloads the edited packaged class (keys now align).
        writeVer(20);
        engine.eval("rehash");
        EXPECT_DOUBLE_EQ(engine.eval("w = geo.Vec(5); w.bump()").toScalar(), 25.0)
            << "edited packaged class must reload after rehash";
    }
    fs::remove_all(dir);
}

// An inline subclass of a FILE base must re-merge the base after `rehash`
// reloads an edited base file. The inline subclass survives rehash (it is not
// an m-file), so rehash reloads the base and cascades the refresh to it.
TEST(ClassdefMFile, RehashBaseRefreshesInlineSubclass)
{
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "numkit_rehash_inline_sub_test";
    fs::create_directories(dir);
    auto writeBase = [&](int k) {
        std::ofstream f(dir / "RBaseF.m", std::ios::trunc);
        f << "classdef RBaseF\n  properties\n    tag = 0\n  end\n"
             "  methods\n    function r = bval(obj)\n      r = "
          << k << ";\n    end\n  end\nend\n";
    };
    for (auto backend : {Engine::Backend::TreeWalker, Engine::Backend::VM}) {
        writeBase(10);
        StdEngine engine;
        engine.setBackend(backend);
        engine.addPath(dir.string());
        // Inline subclass of the FILE base (the base loads via the path).
        engine.eval("classdef RSubI < RBaseF\n"
                    "  methods\n    function b = own(obj)\n      b = 1;\n    end\n  end\nend\n");
        EXPECT_DOUBLE_EQ(engine.eval("s = RSubI; s.bval()").toScalar(), 10.0);
        // Edit the base file, rehash → the inline subclass must re-merge it.
        writeBase(20);
        engine.eval("rehash");
        EXPECT_DOUBLE_EQ(engine.eval("s = RSubI; s.bval()").toScalar(), 20.0)
            << "inline subclass must re-merge the rehashed file base";
        EXPECT_DOUBLE_EQ(engine.eval("s = RSubI; s.own()").toScalar(), 1.0);
    }
    fs::remove_all(dir);
}

// Inline subclass of a PACKAGED file base: rehash of the edited base must
// cascade to the inline subclass. Exercises the propagation cascade matching a
// QUALIFIED superclass name (geo.Shape) — the packaged-class path.
TEST(ClassdefMFile, RehashPackagedBaseRefreshesInlineSubclass)
{
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "numkit_pkg_inherit_rehash_test";
    fs::create_directories(dir / "+geo");
    auto writeBase = [&](int k) {
        std::ofstream f(dir / "+geo" / "Shape.m", std::ios::trunc);
        f << "classdef Shape\n  properties\n    tag = 0\n  end\n"
             "  methods\n    function a = area(obj)\n      a = "
          << k << ";\n    end\n  end\nend\n";
    };
    for (auto backend : {Engine::Backend::TreeWalker, Engine::Backend::VM}) {
        writeBase(10);
        StdEngine engine;
        engine.setBackend(backend);
        engine.addPath(dir.string());
        engine.eval("classdef MyShape < geo.Shape\n"
                    "  methods\n    function n = name(obj)\n      n = 5;\n    end\n  end\nend\n");
        EXPECT_DOUBLE_EQ(engine.eval("s = MyShape; s.area()").toScalar(), 10.0);
        EXPECT_TRUE(engine.eval("isa(s, 'geo.Shape')").toBool());
        writeBase(20);
        engine.eval("rehash");
        EXPECT_DOUBLE_EQ(engine.eval("s = MyShape; s.area()").toScalar(), 20.0)
            << "inline subclass must re-merge the rehashed packaged base";
        EXPECT_DOUBLE_EQ(engine.eval("s = MyShape; s.name()").toScalar(), 5.0);
    }
    fs::remove_all(dir);
}

// Inheritance must not depend on which file is referenced (loaded) first:
// referencing the subclass before the base still pulls the base in.
TEST(ClassdefMFile, InheritanceOrderIndependent)
{
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "numkit_classdef_inh_test";
    fs::create_directories(dir);
    {
        std::ofstream(dir / "AnimalB.m")
            << "classdef AnimalB\n"
               "  properties\n    legs = 4\n  end\n"
               "  methods\n    function s = legCount(obj)\n      s = obj.legs;\n    end\n  end\n"
               "end\n";
        std::ofstream(dir / "DogB.m")
            << "classdef DogB < AnimalB\n  properties\n    barks = 1\n  end\nend\n";
    }
    for (auto backend : {Engine::Backend::TreeWalker, Engine::Backend::VM}) {
        StdEngine engine;
        engine.setBackend(backend);
        engine.addPath(dir.string());
        // Reference DogB FIRST → DogB.m loads before AnimalB.m. The base must
        // still be pulled in so its members merge into DogB.
        EXPECT_DOUBLE_EQ(engine.eval("d = DogB(); d.legs").toScalar(), 4.0);  // inherited prop
        EXPECT_DOUBLE_EQ(engine.eval("d.legCount()").toScalar(), 4.0);        // inherited method
        EXPECT_DOUBLE_EQ(engine.eval("d.barks").toScalar(), 1.0);             // own prop
        EXPECT_TRUE(engine.eval("isa(d, 'AnimalB')").toBool());
    }
    fs::remove_all(dir);
}

// Object-array display goes through Engine::formatObjectDisplay (shared by
// both engines), so test it directly on a C++-built array.
TEST(ObjectArrayDisplay, ArrayHeaderAndProps)
{
    StdEngine engine;
    ObjectArrayTest::registerBox(engine, "Box", /*isHandle=*/false);
    engine.eval("a(1) = Box(10); a(2) = Box(20); a(3) = Box(30);");
    Value a = engine.eval("a;");
    std::string disp = engine.formatObjectDisplay("a", a);
    EXPECT_NE(disp.find("Box array"), std::string::npos) << disp;
    EXPECT_NE(disp.find("1\xC3\x97" "3"), std::string::npos) << disp; // 1×3
    EXPECT_NE(disp.find("v"), std::string::npos) << disp;             // prop list
}

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
    StdEngine engine;
    Value m = c::map(engine.resource());
    c::set(m, S("k"), N(7.0));
    engine.setVariable("m", m);
    EXPECT_DOUBLE_EQ(engine.eval("m('k')").toScalar(), 7.0);
    EXPECT_DOUBLE_EQ(engine.eval("m.Count").toScalar(), 1.0);
}
