// codegen/src/indexing.cpp — see indexing.hpp.

#include <numkit/codegen/indexing.hpp>

namespace numkit::codegen {

bool isBufferArray(const AbstractValue &v)
{
    if (!v.type.isConcrete()) return false;
    switch (v.type.dtype) {
    case ValueType::DOUBLE:
    case ValueType::SINGLE:
    case ValueType::COMPLEX:
    case ValueType::LOGICAL:
    case ValueType::INT8:
    case ValueType::INT16:
    case ValueType::INT32:
    case ValueType::INT64:
    case ValueType::UINT8:
    case ValueType::UINT16:
    case ValueType::UINT32:
    case ValueType::UINT64:
        return true;
    default:  // CHAR / CELL / STRUCT / STRING / FUNC_HANDLE / OBJECT / EMPTY
        return false;
    }
}

bool isScalarPositionIndex(const AbstractValue &idx)
{
    if (!idx.type.isConcrete())    return false;  // end / unknown -> Runtime
    if (!idx.type.shape.isScalar()) return false; // range / vector -> Runtime
    switch (idx.type.dtype) {
    case ValueType::DOUBLE:
    case ValueType::SINGLE:
    case ValueType::INT8:
    case ValueType::INT16:
    case ValueType::INT32:
    case ValueType::INT64:
    case ValueType::UINT8:
    case ValueType::UINT16:
    case ValueType::UINT32:
    case ValueType::UINT64:
        return true;
    default:  // LOGICAL (mask), COMPLEX, CHAR -> Runtime
        return false;
    }
}

namespace {

// The fast-form (LinearScalar / Subscript2D) selection shared by reads
// and writes: a typed buffer indexed by 1 or 2 scalar positions. Zero
// indices, 3+ (N-D), or any non-scalar / non-typed operand -> Runtime.
IndexForm formFor(const AbstractValue &array,
                  const std::vector<AbstractValue> &indices)
{
    if (!isBufferArray(array)) return IndexForm::Runtime;
    for (const auto &idx : indices)
        if (!isScalarPositionIndex(idx)) return IndexForm::Runtime;
    if (indices.size() == 1) return IndexForm::LinearScalar;
    if (indices.size() == 2) return IndexForm::Subscript2D;
    return IndexForm::Runtime;  // 0 args, or 3+ subscripts (N-D) -> engine
}

} // namespace

IndexPlan planIndexRead(const AbstractValue &array,
                        const std::vector<AbstractValue> &indices)
{
    return {formFor(array, indices), /*boundsChecked=*/true};
}

IndexPlan planIndexWrite(const AbstractValue &array,
                         const std::vector<AbstractValue> &indices,
                         const AbstractValue &rhs)
{
    const IndexForm f = formFor(array, indices);
    if (f == IndexForm::Runtime) return {IndexForm::Runtime, true};

    // A direct store `buf[off] = rhs` is sound only when rhs is an unboxed
    // scalar of the buffer's own dtype. A non-matching / non-scalar rhs
    // (deletion `[]`, dtype conversion, growth-with-promotion) -> Runtime.
    const bool rhsOk = rhs.type.isUnboxableScalar()
                       && array.type.isConcrete()
                       && rhs.type.dtype == array.type.dtype;
    if (!rhsOk) return {IndexForm::Runtime, true};
    return {f, true};
}

} // namespace numkit::codegen
