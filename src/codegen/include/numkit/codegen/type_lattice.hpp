// codegen/include/numkit/codegen/type_lattice.hpp
//
// Type lattice for the numkit static type-inference pass — the
// foundation of the AST -> typed-IR -> C++ transpiler.
//
// An InferredType abstracts, at a single program point, what a numkit
// value's dtype and shape are KNOWN to be. The inference pass runs a
// forward dataflow over the control-flow graph; this lattice defines
// the values it propagates and the `join` used at control-flow merges.
//
// Lattice (the relation is "more precise than"):
//
//          Dynamic            <- top: type unknown -> stays a boxed Value
//        /    |    \
//   Concrete(double,3x3) ...  <- a definite, unboxable dtype + shape
//        \    |    /
//           Bottom            <- contradiction / unreachable
//
// The whole point: a value that infers to a Concrete *scalar* numeric
// type can be emitted as an unboxed C++ primitive (`double`, ...)
// instead of a heap-boxed `numkit::Value`. Boxing is the bulk of the
// interpreter's per-operation cost, so proving "this is an unboxed
// double" is exactly what makes transpiled code fast.
//
// This pass is offline analysis (sibling of scriptgraph): it consumes a
// parsed AST and registers no engine builtin.

#pragma once

#include <numkit/value/value_type.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace numkit::codegen {

// ── Shape lattice ─────────────────────────────────────────────────────
// MVP: scalar / concrete-2D-dims / unknown. Symbolic dims and ranks
// beyond 2-D are deliberately deferred; the join already collapses
// anything it cannot prove equal to Unknown, so adding richer shapes
// later only refines results, never breaks soundness.
enum class ShapeKind : std::uint8_t {
    Unknown,    // top — any shape
    Scalar,     // 1 x 1
    RowVector,  // 1 x N, N not statically known (e.g. 1:N, runtime linspace)
    ColVector,  // N x 1, N not statically known
    KnownDims,  // concrete rows x cols (rank 2, both dims compile-time)
    NDims,      // ranked: rank = nd.size(); each nd[k] is the dim size, 0 = runtime-unknown.
                // Used for rank >= 3 (true N-D) AND a rank-2 matrix with a runtime dim.
                // A fully-known rank-2 canonicalises to KnownDims (see ndShape).
};

struct Shape {
    ShapeKind                kind = ShapeKind::Unknown;
    std::size_t              rows = 0;  // meaningful only when kind == KnownDims
    std::size_t              cols = 0;
    std::vector<std::size_t> nd;        // meaningful only when kind == NDims (0 = unknown dim)

    static Shape unknown()   { return {ShapeKind::Unknown, 0, 0, {}}; }
    static Shape scalar()    { return {ShapeKind::Scalar, 1, 1, {}}; }
    static Shape rowVector() { return {ShapeKind::RowVector, 0, 0, {}}; }
    static Shape colVector() { return {ShapeKind::ColVector, 0, 0, {}}; }
    // A 1x1 KnownDims is canonicalised to Scalar so the two never alias.
    static Shape dims(std::size_t r, std::size_t c)
    {
        if (r == 1 && c == 1) return scalar();
        return {ShapeKind::KnownDims, r, c, {}};
    }
    // A ranked shape (column-major). A fully-known rank-2 collapses to
    // KnownDims so static matrices keep their existing single representation;
    // everything else (rank >= 3, or a rank-2 with a runtime dim) is NDims.
    static Shape ndShape(std::vector<std::size_t> d)
    {
        if (d.size() == 2 && d[0] >= 1 && d[1] >= 1) return dims(d[0], d[1]);
        Shape s;
        s.kind = ShapeKind::NDims;
        s.nd   = std::move(d);
        return s;
    }

    bool        isScalar() const { return kind == ShapeKind::Scalar; }
    bool        isNDims() const { return kind == ShapeKind::NDims; }
    std::size_t ndRank() const { return kind == ShapeKind::NDims ? nd.size() : 0; }
    bool        operator==(const Shape &o) const;
    bool        operator!=(const Shape &o) const { return !(*this == o); }
};

// Least upper bound of two shapes (used at control-flow merges).
Shape joinShape(const Shape &a, const Shape &b);

// ── Type lattice ──────────────────────────────────────────────────────
enum class TypeKind : std::uint8_t {
    Bottom,    // no value reaches here (identity of join)
    Concrete,  // a definite dtype + shape
    Dynamic,   // top — statically unknown -> boxed numkit::Value
};

