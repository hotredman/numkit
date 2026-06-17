// bundle/src/register/fusion/fused_rules.cpp
//
// Concrete element-wise fusion rules (idiom → ops kernel). This is the ONLY
// place that knows specific idioms; core stays domain-free (it just iterates
// the registered FusionRule list). Adding an idiom = an ops kernel + a rule
// here — core/VM/compiler are never touched.
//
// Rules:
//   clamp       max(lo, min(hi, x))   → fusedAffineClamp(x, 1, 0, lo, hi)
//   affine_add  (a.*x) + b / b + (a.*x) → fusedAffine(x, a, +b)
//   affine_sub  (a.*x) - b            → fusedAffine(x, a, -b)
//
// A small shared matcher layer (the `is*` helpers) does the AST inspection so
// each rule's match closure stays a few lines; the execute closures share the
// runtime type/shape guard via bindAffineProduct.

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

// ---- shared matcher / guard infra --------------------------------------

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

// node iff it is a binary operator `op` with two children, else null.
const ASTNode *asBinOp(const ASTNode *node, const char *op) {
    if (node->type == NodeType::BINARY_OP && node->children.size() == 2 &&
        node->strValue == op)
        return node;
    return nullptr;
}

// node iff it is a product (`.*` or `*`) of two pure leaves, else null. For a
// scalar coefficient `*` and `.*` are identical, and the execute guard rejects
// a non-scalar coefficient, so accepting both spellings here is safe.
const ASTNode *asPureProduct(const ASTNode *node) {
    if (node->type != NodeType::BINARY_OP || node->children.size() != 2)
        return nullptr;
    if (node->strValue != ".*" && node->strValue != "*") return nullptr;
    if (!isPureLeaf(node->children[0].get()) ||
        !isPureLeaf(node->children[1].get()))
        return nullptr;
    return node;
}

bool isRealDoubleScalar(const Value &v) {
    return v.isScalar() && v.type() == ValueType::DOUBLE && !v.isComplex();
}

// A product's two evaluated operands must be exactly one real-double scalar
// (the coefficient → scale) and one real-double array of >= kFusionMinElems
// (→ x). Bind them; return false (decline → fall back) otherwise. This is what
// resolves `a.*x` vs `x.*a` at runtime without the matcher guessing.
bool bindAffineProduct(const Value &c0, const Value &c1, double &scale,
                       const Value *&x) {
    auto isArr = [](const Value &v) {
        return v.type() == ValueType::DOUBLE && !v.isComplex() &&
               v.numel() >= kFusionMinElems;
    };
    if (isRealDoubleScalar(c0) && isArr(c1)) { scale = c0.toScalar(); x = &c1; return true; }
    if (isRealDoubleScalar(c1) && isArr(c0)) { scale = c1.toScalar(); x = &c0; return true; }
    return false;
}

// ---- clamp:  max(lo, min(hi, x))  →  fusedAffineClamp(x, 1, 0, lo, hi) ----

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

// Fast path for a real-double array x and real-double scalar lo/hi.
// Bit-identical to max(lo, min(hi, x)); declines (false → fall back) otherwise.
bool execClamp(const Value *ops, std::size_t n, Value &out,
               std::pmr::memory_resource *mr) {
    if (n != 3) return false;
    const Value &x = ops[0], &lo = ops[1], &hi = ops[2];
    if (x.type() != ValueType::DOUBLE || x.isComplex()) return false;
    const std::size_t N = x.numel();
    if (N < kFusionMinElems) return false;
    if (!isRealDoubleScalar(lo) || !isRealDoubleScalar(hi)) return false;

    out = ops::createLike(x, ValueType::DOUBLE, mr);
    ops::fusedAffineClamp(x.doubleData(), 1.0, 0.0, lo.toScalar(), hi.toScalar(),
                          out.doubleDataMut(), N);
    return true;
}

// ---- affine:  (a.*x) ± b  →  fusedAffine(x, a, ±b) -----------------------

// match the additive form `prod + b` or `b + prod`, prod = a.*x of pure leaves.
std::optional<std::vector<const ASTNode *>> matchAffineAdd(const ASTNode *node) {
    const ASTNode *add = asBinOp(node, "+");
    if (!add) return std::nullopt;
    const ASTNode *l = add->children[0].get(), *r = add->children[1].get();
    const ASTNode *prod = asPureProduct(l);
    const ASTNode *b = r;
    if (!prod) { prod = asPureProduct(r); b = l; }  // commute: b + a.*x
    if (!prod || !isPureLeaf(b)) return std::nullopt;
    return std::vector<const ASTNode *>{prod->children[0].get(),
                                        prod->children[1].get(), b};
}

// match the subtractive form `prod - b`, prod = a.*x of pure leaves. (`b -
// prod` negates the scale — a different recipe, deferred to a later rule.)
std::optional<std::vector<const ASTNode *>> matchAffineSub(const ASTNode *node) {
    const ASTNode *sub = asBinOp(node, "-");
    if (!sub) return std::nullopt;
    const ASTNode *prod = asPureProduct(sub->children[0].get());
    const ASTNode *b = sub->children[1].get();
    if (!prod || !isPureLeaf(b)) return std::nullopt;
    return std::vector<const ASTNode *>{prod->children[0].get(),
                                        prod->children[1].get(), b};
}

// Shared executor. operands = [c0, c1, b]; the matched expression is
// (c0 ⊗ c1) ⊕ b. `bSign` carries the additive operator (+1 add, -1 sub).
// offset = bSign*b is exact (multiply by ±1.0 does not round), and
// scale*x + offset matches per-op `a.*x ± b` bit-for-bit (negation and
// add/sub are the same exact IEEE op, both round twice overall).
bool execAffine(const Value *ops, std::size_t n, Value &out,
                std::pmr::memory_resource *mr, double bSign) {
    if (n != 3) return false;
    const Value &b = ops[2];
    if (!isRealDoubleScalar(b)) return false;
    double scale = 0.0;
    const Value *x = nullptr;
    if (!bindAffineProduct(ops[0], ops[1], scale, x)) return false;
    const double offset = bSign * b.toScalar();

    out = ops::createLike(*x, ValueType::DOUBLE, mr);
    ops::fusedAffine(x->doubleData(), scale, offset, out.doubleDataMut(),
                     x->numel());
    return true;
}

} // namespace

void registerFusionRules(Engine &engine) {
    FusionRule clamp;
    clamp.name    = "clamp";
    clamp.match   = matchClamp;
    clamp.execute = execClamp;
    engine.addFusionRule(std::move(clamp));

    FusionRule affineAdd;
    affineAdd.name  = "affine_add";
    affineAdd.match = matchAffineAdd;
    affineAdd.execute = [](const Value *ops, std::size_t n, Value &out,
                           std::pmr::memory_resource *mr) {
        return execAffine(ops, n, out, mr, +1.0);
    };
    engine.addFusionRule(std::move(affineAdd));

    FusionRule affineSub;
    affineSub.name  = "affine_sub";
    affineSub.match = matchAffineSub;
    affineSub.execute = [](const Value *ops, std::size_t n, Value &out,
                           std::pmr::memory_resource *mr) {
        return execAffine(ops, n, out, mr, -1.0);
    };
    engine.addFusionRule(std::move(affineSub));
}

} // namespace numkit
