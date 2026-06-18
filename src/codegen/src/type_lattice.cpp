// codegen/src/type_lattice.cpp
//
// Implementation of the type lattice — see type_lattice.hpp.

#include <numkit/codegen/type_lattice.hpp>

#include <cmath>
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
                case ShapeKind::RowVector: os << " 1x?"; break;
                case ShapeKind::ColVector: os << " ?x1"; break;
                case ShapeKind::KnownDims: os << ' ' << shape.rows << 'x' << shape.cols; break;
                case ShapeKind::Unknown:   os << " [?]"; break;
            }
            return os.str();
        }
    }
    return "?";
}

// ── ConstVal ──────────────────────────────────────────────────────────

bool ConstVal::asDim(std::size_t &out) const
{
    if (kind != ConstKind::KnownReal) return false;
    // A valid dimension is a non-negative integer (finite) value.
    if (!std::isfinite(value) || value < 0.0) return false;
    const double r = std::floor(value);
    if (r != value) return false;  // non-integral
    out = static_cast<std::size_t>(r);
    return true;
}

bool ConstVal::operator==(const ConstVal &o) const
{
    if (kind != o.kind) return false;
    if (kind != ConstKind::KnownReal) return true;  // both Unknown
    // Bit-exact compare so NaN-vs-NaN and +0/-0 are handled consistently
    // with the rest of the lattice (Known(NaN) only equals Known(NaN)).
    if (std::isnan(value) && std::isnan(o.value)) return true;
    return value == o.value;
}

ConstVal join(const ConstVal &a, const ConstVal &b)
{
    if (a.isKnown() && b.isKnown() && a == b) return a;  // same constant
    return ConstVal::unknown();                          // disagree → top
}

std::string ConstVal::str() const
{
    if (kind != ConstKind::KnownReal) return "?";
    std::ostringstream os;
    os << value;
    return os.str();
}

} // namespace numkit::codegen