struct InferredType {
    TypeKind  kind  = TypeKind::Dynamic;
    ValueType dtype = ValueType::EMPTY;  // meaningful only when Concrete
    Shape     shape{};                   // meaningful only when Concrete
    // Class identity, meaningful ONLY when dtype == OBJECT (a class
    // instance). -1 for every non-object type, so it never perturbs the
    // numeric path's equality / join. Indexes a ClassRegistry (classinfo.hpp).
    int       classId = -1;

    // ── constructors ──
    static InferredType dynamic() { return {TypeKind::Dynamic, ValueType::EMPTY, {}}; }
    static InferredType bottom()  { return {TypeKind::Bottom,  ValueType::EMPTY, {}}; }
    static InferredType concrete(ValueType dt, Shape s)
    {
        return {TypeKind::Concrete, dt, s};
    }
    static InferredType scalar(ValueType dt) { return concrete(dt, Shape::scalar()); }
    // A class instance (value or handle decided by the ClassRegistry, not
    // the lattice). A scalar object in v1 (object arrays are a later tier).
    static InferredType object(int classId)
    {
        return {TypeKind::Concrete, ValueType::OBJECT, Shape::scalar(), classId};
    }

    bool isDynamic()  const { return kind == TypeKind::Dynamic; }
    bool isConcrete() const { return kind == TypeKind::Concrete; }
    bool isBottom()   const { return kind == TypeKind::Bottom; }
    bool isObject()   const { return kind == TypeKind::Concrete && dtype == ValueType::OBJECT; }

    // True when this can be emitted as an unboxed C++ primitive scalar
    // (double / float / bool / intN / complex<double>) rather than a
    // heap-boxed numkit::Value. The transpiler's green light.
    bool isUnboxableScalar() const;

    bool operator==(const InferredType &o) const;
    bool operator!=(const InferredType &o) const { return !(*this == o); }

    std::string str() const;  // human-readable, for debug / diagnostics
};

// Lattice join (least upper bound) — combine the two inferred types that
// reach a control-flow merge point. Bottom is the identity; Dynamic is
// absorbing; two Concretes join to a Concrete iff their dtype agrees
// (shape generalises via joinShape), otherwise the variable is
// type-unstable here and the result is Dynamic (must be boxed).
InferredType join(const InferredType &a, const InferredType &b);

// ── Constant lattice ──────────────────────────────────────────────────
// Sparse-conditional-constant-propagation facet, tracked alongside the
// type for every value. It exists because MATLAB *shapes* depend on
// argument *values*, not just types: `linspace(0,1,n)` / `zeros(n)` /
// `reshape(x,a,b)` need the compile-time value of `n`/`a`/`b` to infer a
// concrete shape. A value whose constant is Known lets a size-constructor
// transfer function produce KnownDims instead of an Unknown shape.
//
// Lattice: Unknown (top — could be anything) over Known(real scalar).
// (Bottom/undefined is not modelled separately in the MVP — an
//  unreachable value's type is already Bottom.) Only real scalar
// constants are tracked for now; that covers the size arguments that
// drive shape inference.
enum class ConstKind : std::uint8_t {
    Unknown,    // top — not a known compile-time constant
    KnownReal,  // a known real scalar value
};

struct ConstVal {
    ConstKind kind  = ConstKind::Unknown;
    double    value = 0.0;  // meaningful only when kind == KnownReal

    static ConstVal unknown()       { return {ConstKind::Unknown, 0.0}; }
    static ConstVal known(double v) { return {ConstKind::KnownReal, v}; }

    bool isKnown() const { return kind == ConstKind::KnownReal; }

    // Read the constant as an array dimension: a known non-negative
    // integer value, else nullopt. This is the form size-constructor
    // transfer functions consume (the `n` in zeros(n) / linspace(_,_,n)).
    // Returns nullopt for Unknown, non-integral, or negative values.
    // (Out param keeps the header free of <optional> in hot include
    //  paths; returns true on success.)
    bool asDim(std::size_t &out) const;

    bool operator==(const ConstVal &o) const;
    bool operator!=(const ConstVal &o) const { return !(*this == o); }

    std::string str() const;
};

// Lattice join for constants: equal Known values stay Known; anything
// else (Unknown present, or two different Known values) is Unknown.
ConstVal join(const ConstVal &a, const ConstVal &b);

} // namespace numkit::codegen
