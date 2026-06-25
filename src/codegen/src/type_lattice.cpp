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
    if (kind == ShapeKind::NDims) return nd == o.nd;
    return true;  // Unknown == Unknown, Scalar == Scalar
}

Shape joinShape(const Shape &a, const Shape &b)
{
    // Equal shapes (incl. Unknown==Unknown, Scalar==Scalar, matching
    // KnownDims / NDims) are their own LUB.
    if (a == b) return a;
    // Two NDims of the SAME rank join per dimension: a dimension both agree
    // on is kept, any disagreement becomes unknown (0). Rank disagreement, or
    // NDims vs a different kind, we cannot prove away -> Unknown.
    if (a.kind == ShapeKind::NDims && b.kind == ShapeKind::NDims
        && a.nd.size() == b.nd.size()) {
        std::vector<std::size_t> d(a.nd.size());
        for (std::size_t i = 0; i < d.size(); ++i) d[i] = (a.nd[i] == b.nd[i]) ? a.nd[i] : 0;
        return Shape::ndShape(std::move(d));
    }
    // Any other disagreement (Scalar vs KnownDims, two different KnownDims,
    // mixed rank/kind) collapses to Unknown — no longer statically fixed.
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
        case ValueType::CHAR:  // a 1x1 char is a uint16 code-unit scalar
            return true;
        // The aggregate / handle types (CELL / STRUCT / FUNC_HANDLE / STRING /
        // OBJECT / EMPTY) are not unboxable C++ scalars in the MVP.
        default:
            return false;
    }
}

const InferredType *StructLayout::field(const std::string &name) const
{
    for (const auto &f : fields)
        if (f.first == name) return &f.second;
    return nullptr;
}

bool StructLayout::operator==(const StructLayout &o) const
{
    if (fields.size() != o.fields.size()) return false;
    for (std::size_t i = 0; i < fields.size(); ++i)
        if (fields[i].first != o.fields[i].first || fields[i].second != o.fields[i].second)
            return false;  // InferredType::!= recurses into nested struct layouts
    return true;
}

// Equality of two (possibly null) shared struct layouts: same pointer (incl. both null)
// is equal; one null is unequal; otherwise a deep field-by-field compare.
static bool structLayoutEq(const std::shared_ptr<const StructLayout> &a,
                           const std::shared_ptr<const StructLayout> &b)
{
    if (a == b) return true;
    if (!a || !b) return false;
    return *a == *b;
}

bool InferredType::operator==(const InferredType &o) const
{
    if (kind != o.kind) return false;
    if (kind != TypeKind::Concrete) return true;  // Bottom/Dynamic carry no payload
    // classId is -1 for non-object types and structLayout is null for non-struct types,
    // so both are no-ops on the numeric path.
    return dtype == o.dtype && shape == o.shape && classId == o.classId
           && structLayoutEq(structLayout, o.structLayout);
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
    // Two object instances unify only if they are the SAME class; distinct
    // classes at a merge are type-unstable -> Dynamic (boxed). (Common
    // superclass / polymorphism is a later refinement.)
    if (a.dtype == ValueType::OBJECT) {
        if (a.classId != b.classId) return InferredType::dynamic();
        return InferredType::object(a.classId);
    }
    // Two struct values unify only if they have the IDENTICAL field layout; distinct
    // layouts at a merge are type-unstable -> Dynamic (boxed). (A width/depth-subtyping
    // join is a later refinement.)
    if (a.dtype == ValueType::STRUCT) {
        if (!structLayoutEq(a.structLayout, b.structLayout)) return InferredType::dynamic();
        return a;
    }
    return InferredType::concrete(a.dtype, joinShape(a.shape, b.shape));
}

std::string InferredType::str() const
{
    switch (kind) {
        case TypeKind::Bottom:  return "bottom";
        case TypeKind::Dynamic: return "dynamic";
        case TypeKind::Concrete: {
            std::ostringstream os;
            if (dtype == ValueType::OBJECT) {
                os << "object#" << classId;
                return os.str();
            }
            if (dtype == ValueType::STRUCT) {
                os << "struct{";
                if (structLayout)
                    for (std::size_t i = 0; i < structLayout->fields.size(); ++i) {
                        if (i) os << ',';
                        os << structLayout->fields[i].first;
                    }
                os << '}';
                return os.str();
            }
            os << mtypeName(dtype);
            switch (shape.kind) {
                case ShapeKind::Scalar:    os << " scalar"; break;
                case ShapeKind::RowVector: os << " 1x?"; break;
                case ShapeKind::ColVector: os << " ?x1"; break;
                case ShapeKind::KnownDims: os << ' ' << shape.rows << 'x' << shape.cols; break;
                case ShapeKind::NDims:
                    os << ' ';
                    for (std::size_t i = 0; i < shape.nd.size(); ++i) {
                        if (i) os << 'x';
                        if (shape.nd[i] == 0) os << '?';
                        else os << shape.nd[i];
                    }
                    break;
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
