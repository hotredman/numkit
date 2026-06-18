// codegen/tests/indexing_test.cpp
//
// Unit tests for the index module's DECISION logic (planIndexRead/Write).
// No emitter or compiler involved — we feed inferred abstract values and
// assert which lowering form is chosen. The guiding property: anything
// not provably a simple scalar / 2-D subscript on a typed buffer routes
// to Runtime (the always-correct engine fallback).

#include <numkit/codegen/indexing.hpp>

#include <gtest/gtest.h>

using numkit::ValueType;
using namespace numkit::codegen;

namespace {
AbstractValue arr(ValueType dt, std::size_t r, std::size_t c)
{
    return {InferredType::concrete(dt, Shape::dims(r, c)), ConstVal::unknown()};
}
AbstractValue scalar(ValueType dt)
{
    return {InferredType::scalar(dt), ConstVal::unknown()};
}
AbstractValue dynamic() { return AbstractValue::dynamic(); }
}  // namespace

// x(n): a scalar numeric index on a double vector -> LinearScalar.
TEST(Indexing, LinearScalarRead)
{
    const auto p = planIndexRead(arr(ValueType::DOUBLE, 1, 10), {scalar(ValueType::DOUBLE)});
    EXPECT_EQ(p.form, IndexForm::LinearScalar);
}

// A(i,j): two scalar indices -> Subscript2D.
TEST(Indexing, Subscript2DRead)
{
    const auto p = planIndexRead(arr(ValueType::DOUBLE, 4, 4),
                                 {scalar(ValueType::DOUBLE), scalar(ValueType::DOUBLE)});
    EXPECT_EQ(p.form, IndexForm::Subscript2D);
}

// N-D (3+ subscripts) -> Runtime (correctness via the engine fallback).
TEST(Indexing, NDSubscriptIsRuntime)
{
    const auto p = planIndexRead(arr(ValueType::DOUBLE, 2, 2),
                                 {scalar(ValueType::DOUBLE), scalar(ValueType::DOUBLE),
                                  scalar(ValueType::DOUBLE)});
    EXPECT_EQ(p.form, IndexForm::Runtime);
}

// A range index (x(2:5) -> a row vector) is not a scalar position -> Runtime.
TEST(Indexing, RangeIndexIsRuntime)
{
    const AbstractValue range = {InferredType::concrete(ValueType::DOUBLE, Shape::rowVector()),
                                 ConstVal::unknown()};
    const auto p = planIndexRead(arr(ValueType::DOUBLE, 1, 10), {range});
    EXPECT_EQ(p.form, IndexForm::Runtime);
}

// A logical index is mask indexing, NOT a position -> Runtime (even scalar).
TEST(Indexing, LogicalIndexIsRuntime)
{
    const auto p = planIndexRead(arr(ValueType::DOUBLE, 1, 10), {scalar(ValueType::LOGICAL)});
    EXPECT_EQ(p.form, IndexForm::Runtime);
}

// `end` / anything inference couldn't type is Dynamic -> Runtime.
TEST(Indexing, DynamicIndexIsRuntime)
{
    const auto p = planIndexRead(arr(ValueType::DOUBLE, 1, 10), {dynamic()});
    EXPECT_EQ(p.form, IndexForm::Runtime);
}

// A non-typed array (Dynamic / cell / struct) -> Runtime.
TEST(Indexing, NonBufferArrayIsRuntime)
{
    EXPECT_EQ(planIndexRead(dynamic(), {scalar(ValueType::DOUBLE)}).form, IndexForm::Runtime);
    EXPECT_EQ(planIndexRead(scalar(ValueType::CELL), {scalar(ValueType::DOUBLE)}).form,
              IndexForm::Runtime);
}

// Integer-typed buffers and indices are fine (e.g. int8 array, index n).
TEST(Indexing, IntegerBufferLinearScalar)
{
    const auto p = planIndexRead(arr(ValueType::INT8, 1, 10), {scalar(ValueType::DOUBLE)});
    EXPECT_EQ(p.form, IndexForm::LinearScalar);
}

// ── writes ────────────────────────────────────────────────────────────

// y(n) = yn  with yn a double scalar into a double buffer -> LinearScalar.
TEST(Indexing, ScalarWriteMatchingDtype)
{
    const auto p = planIndexWrite(arr(ValueType::DOUBLE, 1, 10),
                                  {scalar(ValueType::DOUBLE)}, scalar(ValueType::DOUBLE));
    EXPECT_EQ(p.form, IndexForm::LinearScalar);
}

// y(n) = <dynamic> (e.g. deletion [] or an untyped rhs) -> Runtime.
TEST(Indexing, WriteDynamicRhsIsRuntime)
{
    const auto p = planIndexWrite(arr(ValueType::DOUBLE, 1, 10),
                                  {scalar(ValueType::DOUBLE)}, dynamic());
    EXPECT_EQ(p.form, IndexForm::Runtime);
}

// y(n) = <complex> into a double buffer -> dtype change -> Runtime
// (promotion of the whole array is the engine's job).
TEST(Indexing, WriteDtypeMismatchIsRuntime)
{
    const auto p = planIndexWrite(arr(ValueType::DOUBLE, 1, 10),
                                  {scalar(ValueType::DOUBLE)}, scalar(ValueType::COMPLEX));
    EXPECT_EQ(p.form, IndexForm::Runtime);
}

// y(n) = <array> (non-scalar rhs) -> Runtime.
TEST(Indexing, WriteNonScalarRhsIsRuntime)
{
    const auto p = planIndexWrite(arr(ValueType::DOUBLE, 1, 10),
                                  {scalar(ValueType::DOUBLE)},
                                  arr(ValueType::DOUBLE, 1, 3));
    EXPECT_EQ(p.form, IndexForm::Runtime);
}