// bundle/src/register/fusion/fused_rules.cpp
//
// Concrete element-wise fusion rules (idiom → ops kernel). This is the ONLY
// place that knows specific idioms; core stays domain-free (it just iterates
// the registered FusionRule list). Adding an idiom = an ops kernel + a rule
// here — core/VM/compiler are never touched.
//
// Rules:
//   clamp[_min_outer]  max(lo,min(hi,x)) / min(hi,max(lo,x)) → fusedAffineClamp[MinOuter]
//   affine_add  (a.*x) + b / b + (a.*x) → fusedAffine(x, a, +b)
//   affine_sub  (a.*x) - b            → fusedAffine(x, a, -b)
//   axpby_add   (a.*x) + (b.*y)       → fusedAxpby(x, a, y, +b)
//   axpby_sub   (a.*x) - (b.*y)       → fusedAxpby(x, a, y, -b)
//   shift_scale_mul  (x-c).*s / s.*(x-c) → fusedShiftScaleMul(x, c, s)
//   shift_scale_div  (x-c)./d           → fusedShiftScaleDiv(x, c, d)
//   affine_clamp_*  max(lo,min(hi,a.*x±b)) → fusedAffineClamp(x, a, ±b, lo, hi)
//   abs_affine_*    abs(a.*x ± b)         → fusedAbsAffine(x, a, ±b)
//   abs_diff        abs(x - y) / abs(x-c) → fusedAbsDiff / fusedAbsAffine
//   {sqrt,floor,ceil}_affine_*  f(a.*x ± b) → fusedUnaryAffine(x, a, ±b, fn)
//   sq_affine_*  (a.*x ± b).^2  → fusedSqAffine(x, a, ±b)
//   sq_diff      (x-y).^2 / (x-c).^2 → fusedSqDiff / fusedSqAffine
//   sqrt_sumsq   sqrt(x.^2 + y.^2)   → fusedSqrtSumSq(x, y)
//   soft_threshold  sign(x).*max(0,abs(x)-t) → fusedSoftThreshold(x, t)
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

// ---- clamp:  max(lo,min(hi,x)) / min(hi,max(lo,x)) → fusedAffineClamp[MinOuter]

// Peel a clamp shape → (lo, hi, inner). minOuter=false is the max-outer spelling
// `max(lo, min(hi, inner))`; minOuter=true is `min(hi, max(lo, inner))`. The two
// agree for finite values but differ on NaN, so each routes to its own kernel.
// The inner expression is left for the caller to classify (a leaf for plain
// clamp, an affine product for the affine-clamp rules).
struct ClampShape { const ASTNode *lo, *hi, *inner; };
std::optional<ClampShape> peelClamp(const ASTNode *node, bool minOuter) {
    const char *outerFn = minOuter ? "min" : "max";
    const char *innerFn = minOuter ? "max" : "min";
    const ASTNode *o = calleeName(node);
    if (!o || o->strValue != outerFn || node->children.size() != 3)
        return std::nullopt;
    const ASTNode *innerCall = node->children[2].get();
    const ASTNode *ic = calleeName(innerCall);
    if (!ic || ic->strValue != innerFn || innerCall->children.size() != 3)
        return std::nullopt;
    // outer = OUTER(boundO, INNER(boundI, x)); max-outer → boundO=lo, boundI=hi;
    // min-outer → boundO=hi, boundI=lo.
    const ASTNode *boundO = node->children[1].get();
    const ASTNode *boundI = innerCall->children[1].get();
    const ASTNode *x = innerCall->children[2].get();
    return ClampShape{minOuter ? boundI : boundO, minOuter ? boundO : boundI, x};
}

std::optional<std::vector<const ASTNode *>> matchClamp(const ASTNode *node,
                                                       bool minOuter) {
    auto cs = peelClamp(node, minOuter);
    if (!cs) return std::nullopt;
    if (!isPureLeaf(cs->lo) || !isPureLeaf(cs->hi) || !isPureLeaf(cs->inner))
        return std::nullopt;
    return std::vector<const ASTNode *>{cs->inner, cs->lo, cs->hi};
}

