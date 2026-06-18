// codegen/src/type_lattice.cpp
//
// Implementation of the type lattice — see type_lattice.hpp.

#include <numkit/codegen/type_lattice.hpp>

#include <sstream>

namespace numkit::codegen {

bool Shape::operator==(const Shape &o) const
{
    if (kind != o.kind) return false;
    if (kind == ShapeKind::KnownDims) return rows == o.rows && cols == o.cols;
    return true;  // Unknown == Unknown, Scalar == Scalar
}

Shape joinShape(const Shape &a, const Shape &b)
{
    // Equal shapes (incl. Unknown==Unknown, Scalar==Scalar, matching
    // KnownDims) are their own LUB. Any disagreement we cannot prove
    // away — Scalar vs KnownDims, or two different KnownDims — collapses
    // to Unknown: the shape is no longer statically fixed at this point.
    // (When symbolic dims / ranks are added later, this is where the
    //  richer join logic slots in; today it is intentionally coarse.)
    if (a == b) return a;
    return Shape::unknown();
}

bool InferredType::isUnboxableScalar() const
{
    if (kind != TypeKind::Concrete) return false;
    if (!shape.isScalar())          return false;
    switch (dtype) {
        case ValueType::DOUBLE:
        case ValueType::SINGLE:
        case ValueType::LOGICAL:
        case ValueType::COMPLEX:
        case ValueType::INT8:
        case ValueType::INT16:
        case ValueType::INT32:
        case ValueType::INT64:
        case ValueType::UINT8:
        case ValueType::UINT16:
        case ValueType::UINT32:
        case ValueType::UINT64:
            return true;
        // CHAR (MATLAB char is UTF-16) and the aggregate / handle types
        // (CELL / STRUCT / FUNC_HANDLE / STRING / OBJECT / EMPTY) are not
        // unboxable C++ scalars in the MVP.
        default:
            return false;
    }
}

bool InferredType::operator==(const InferredType &o) const
{
    if (kind != o.kind) return false;
    if (kind != TypeKind::Concrete) return true;  // Bottom/Dynamic carry no payload
    return dtype == o.dtype && shape == o.shape;
}

InferredType join(const InferredType &a, const InferredType &b)
{
    if (a.isBottom()) return b;             // Bottom is the identity
    if (b.isBottom()) return a;
    if (a.isDynamic() || b.isDynamic())     // Dynamic is absorbing
        return InferredType::dynamic();

    // Both Concrete. Differing dtype => the variable is type-unstable at
    // this merge (e.g. `x=1.0` on one path, `x='c'` on another) and must
    // be boxed. Same dtype => keep it; the shape generalises.
    if (a.dtype != b.dtype)
        return InferredType::dynamic();
    return InferredType::concrete(a.dtype, joinShape(a.shape, b.shape));
}

std::string InferredType::str() const
{
    switch (kind) {
        case TypeKind::Bottom:  return "bottom";
        case TypeKind::Dynamic: return "dynamic";
        case TypeKind::Concrete: {
            std::ostringstream os;
            os << mtypeName(dtype);
            switch (shape.kind) {
                case ShapeKind::Scalar:    os << " scalar"; break;
                case ShapeKind::KnownDims: os << ' ' << shape.rows << 'x' << shape.cols; break;
                case ShapeKind::Unknown:   os << " [?]"; break;
            }
            return os.str();
        }
    }
    return "?";
}

} // namespace numkit::codegen
