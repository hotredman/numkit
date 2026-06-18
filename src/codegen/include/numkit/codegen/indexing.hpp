// codegen/include/numkit/codegen/indexing.hpp
//
// The index module — the single place that decides HOW an indexing site
// `array(i, ...)` is lowered. It is split into a pure DECISION
// (planIndexRead/Write, here) and the C++ EMISSION (done later by the
// emitter from the chosen form + the argument sub-expressions). This
// file is the decision: unit-testable on its own, without an emitter or
// a compiler.
//
// Contract 2 (DESIGN.md §10): a fast (unboxed) form is chosen only when
// its precondition is provable; everything else returns Runtime — the
// always-correct fallback that reuses the engine's own indexing
// (Value::index / indexSet). So this function can never produce a wrong
// access: an un-provable case routes to the engine. N-D (3+ subscripts),
// logical (mask) / range / `end` indexing, deletion, and non-typed
// arrays all fall to Runtime.

#pragma once

#include <numkit/codegen/inference.hpp>  // AbstractValue

#include <vector>

namespace numkit::codegen {

enum class IndexForm {
    LinearScalar,  // x(n)    — one scalar numeric (non-logical) index
    Subscript2D,   // A(i,j)  — two scalar numeric indices, column-major
    Runtime,       // everything else -> nk_rt::index / indexSet (engine)
};

struct IndexPlan {
    IndexForm form          = IndexForm::Runtime;
    bool      boundsChecked = true;  // false only when an in-bounds fact is proven
                                     // (no bounds analysis yet -> always true)

    bool operator==(const IndexPlan &o) const
    {
        return form == o.form && boundsChecked == o.boundsChecked;
    }
    bool operator!=(const IndexPlan &o) const { return !(*this == o); }
};

// A value we materialise as a raw typed buffer (the fast path can take
// its address): a concrete numeric / complex / logical array or scalar.
// Cell / struct / string / func-handle / char / object / dynamic are not.
bool isBufferArray(const AbstractValue &v);

// A value usable as a 1-based scalar POSITION index: a concrete
// non-logical numeric scalar. Logical -> mask indexing (Runtime); a
// range/vector is non-scalar (Runtime); `end`/unknown is Dynamic
// (Runtime).
bool isScalarPositionIndex(const AbstractValue &idx);

// Decide how to lower `array(indices)` as a READ.
IndexPlan planIndexRead(const AbstractValue &array,
                        const std::vector<AbstractValue> &indices);

// Decide how to lower `array(indices) = rhs` as a WRITE. The fast store
// additionally requires `rhs` to be an unboxed scalar whose dtype matches
// the buffer (yp[i] = <double>); deletion (`x(i)=[]`), dtype conversion,
// and growth-with-promotion all route to Runtime.
IndexPlan planIndexWrite(const AbstractValue &array,
                         const std::vector<AbstractValue> &indices,
                         const AbstractValue &rhs);

} // namespace numkit::codegen
