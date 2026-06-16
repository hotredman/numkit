// bundle/src/register/fusion/fused_rules.cpp
//
// Concrete element-wise fusion rules (idiom → ops kernel). This is the ONLY
// place that knows specific idioms; core stays domain-free (it just iterates
// the registered FusionRule list). Adding an idiom = an ops kernel + a rule
// here — core/VM/compiler are never touched.
//
// Rule 1 — clamp:  max(lo, min(hi, x))  →  fusedAffineClamp(x, 1, 0, lo, hi).
// Reuses the affine-clamp kernel (scale=1, offset=0). Fires on the common
// saturate idiom (e.g. XMAP's repeated max(0, min(1, v))).

#include <numkit/core/engine.hpp>
#include <numkit/core/fusion_rule.hpp>
#include <numkit/core/ast.hpp>
#include <numkit/value/value.hpp>
#include <numkit/ops/fused_kernels.hpp>
#include <numkit/ops/helpers.hpp>   // numkit::ops::createLike

#include <optional>
#include <vector>

namespace numkit {

namespace {

// Below this many elements the per-op path is fine and the dispatch isn't
// worth it — decline (fall back). Correctness is unaffected either way.
constexpr std::size_t kFusionMinElems = 1024;

// A leaf whose re-evaluation is free and side-effect-free, so that a declined
// kernel can fall back to the normal per-op path without re-running effects.
bool isPureLeaf(const ASTNode *n) {
    switch (n->type) {
        case NodeType::IDENTIFIER:
        case NodeType::NUMBER_LITERAL:
        case NodeType::BOOL_LITERAL:
            return true;
        case NodeType::FIELD_ACCESS:  // s.f / a.b.c over a pure root
            return !n->children.empty() && isPureLeaf(n->children[0].get());
        default:
            return false;
    }
}

// CALL whose callee is a bare identifier → return that name node, else null.
const ASTNode *calleeName(const ASTNode *node) {
    if (node->type != NodeType::CALL || node->children.empty()) return nullptr;
    const ASTNode *callee = node->children[0].get();
    return callee->type == NodeType::IDENTIFIER ? callee : nullptr;
}

// match: max(lo, min(hi, x))  →  operands [x, lo, hi]  (idiomatic order).
std::optional<std::vector<const ASTNode *>> matchClamp(const ASTNode *node) {
    const ASTNode *mx = calleeName(node);
    if (!mx || mx->strValue != "max" || node->children.size() != 3)
        return std::nullopt;
    const ASTNode *lo    = node->children[1].get();
    const ASTNode *inner = node->children[2].get();
    const ASTNode *mn = calleeName(inner);
    if (!mn || mn->strValue != "min" || inner->children.size() != 3)
        return std::nullopt;
    const ASTNode *hi = inner->children[1].get();
    const ASTNode *x  = inner->children[2].get();
    if (!isPureLeaf(lo) || !isPureLeaf(hi) || !isPureLeaf(x))
        return std::nullopt;
    return std::vector<const ASTNode *>{x, lo, hi};
}

// execute: fast path for a real-double array x and real-double scalar lo/hi.
// Bit-identical to max(lo, min(hi, x)); declines (false → fall back) otherwise.
bool execClamp(const Value *ops, std::size_t n, Value &out,
               std::pmr::memory_resource *mr) {
    if (n != 3) return false;
    const Value &x = ops[0], &lo = ops[1], &hi = ops[2];
    if (x.type() != ValueType::DOUBLE || x.isComplex()) return false;
    const std::size_t N = x.numel();
    if (N < kFusionMinElems) return false;
    if (!lo.isScalar() || lo.type() != ValueType::DOUBLE || lo.isComplex()) return false;
    if (!hi.isScalar() || hi.type() != ValueType::DOUBLE || hi.isComplex()) return false;

    out = ops::createLike(x, ValueType::DOUBLE, mr);
    ops::fusedAffineClamp(x.doubleData(), 1.0, 0.0, lo.toScalar(), hi.toScalar(),
                          out.doubleDataMut(), N);
    return true;
}

} // namespace

void registerFusionRules(Engine &engine) {
    FusionRule clamp;
    clamp.name    = "clamp";
    clamp.match   = matchClamp;
    clamp.execute = execClamp;
    engine.addFusionRule(std::move(clamp));
}

} // namespace numkit
