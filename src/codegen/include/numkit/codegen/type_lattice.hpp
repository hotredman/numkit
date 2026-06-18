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

namespace numkit::codegen {

// ── Shape lattice ─────────────────────────────────────────────────────
// MVP: scalar / concrete-2D-dims / unknown. Symbolic dims and ranks
// beyond 2-D are deliberately deferred; the join already collapses
// anything it cannot prove equal to Unknown, so adding richer shapes
// later only refines results, never breaks soundness.
enum class ShapeKind : std::uint8_t {
    Unknown,    // top — any shape
    Scalar,     // 1 x 1
    KnownDims,  // concrete rows x cols
};

struct Shape {
    ShapeKind   kind = ShapeKind::Unknown;
    std::size_t rows = 0;   // meaningful only when kind == KnownDims
    std::size_t cols = 0;

    static Shape unknown() { return {ShapeKind::Unknown, 0, 0}; }
    static Shape scalar()  { return {ShapeKind::Scalar, 1, 1}; }
    // A 1x1 KnownDims is canonicalised to Scalar so the two never alias.
    static Shape dims(std::size_t r, std::size_t c)
    {
        if (r == 1 && c == 1) return scalar();
        return {ShapeKind::KnownDims, r, c};
    }

    bool isScalar() const { return kind == ShapeKind::Scalar; }
    bool operator==(const Shape &o) const;
    bool operator!=(const Shape &o) const { return !(*this == o); }
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

    // ── constructors ──
    static InferredType dynamic() { return {TypeKind::Dynamic, ValueType::EMPTY, {}}; }
    static InferredType bottom()  { return {TypeKind::Bottom,  ValueType::EMPTY, {}}; }
    static InferredType concrete(ValueType dt, Shape s)
    {
        return {TypeKind::Concrete, dt, s};
    }
    static InferredType scalar(ValueType dt) { return concrete(dt, Shape::scalar()); }

    bool isDynamic()  const { return kind == TypeKind::Dynamic; }
    bool isConcrete() const { return kind == TypeKind::Concrete; }
    bool isBottom()   const { return kind == TypeKind::Bottom; }

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