// Fast path for a real-double array x and real-double scalar lo/hi. Bit-
// identical to the matched clamp spelling; declines (false → fall back) else.
bool execClamp(const Value *ops, std::size_t n, Value &out,
               std::pmr::memory_resource *mr, bool minOuter) {
    if (n != 3) return false;
    const Value &x = ops[0], &lo = ops[1], &hi = ops[2];
    if (x.type() != ValueType::DOUBLE || x.isComplex()) return false;
    const std::size_t N = x.numel();
    if (N < kFusionMinElems) return false;
    if (!isRealDoubleScalar(lo) || !isRealDoubleScalar(hi)) return false;

    out = ops::createLike(x, ValueType::DOUBLE, mr);
    auto kernel = minOuter ? ops::fusedAffineClampMinOuter : ops::fusedAffineClamp;
    kernel(x.doubleData(), 1.0, 0.0, lo.toScalar(), hi.toScalar(),
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

// ---- axpby:  (a.*x) ± (b.*y)  →  fusedAxpby(x, a, y, ±b) -----------------

// match `prod ± prod`, both products of pure leaves. Requiring BOTH sides to be
// products keeps this structurally disjoint from affine_add/sub (whose offset
// is a leaf, not a product), so each binary node matches at most one rule —
// important because the VM compiler commits to one rule at compile time.
std::optional<std::vector<const ASTNode *>> matchAxpby(const ASTNode *node,
                                                       const char *op) {
    const ASTNode *bin = asBinOp(node, op);
    if (!bin) return std::nullopt;
    const ASTNode *p = asPureProduct(bin->children[0].get());
    const ASTNode *q = asPureProduct(bin->children[1].get());
    if (!p || !q) return std::nullopt;
    return std::vector<const ASTNode *>{p->children[0].get(), p->children[1].get(),
                                        q->children[0].get(), q->children[1].get()};
}

// operands = [pc0, pc1, qc0, qc1]; expr = (pc0⊗pc1) ⊕ (qc0⊗qc1). `bSign` carries
// the additive operator: out = a*x + (bSign*b)*y, bit-identical to a.*x ± b.*y
// ((-b)*y = -(b*y) exactly, so the sign fold does not change any rounding).
bool execAxpby(const Value *ops, std::size_t n, Value &out,
               std::pmr::memory_resource *mr, double bSign) {
    if (n != 4) return false;
    double a = 0.0, b = 0.0;
    const Value *x = nullptr, *y = nullptr;
    if (!bindAffineProduct(ops[0], ops[1], a, x)) return false;
    if (!bindAffineProduct(ops[2], ops[3], b, y)) return false;
    if (x->dims() != y->dims()) return false;  // element-wise: identical shape

    out = ops::createLike(*x, ValueType::DOUBLE, mr);
    ops::fusedAxpby(x->doubleData(), a, y->doubleData(), bSign * b,
                    out.doubleDataMut(), x->numel());
    return true;
}

// ---- shift-scale:  (x - c) .* s  /  (x - c) ./ d ------------------------

// node iff it is `leaf - leaf` (a pure subtract), else null.
const ASTNode *asPureSub(const ASTNode *node) {
    if (node->type != NodeType::BINARY_OP || node->children.size() != 2)
        return nullptr;
    if (node->strValue != "-") return nullptr;
    if (!isPureLeaf(node->children[0].get()) ||
        !isPureLeaf(node->children[1].get()))
        return nullptr;
    return node;
}

// match `(A - B) .* s` / `s .* (A - B)` (`*` accepted: scalar s ≡ `.*`).
std::optional<std::vector<const ASTNode *>>
matchShiftScaleMul(const ASTNode *node) {
    const ASTNode *mul = asBinOp(node, ".*");
    if (!mul) mul = asBinOp(node, "*");
    if (!mul) return std::nullopt;
    const ASTNode *sub = asPureSub(mul->children[0].get());
    const ASTNode *s = mul->children[1].get();
    if (!sub) { sub = asPureSub(mul->children[1].get()); s = mul->children[0].get(); }
    if (!sub || !isPureLeaf(s)) return std::nullopt;
    return std::vector<const ASTNode *>{sub->children[0].get(),
                                        sub->children[1].get(), s};
}

// match `(A - B) ./ d` / `(A - B) / d` (division does NOT commute → d is the
// right operand only). A non-scalar d makes `/` an mrdivide; the execute guard
// declines it, so accepting `/` here is safe.
std::optional<std::vector<const ASTNode *>>
matchShiftScaleDiv(const ASTNode *node) {
    const ASTNode *div = asBinOp(node, "./");
    if (!div) div = asBinOp(node, "/");
    if (!div) return std::nullopt;
    const ASTNode *sub = asPureSub(div->children[0].get());
    const ASTNode *d = div->children[1].get();
    if (!sub || !isPureLeaf(d)) return std::nullopt;
    return std::vector<const ASTNode *>{sub->children[0].get(),
                                        sub->children[1].get(), d};
}

// operands = [A, B, s]; expr = (A - B) ⊗ s, ⊗ = `*` (isDiv=false) or `/`. The
// subtract must be array-minus-scalar — A the real-double array, B a real
// scalar; `c - x` (A scalar) declines on size and falls back. s/d real scalar.
bool execShiftScale(const Value *ops, std::size_t n, Value &out,
                    std::pmr::memory_resource *mr, bool isDiv) {
    if (n != 3) return false;
    const Value &A = ops[0], &B = ops[1], &s = ops[2];
    if (A.type() != ValueType::DOUBLE || A.isComplex()) return false;
    const std::size_t N = A.numel();
    if (N < kFusionMinElems) return false;
    if (!isRealDoubleScalar(B) || !isRealDoubleScalar(s)) return false;

    out = ops::createLike(A, ValueType::DOUBLE, mr);
    if (isDiv)
        ops::fusedShiftScaleDiv(A.doubleData(), B.toScalar(), s.toScalar(),
                                out.doubleDataMut(), N);
    else
        ops::fusedShiftScaleMul(A.doubleData(), B.toScalar(), s.toScalar(),
                                out.doubleDataMut(), N);
    return true;
}

// ---- affine-clamp:  max(lo, min(hi, (a.*x) ± b)) ------------------------
// Normalize-then-saturate in one pass — reuses fusedAffineClamp with bound
// scale/offset (plain clamp = the a=1,b=0 special case). The inner affine is
// detected by reusing matchAffineAdd/matchAffineSub.

std::optional<std::vector<const ASTNode *>>
matchAffineClamp(const ASTNode *node, bool sub, bool minOuter) {
    auto cs = peelClamp(node, minOuter);
    if (!cs || !isPureLeaf(cs->lo) || !isPureLeaf(cs->hi)) return std::nullopt;
    auto aff = sub ? matchAffineSub(cs->inner) : matchAffineAdd(cs->inner);
    if (!aff) return std::nullopt;            // [c0, c1, b]
    aff->push_back(cs->lo);
    aff->push_back(cs->hi);
    return aff;                               // [c0, c1, b, lo, hi]
}

// operands = [c0, c1, b, lo, hi]; expr = clamp((c0⊗c1) ⊕ b) to [lo,hi]. Same
// bit-exactness as affine (mul-then-add, ±b exact) composed with the clamp
// kernel's fmin/fmax (validated against per-op min/max by the plain-clamp rule).
bool execAffineClamp(const Value *ops, std::size_t n, Value &out,
                     std::pmr::memory_resource *mr, double bSign, bool minOuter) {
    if (n != 5) return false;
    const Value &b = ops[2], &lo = ops[3], &hi = ops[4];
    if (!isRealDoubleScalar(b) || !isRealDoubleScalar(lo) ||
        !isRealDoubleScalar(hi))
        return false;
    double scale = 0.0;
    const Value *x = nullptr;
    if (!bindAffineProduct(ops[0], ops[1], scale, x)) return false;
    const double offset = bSign * b.toScalar();

    out = ops::createLike(*x, ValueType::DOUBLE, mr);
    auto kernel = minOuter ? ops::fusedAffineClampMinOuter : ops::fusedAffineClamp;
    kernel(x->doubleData(), scale, offset, lo.toScalar(), hi.toScalar(),
           out.doubleDataMut(), x->numel());
    return true;
}

// ---- abs:  abs(a.*x ± b)  and  abs(x - y) / abs(x - c) ------------------

// single-argument abs(arg) → arg node, else null.
const ASTNode *absArg(const ASTNode *node) {
    const ASTNode *fn = calleeName(node);
    if (!fn || fn->strValue != "abs" || node->children.size() != 2) return nullptr;
    return node->children[1].get();
}

// abs of an affine product: reuse the affine inner-detection on abs's argument.
std::optional<std::vector<const ASTNode *>> matchAbsAffine(const ASTNode *node,
                                                           bool sub) {
    const ASTNode *arg = absArg(node);
    if (!arg) return std::nullopt;
    return sub ? matchAffineSub(arg) : matchAffineAdd(arg);  // [c0,c1,b]
}

// abs of a pure subtract `A - B` (covers abs(x-y), abs(x-c), abs(c-x)).
std::optional<std::vector<const ASTNode *>> matchAbsDiff(const ASTNode *node) {
    const ASTNode *arg = absArg(node);
    if (!arg) return std::nullopt;
    const ASTNode *s = asPureSub(arg);
    if (!s) return std::nullopt;
    return std::vector<const ASTNode *>{s->children[0].get(),
                                        s->children[1].get()};
}

// operands = [c0, c1, b]; expr = abs((c0⊗c1) ⊕ b) → |scale*x ± b|.
bool execAbsAffine(const Value *ops, std::size_t n, Value &out,
                   std::pmr::memory_resource *mr, double bSign) {
    if (n != 3) return false;
    const Value &b = ops[2];
    if (!isRealDoubleScalar(b)) return false;
    double scale = 0.0;
    const Value *x = nullptr;
    if (!bindAffineProduct(ops[0], ops[1], scale, x)) return false;
    out = ops::createLike(*x, ValueType::DOUBLE, mr);
    ops::fusedAbsAffine(x->doubleData(), scale, bSign * b.toScalar(),
                        out.doubleDataMut(), x->numel());
    return true;
}

// operands = [A, B]; expr = abs(A - B). Two same-shape arrays → fusedAbsDiff;
// array-minus-scalar (either side) → fusedAbsAffine (|x-c| == |1*x + (-c)|,
// and |c-x| == |x-c|). Anything else (broadcast, both scalar) declines.
bool execAbsDiff(const Value *ops, std::size_t n, Value &out,
                 std::pmr::memory_resource *mr) {
    if (n != 2) return false;
    const Value &A = ops[0], &B = ops[1];
    auto isArr = [](const Value &v) {
        return v.type() == ValueType::DOUBLE && !v.isComplex() &&
               v.numel() >= kFusionMinElems;
    };
    const bool aArr = isArr(A), bArr = isArr(B);
    if (aArr && bArr) {
        if (A.dims() != B.dims()) return false;
        out = ops::createLike(A, ValueType::DOUBLE, mr);
        ops::fusedAbsDiff(A.doubleData(), B.doubleData(), out.doubleDataMut(),
                          A.numel());
        return true;
    }
    if (aArr && isRealDoubleScalar(B)) {  // |A - c|
        out = ops::createLike(A, ValueType::DOUBLE, mr);
        ops::fusedAbsAffine(A.doubleData(), 1.0, -B.toScalar(),
                            out.doubleDataMut(), A.numel());
        return true;
    }
    if (bArr && isRealDoubleScalar(A)) {  // |c - B| == |B - c|
        out = ops::createLike(B, ValueType::DOUBLE, mr);
        ops::fusedAbsAffine(B.doubleData(), 1.0, -A.toScalar(),
                            out.doubleDataMut(), B.numel());
        return true;
    }
    return false;
}

// ---- unary-affine:  f(a.*x ± b),  f ∈ {sqrt, floor, ceil} --------------
// A unary whose SIMD form is bit-identical to libm (sqrt correctly-rounded,
// floor/ceil exact) applied to an affine, in one pass. Reuses the affine inner-
// detection. exp/log/sin… are excluded — Highway's polynomial differs from libm
// by a few ULP, so they would not be bit-exact across the SIMD/scalar split.

// fname(arg) single-arg call → arg node, else null.
const ASTNode *unaryCallArg(const ASTNode *node, const char *fname) {
    const ASTNode *fn = calleeName(node);
    if (!fn || fn->strValue != fname || node->children.size() != 2) return nullptr;
    return node->children[1].get();
}

std::optional<std::vector<const ASTNode *>>
matchUnaryAffine(const ASTNode *node, const char *fname, bool sub) {
    const ASTNode *arg = unaryCallArg(node, fname);
    if (!arg) return std::nullopt;
    return sub ? matchAffineSub(arg) : matchAffineAdd(arg);  // [c0, c1, b]
}

// True if any scale*x[i]+offset is < 0 (the affine the kernel will compute).
// Used to decline sqrt of an affine that goes negative — MATLAB promotes the
// whole array to complex there, which the per-op fallback reproduces.
bool affineAnyNegative(const double *x, double scale, double offset,
                       std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
        if (scale * x[i] + offset < 0.0) return true;
    return false;
}

bool execUnaryAffine(const Value *ops, std::size_t n, Value &out,
                     std::pmr::memory_resource *mr, double bSign,
                     numkit::ops::UnaryAffineFn fn) {
    if (n != 3) return false;
    const Value &b = ops[2];
    if (!isRealDoubleScalar(b)) return false;
    double scale = 0.0;
    const Value *x = nullptr;
    if (!bindAffineProduct(ops[0], ops[1], scale, x)) return false;
    const double offset = bSign * b.toScalar();
    const std::size_t N = x->numel();
    if (fn == numkit::ops::UnaryAffineFn::Sqrt &&
        affineAnyNegative(x->doubleData(), scale, offset, N))
        return false;  // → complex per-op path

    out = ops::createLike(*x, ValueType::DOUBLE, mr);
    ops::fusedUnaryAffine(x->doubleData(), scale, offset, fn,
                          out.doubleDataMut(), N);
    return true;
}

// ---- square / magnitude:  (a.*x±b).^2,  (x-y).^2,  sqrt(x.^2+y.^2) ------
// All keyed on `.^ 2` literal nodes (the AST is unchanged by the x.^2==x.*x
// runtime fast-path), and bit-exact because each square is a plain Mul.

// node iff it is `base .^ 2` (literal exponent 2) → base, else null.
const ASTNode *asSquare(const ASTNode *node) {
    if (node->type != NodeType::BINARY_OP || node->strValue != ".^" ||
        node->children.size() != 2)
        return nullptr;
    const ASTNode *e = node->children[1].get();
    if (e->type != NodeType::NUMBER_LITERAL || e->numValue != 2.0) return nullptr;
    return node->children[0].get();
}

// (a.*x ± b).^2 — square of an affine product, reusing the affine detection.
std::optional<std::vector<const ASTNode *>> matchSqAffine(const ASTNode *node,
                                                          bool sub) {
    const ASTNode *base = asSquare(node);
    if (!base) return std::nullopt;
    return sub ? matchAffineSub(base) : matchAffineAdd(base);  // [c0, c1, b]
}

// (A - B).^2 — square of a pure subtract (SSE term, squared deviation).
std::optional<std::vector<const ASTNode *>> matchSqDiff(const ASTNode *node) {
    const ASTNode *base = asSquare(node);
    if (!base) return std::nullopt;
    const ASTNode *s = asPureSub(base);
    if (!s) return std::nullopt;
    return std::vector<const ASTNode *>{s->children[0].get(),
                                        s->children[1].get()};
}

// sqrt(x.^2 + y.^2) — magnitude of two pure-leaf arrays.
std::optional<std::vector<const ASTNode *>> matchSqrtSumSq(const ASTNode *node) {
    const ASTNode *arg = unaryCallArg(node, "sqrt");
    if (!arg) return std::nullopt;
    const ASTNode *add = asBinOp(arg, "+");
    if (!add) return std::nullopt;
    const ASTNode *bx = asSquare(add->children[0].get());
    const ASTNode *by = asSquare(add->children[1].get());
    if (!bx || !by || !isPureLeaf(bx) || !isPureLeaf(by)) return std::nullopt;
    return std::vector<const ASTNode *>{bx, by};
}

bool execSqAffine(const Value *ops, std::size_t n, Value &out,
                  std::pmr::memory_resource *mr, double bSign) {
    if (n != 3) return false;
    const Value &b = ops[2];
    if (!isRealDoubleScalar(b)) return false;
    double scale = 0.0;
    const Value *x = nullptr;
    if (!bindAffineProduct(ops[0], ops[1], scale, x)) return false;
    out = ops::createLike(*x, ValueType::DOUBLE, mr);
    ops::fusedSqAffine(x->doubleData(), scale, bSign * b.toScalar(),
                       out.doubleDataMut(), x->numel());
    return true;
}

// (A - B).^2: two same-shape arrays → fusedSqDiff; array-minus-scalar either
// way → fusedSqAffine(arr, 1, -c) (squaring makes (x-c)^2 == (c-x)^2).
bool execSqDiff(const Value *ops, std::size_t n, Value &out,
                std::pmr::memory_resource *mr) {
    if (n != 2) return false;
    const Value &A = ops[0], &B = ops[1];
    auto isArr = [](const Value &v) {
        return v.type() == ValueType::DOUBLE && !v.isComplex() &&
               v.numel() >= kFusionMinElems;
    };
    const bool aArr = isArr(A), bArr = isArr(B);
    if (aArr && bArr) {
        if (A.dims() != B.dims()) return false;
        out = ops::createLike(A, ValueType::DOUBLE, mr);
        ops::fusedSqDiff(A.doubleData(), B.doubleData(), out.doubleDataMut(),
                         A.numel());
        return true;
    }
    if (aArr && isRealDoubleScalar(B)) {
        out = ops::createLike(A, ValueType::DOUBLE, mr);
        ops::fusedSqAffine(A.doubleData(), 1.0, -B.toScalar(),
                           out.doubleDataMut(), A.numel());
        return true;
    }
    if (bArr && isRealDoubleScalar(A)) {
        out = ops::createLike(B, ValueType::DOUBLE, mr);
        ops::fusedSqAffine(B.doubleData(), 1.0, -A.toScalar(),
                           out.doubleDataMut(), B.numel());
        return true;
    }
    return false;
}

bool execSqrtSumSq(const Value *ops, std::size_t n, Value &out,
                   std::pmr::memory_resource *mr) {
    if (n != 2) return false;
    const Value &x = ops[0], &y = ops[1];
    auto isArr = [](const Value &v) {
        return v.type() == ValueType::DOUBLE && !v.isComplex() &&
               v.numel() >= kFusionMinElems;
    };
    if (!isArr(x) || !isArr(y) || x.dims() != y.dims()) return false;
    out = ops::createLike(x, ValueType::DOUBLE, mr);
    ops::fusedSqrtSumSq(x.doubleData(), y.doubleData(), out.doubleDataMut(),
                        x.numel());
    return true;
}

// ---- soft-threshold:  sign(x) .* max(0, abs(x) - t) --------------------
// Wavelet/L1 shrinkage. A fixed multi-node shape; the sign(x) and abs(x) must
// refer to the SAME variable so re-evaluating the (identifier) operand on a
// declined fallback is safe and identical.

bool isZeroLiteral(const ASTNode *n) {
    return n->type == NodeType::NUMBER_LITERAL && n->numValue == 0.0;
}
bool sameIdentifier(const ASTNode *a, const ASTNode *b) {
    return a->type == NodeType::IDENTIFIER && b->type == NodeType::IDENTIFIER &&
           a->strValue == b->strValue;
}

std::optional<std::vector<const ASTNode *>> matchSoftThreshold(const ASTNode *node) {
    const ASTNode *mul = asBinOp(node, ".*");
    if (!mul) return std::nullopt;
    // one factor is sign(x), the other max(0, abs(x)-t) (either order).
    const ASTNode *c0 = mul->children[0].get(), *c1 = mul->children[1].get();
    const ASTNode *signX = unaryCallArg(c0, "sign");
    const ASTNode *maxNode = c1;
    if (!signX) { signX = unaryCallArg(c1, "sign"); maxNode = c0; }
    if (!signX) return std::nullopt;
    // maxNode = max(0, abs(x)-t) / max(abs(x)-t, 0).
    const ASTNode *mxName = calleeName(maxNode);
    if (!mxName || mxName->strValue != "max" || maxNode->children.size() != 3)
        return std::nullopt;
    const ASTNode *ma = maxNode->children[1].get(), *mb = maxNode->children[2].get();
    const ASTNode *sub = isZeroLiteral(ma) ? mb : (isZeroLiteral(mb) ? ma : nullptr);
    if (!sub) return std::nullopt;
    const ASTNode *subN = asBinOp(sub, "-");           // must be abs(x) - t
    if (!subN) return std::nullopt;
    const ASTNode *absX = unaryCallArg(subN->children[0].get(), "abs");
    const ASTNode *t = subN->children[1].get();
    if (!absX || !isPureLeaf(t) || !sameIdentifier(signX, absX)) return std::nullopt;
    return std::vector<const ASTNode *>{signX, t};     // [x, t]
}

bool execSoftThreshold(const Value *ops, std::size_t n, Value &out,
                       std::pmr::memory_resource *mr) {
    if (n != 2) return false;
    const Value &x = ops[0], &t = ops[1];
    if (x.type() != ValueType::DOUBLE || x.isComplex()) return false;
    const std::size_t N = x.numel();
    if (N < kFusionMinElems) return false;
    if (!isRealDoubleScalar(t)) return false;
    out = ops::createLike(x, ValueType::DOUBLE, mr);
    ops::fusedSoftThreshold(x.doubleData(), t.toScalar(), out.doubleDataMut(), N);
    return true;
}

} // namespace

void registerFusionRules(Engine &engine) {
    // Plain clamp, both spellings: max(lo,min(hi,x)) and min(hi,max(lo,x)).
    for (bool minOuter : {false, true}) {
        FusionRule clamp;
        clamp.name    = minOuter ? "clamp_min_outer" : "clamp";
        clamp.match   = [minOuter](const ASTNode *node) {
            return matchClamp(node, minOuter);
        };
        clamp.execute = [minOuter](const Value *ops, std::size_t n, Value &out,
                                   std::pmr::memory_resource *mr) {
            return execClamp(ops, n, out, mr, minOuter);
        };
        engine.addFusionRule(std::move(clamp));
    }

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

    FusionRule axpbyAdd;
    axpbyAdd.name  = "axpby_add";
    axpbyAdd.match = [](const ASTNode *node) { return matchAxpby(node, "+"); };
    axpbyAdd.execute = [](const Value *ops, std::size_t n, Value &out,
                          std::pmr::memory_resource *mr) {
        return execAxpby(ops, n, out, mr, +1.0);
    };
    engine.addFusionRule(std::move(axpbyAdd));

    FusionRule axpbySub;
    axpbySub.name  = "axpby_sub";
    axpbySub.match = [](const ASTNode *node) { return matchAxpby(node, "-"); };
    axpbySub.execute = [](const Value *ops, std::size_t n, Value &out,
                          std::pmr::memory_resource *mr) {
        return execAxpby(ops, n, out, mr, -1.0);
    };
    engine.addFusionRule(std::move(axpbySub));

    FusionRule shiftScaleMul;
    shiftScaleMul.name  = "shift_scale_mul";
    shiftScaleMul.match = matchShiftScaleMul;
    shiftScaleMul.execute = [](const Value *ops, std::size_t n, Value &out,
                               std::pmr::memory_resource *mr) {
        return execShiftScale(ops, n, out, mr, /*isDiv=*/false);
    };
    engine.addFusionRule(std::move(shiftScaleMul));

    FusionRule shiftScaleDiv;
    shiftScaleDiv.name  = "shift_scale_div";
    shiftScaleDiv.match = matchShiftScaleDiv;
    shiftScaleDiv.execute = [](const Value *ops, std::size_t n, Value &out,
                               std::pmr::memory_resource *mr) {
        return execShiftScale(ops, n, out, mr, /*isDiv=*/true);
    };
    engine.addFusionRule(std::move(shiftScaleDiv));

    // affine-clamp, both add/sub and both clamp spellings (4 rules).
    auto addAffineClamp = [&engine](const char *name, bool sub, bool minOuter) {
        const double bSign = sub ? -1.0 : +1.0;
        FusionRule r;
        r.name  = name;
        r.match = [sub, minOuter](const ASTNode *node) {
            return matchAffineClamp(node, sub, minOuter);
        };
        r.execute = [bSign, minOuter](const Value *ops, std::size_t n, Value &out,
                                      std::pmr::memory_resource *mr) {
            return execAffineClamp(ops, n, out, mr, bSign, minOuter);
        };
        engine.addFusionRule(std::move(r));
    };
    addAffineClamp("affine_clamp_add",           /*sub=*/false, /*minOuter=*/false);
    addAffineClamp("affine_clamp_sub",           /*sub=*/true,  /*minOuter=*/false);
    addAffineClamp("affine_clamp_min_outer_add", /*sub=*/false, /*minOuter=*/true);
    addAffineClamp("affine_clamp_min_outer_sub", /*sub=*/true,  /*minOuter=*/true);

    FusionRule absAffineAdd;
    absAffineAdd.name  = "abs_affine_add";
    absAffineAdd.match = [](const ASTNode *node) {
        return matchAbsAffine(node, /*sub=*/false);
    };
    absAffineAdd.execute = [](const Value *ops, std::size_t n, Value &out,
                              std::pmr::memory_resource *mr) {
        return execAbsAffine(ops, n, out, mr, +1.0);
    };
    engine.addFusionRule(std::move(absAffineAdd));

    FusionRule absAffineSub;
    absAffineSub.name  = "abs_affine_sub";
    absAffineSub.match = [](const ASTNode *node) {
        return matchAbsAffine(node, /*sub=*/true);
    };
    absAffineSub.execute = [](const Value *ops, std::size_t n, Value &out,
                              std::pmr::memory_resource *mr) {
        return execAbsAffine(ops, n, out, mr, -1.0);
    };
    engine.addFusionRule(std::move(absAffineSub));

    FusionRule absDiff;
    absDiff.name  = "abs_diff";
    absDiff.match = matchAbsDiff;
    absDiff.execute = execAbsDiff;
    engine.addFusionRule(std::move(absDiff));

    // unary-affine f(a.*x ± b): one rule per (function, additive sign). The
    // function-id and ±b sign are baked into the captured execute closure.
    auto addUnaryAffine = [&engine](const char *name, const char *fname,
                                    bool sub, double bSign,
                                    numkit::ops::UnaryAffineFn fn) {
        FusionRule r;
        r.name  = name;
        r.match = [fname, sub](const ASTNode *node) {
            return matchUnaryAffine(node, fname, sub);
        };
        r.execute = [bSign, fn](const Value *ops, std::size_t n, Value &out,
                                std::pmr::memory_resource *mr) {
            return execUnaryAffine(ops, n, out, mr, bSign, fn);
        };
        engine.addFusionRule(std::move(r));
    };
    using UF = numkit::ops::UnaryAffineFn;
    addUnaryAffine("sqrt_affine_add",  "sqrt",  false, +1.0, UF::Sqrt);
    addUnaryAffine("sqrt_affine_sub",  "sqrt",  true,  -1.0, UF::Sqrt);
    addUnaryAffine("floor_affine_add", "floor", false, +1.0, UF::Floor);
    addUnaryAffine("floor_affine_sub", "floor", true,  -1.0, UF::Floor);
    addUnaryAffine("ceil_affine_add",  "ceil",  false, +1.0, UF::Ceil);
    addUnaryAffine("ceil_affine_sub",  "ceil",  true,  -1.0, UF::Ceil);

    FusionRule sqAffineAdd;
    sqAffineAdd.name  = "sq_affine_add";
    sqAffineAdd.match = [](const ASTNode *node) { return matchSqAffine(node, false); };
    sqAffineAdd.execute = [](const Value *ops, std::size_t n, Value &out,
                             std::pmr::memory_resource *mr) {
        return execSqAffine(ops, n, out, mr, +1.0);
    };
    engine.addFusionRule(std::move(sqAffineAdd));

    FusionRule sqAffineSub;
    sqAffineSub.name  = "sq_affine_sub";
    sqAffineSub.match = [](const ASTNode *node) { return matchSqAffine(node, true); };
    sqAffineSub.execute = [](const Value *ops, std::size_t n, Value &out,
                             std::pmr::memory_resource *mr) {
        return execSqAffine(ops, n, out, mr, -1.0);
    };
    engine.addFusionRule(std::move(sqAffineSub));

    FusionRule sqDiff;
    sqDiff.name  = "sq_diff";
    sqDiff.match = matchSqDiff;
    sqDiff.execute = execSqDiff;
    engine.addFusionRule(std::move(sqDiff));

    FusionRule sqrtSumSq;
    sqrtSumSq.name  = "sqrt_sumsq";
    sqrtSumSq.match = matchSqrtSumSq;
    sqrtSumSq.execute = execSqrtSumSq;
    engine.addFusionRule(std::move(sqrtSumSq));

    FusionRule softThreshold;
    softThreshold.name  = "soft_threshold";
    softThreshold.match = matchSoftThreshold;
    softThreshold.execute = execSoftThreshold;
    engine.addFusionRule(std::move(softThreshold));
}

} // namespace numkit
