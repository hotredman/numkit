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
    Engine engine;

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
    Engine engine;
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
INSTANTIATE_TEST_SUITE_P(Backends, ClassdefTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// ── classdef handle classes (< handle): reference semantics ──
class HandleClassdefTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    Engine engine;
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
    Engine engine;
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
    Engine engine;
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
INSTANTIATE_TEST_SUITE_P(Backends, AttrClassdefTest,
                         ::testing::Values(Engine::Backend::TreeWalker,
                                           Engine::Backend::VM));

// Object-array display goes through Engine::formatObjectDisplay (shared by
// both engines), so test it directly on a C++-built array.
TEST(ObjectArrayDisplay, ArrayHeaderAndProps)
{
    Engine engine;
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
    Engine engine;
    Value m = c::map(engine.resource());
    c::set(m, S("k"), N(7.0));
    engine.setVariable("m", m);
    EXPECT_DOUBLE_EQ(engine.eval("m('k')").toScalar(), 7.0);
    EXPECT_DOUBLE_EQ(engine.eval("m.Count").toScalar(), 1.0);
}
