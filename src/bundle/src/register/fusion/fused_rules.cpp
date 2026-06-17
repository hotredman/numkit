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
//   (affine_add/sub also fuse the implicit-1 forms a.*x±y / x±b.*y → fusedAxpby
//    with coeff 1; negprod handles `leaf - prod`: x - b.*y and c - a.*x.)
//   shift_scale_mul  (x-c).*s / s.*(x-c) → fusedShiftScaleMul(x, c, s)
//   shift_scale_div  (x-c)./d           → fusedShiftScaleDiv(x, c, d)
//   affine_clamp[_min_outer]  max/min(.. <inner> ..) → fusedAffineClamp[MinOuter]
//   abs_affine  abs(<inner>)          → fusedAbsAffine
//   abs_diff    abs(x - y) / abs(x-c) → fusedAbsDiff / fusedAbsAffine
//   {sqrt,floor,ceil,fix,round}_affine  f(<inner>)  → fusedUnaryAffine
//     (fix=trunc, round=half-away via CopySign+Trunc — NOT Highway's round-even)
//   sq_affine   (<inner>).^2          → fusedSqAffine
//   sq_diff     (x-y).^2 / (x-c).^2   → fusedSqDiff / fusedSqAffine
// (<inner> = any affine spelling via matchInner: a.*x±b, a.*x, x±c, -x; sq/abs
//  skip ShiftSub since their _diff rule owns leaf-leaf subtract.)
//   sqrt_sumsq   sqrt(x.^2 + y.^2)   → fusedSqrtSumSq(x, y)
//   soft_threshold  sign(x).*max(0,abs(x)-t) → fusedSoftThreshold(x, t)
//   {exp,expm1,log,log2,log10,sin,cos,tan,tanh,sinh,cosh,atan,asinh,asin,acos,
//    acosh,atanh,log1p}_affine  f(<inner>) → fusedTransAffine. Complex-promoting f
//    decline on the offending range: log* on inner<0, log1p on <-1, acosh on <1,
//    {asin,acos,atanh} on |inner|>1 (per-op produces the complex result). tan
//    mirrors numkit's TanVec + per-block 1e6 fallback (always-real).
//   {sqrt,floor,ceil,exp,…}_div  f(x./d) / f((x-c)./d) → fused{Unary,Trans}ShiftDiv
//     (div is a distinct rounding from scale*x+offset → dedicated kernels)
//   sq_div / abs_div / affine_clamp_div[_min_outer]  (<div-arg>).^2 / abs(...) /
//     max(lo,min(hi,(x-c)./d)) → fused{Sq,Abs,AffineClamp}ShiftDiv
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
        case NodeType::UNARY_OP:      // -leaf (negation is pure + exact)
            return n->strValue == "-" && !n->children.empty() &&
                   isPureLeaf(n->children[0].get());
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

// A real-double array big enough to be worth fusing (the "x" of an idiom).
bool isFusibleArray(const Value &v) {
    return v.type() == ValueType::DOUBLE && !v.isComplex() &&
           v.numel() >= kFusionMinElems;
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

// ---- complex binding -----------------------------------------------------
// Complex fusion is triggered by a COMPLEX array operand; scalar coefficients
// may be real or complex (real → Complex(v,0)). A real array with a complex
// coefficient is NOT fused here (declines → per-op promotes it); the high-value
// case (transform a complex array) is covered. The complex kernels are scalar
// std::complex, so binding only needs to gather Complex scale/offset + the array.

bool isFusibleComplexArray(const Value &v) {
    return v.isComplex() && v.numel() >= kFusionMinElems;
}

// A scalar coefficient → Complex (real-double scalar → (v,0); complex scalar →
// its value). false if not a scalar real-double/complex value.
bool asComplexScalar(const Value &v, Complex &out) {
    if (!v.isScalar()) return false;
    if (v.isComplex()) { out = v.toComplex(); return true; }
    if (v.type() == ValueType::DOUBLE) { out = Complex(v.toScalar(), 0.0); return true; }
    return false;
}

// product's two operands → (Complex coeff, complex array x): exactly one scalar
// coefficient (real/complex) and one complex array. Resolves `a.*z` vs `z.*a`.
bool bindAffineProductCx(const Value &c0, const Value &c1, Complex &scale,
                         const Value *&x) {
    if (isFusibleComplexArray(c1) && asComplexScalar(c0, scale)) { x = &c1; return true; }
    if (isFusibleComplexArray(c0) && asComplexScalar(c1, scale)) { x = &c0; return true; }
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

// Shared executor. operands = [c0, c1, leaf]; the matched expression is
// (c0 ⊗ c1) ⊕ leaf, ⊕ carrying `bSign` (+1 add, -1 sub). The trailing leaf is
// either a real-double scalar (→ affine offset `a.*x ± b`) or a same-shape
// real-double array (→ implicit-coefficient-1 axpby `a.*x ± y`). offset =
// bSign*leaf and the y-coefficient bSign*1.0 are both exact (×±1 does not
// round), so each path is bit-identical to its per-op spelling (negation and
// add/sub are the same exact IEEE op, both rounding twice overall).
bool execAffine(const Value *ops, std::size_t n, Value &out,
                std::pmr::memory_resource *mr, double bSign) {
    if (n != 3) return false;
    double scale = 0.0;
    const Value *x = nullptr;
    if (bindAffineProduct(ops[0], ops[1], scale, x)) {
        const Value &leaf = ops[2];
        if (isRealDoubleScalar(leaf)) {                  // a.*x ± b → affine
            out = ops::createLike(*x, ValueType::DOUBLE, mr);
            ops::fusedAffine(x->doubleData(), scale, bSign * leaf.toScalar(),
                             out.doubleDataMut(), x->numel());
            return true;
        }
        if (isFusibleArray(leaf) && leaf.dims() == x->dims()) {  // a.*x ± y → axpby
            out = ops::createLike(*x, ValueType::DOUBLE, mr);
            ops::fusedAxpby(x->doubleData(), scale, leaf.doubleData(), bSign * 1.0,
                            out.doubleDataMut(), x->numel());
            return true;
        }
    }
    // complex array → complex affine (a.*z ± b) / axpby (a.*z ± w).
    Complex cscale;
    const Value *cz = nullptr;
    if (bindAffineProductCx(ops[0], ops[1], cscale, cz)) {
        const Value &leaf = ops[2];
        Complex coff;
        if (asComplexScalar(leaf, coff)) {
            out = ops::createLike(*cz, ValueType::COMPLEX, mr);
            ops::fusedAffineCx(cz->complexData(), cscale, bSign < 0 ? -coff : coff,
                               out.complexDataMut(), cz->numel());
            return true;
        }
        if (isFusibleComplexArray(leaf) && leaf.dims() == cz->dims()) {
            out = ops::createLike(*cz, ValueType::COMPLEX, mr);
            ops::fusedAxpbyCx(cz->complexData(), cscale, leaf.complexData(),
                              Complex(bSign, 0.0), out.complexDataMut(), cz->numel());
            return true;
        }
    }
    return false;
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
    if (bindAffineProduct(ops[0], ops[1], a, x) &&
        bindAffineProduct(ops[2], ops[3], b, y) &&
        x->dims() == y->dims()) {                   // element-wise: identical shape
        out = ops::createLike(*x, ValueType::DOUBLE, mr);
        ops::fusedAxpby(x->doubleData(), a, y->doubleData(), bSign * b,
                        out.doubleDataMut(), x->numel());
        return true;
    }
    // complex: a.*z ± b.*w (both products bind to complex arrays).
    Complex ca, cb;
    const Value *cz = nullptr, *cw = nullptr;
    if (bindAffineProductCx(ops[0], ops[1], ca, cz) &&
        bindAffineProductCx(ops[2], ops[3], cb, cw) &&
        cz->dims() == cw->dims()) {
        out = ops::createLike(*cz, ValueType::COMPLEX, mr);
        ops::fusedAxpbyCx(cz->complexData(), ca, cw->complexData(),
                          bSign < 0 ? -cb : cb, out.complexDataMut(), cz->numel());
        return true;
    }
    return false;
}

// ---- axpby implicit-1 (subtractive `leaf - prod`):  x - b.*y / c - a.*x --
// `prod - leaf` is affine_sub's shape (left = product); the reverse `leaf -
// prod` (left = a pure leaf) is unmatched by it, so this rule owns it. The
// trailing product is a.*y; the leading leaf is either a same-shape array
// (→ `x - b.*y` = axpby(x, 1, y, -b)) or a real scalar (→ `c - a.*x`, the
// negated-scale affine fusedAffine(x, -a, c) — the recipe affine_sub defers).
// `prod - prod` stays with axpby_sub (a product is not a pure leaf → declines).
std::optional<std::vector<const ASTNode *>> matchNegProd(const ASTNode *node) {
    const ASTNode *sub = asBinOp(node, "-");
    if (!sub) return std::nullopt;
    const ASTNode *leaf = sub->children[0].get();
    const ASTNode *prod = asPureProduct(sub->children[1].get());
    if (!isPureLeaf(leaf) || !prod) return std::nullopt;
    return std::vector<const ASTNode *>{leaf, prod->children[0].get(),
                                        prod->children[1].get()};
}

// operands = [leaf, pc0, pc1]; expr = leaf - (pc0 ⊗ pc1). Bind the product →
// (coef, p). leaf scalar → `c - a.*x` = fusedAffine(p, -coef, c); leaf same-
// shape array → `x - b.*y` = fusedAxpby(leaf, 1, p, -coef). Both exact: (-a)*x
// == -(a*x), and c + (-(a*x)) == c - a*x (IEEE add commutes bit-for-bit).
bool execNegProd(const Value *ops, std::size_t n, Value &out,
                 std::pmr::memory_resource *mr) {
    if (n != 3) return false;
    double coef = 0.0;
    const Value *p = nullptr;
    if (bindAffineProduct(ops[1], ops[2], coef, p)) {
        const Value &leaf = ops[0];
        if (isRealDoubleScalar(leaf)) {                  // c - a.*x
            out = ops::createLike(*p, ValueType::DOUBLE, mr);
            ops::fusedAffine(p->doubleData(), -coef, leaf.toScalar(),
                             out.doubleDataMut(), p->numel());
            return true;
        }
        if (isFusibleArray(leaf) && leaf.dims() == p->dims()) {  // x - b.*y
            out = ops::createLike(leaf, ValueType::DOUBLE, mr);
            ops::fusedAxpby(leaf.doubleData(), 1.0, p->doubleData(), -coef,
                            out.doubleDataMut(), leaf.numel());
            return true;
        }
    }
    // complex: c - a.*z (scalar leaf) / w - b.*z (array leaf).
    Complex ccoef;
    const Value *cp = nullptr;
    if (bindAffineProductCx(ops[1], ops[2], ccoef, cp)) {
        const Value &leaf = ops[0];
        Complex cleaf;
        if (asComplexScalar(leaf, cleaf)) {              // c - a.*z = -a.*z + c
            out = ops::createLike(*cp, ValueType::COMPLEX, mr);
            ops::fusedAffineCx(cp->complexData(), -ccoef, cleaf,
                               out.complexDataMut(), cp->numel());
            return true;
        }
        if (isFusibleComplexArray(leaf) && leaf.dims() == cp->dims()) {  // w - b.*z
            out = ops::createLike(leaf, ValueType::COMPLEX, mr);
            ops::fusedAxpbyCx(leaf.complexData(), Complex(1.0, 0.0),
                              cp->complexData(), -ccoef, out.complexDataMut(),
                              leaf.numel());
            return true;
        }
    }
    return false;
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
    // real path: A a real-double array, B/s real scalars.
    if (A.type() == ValueType::DOUBLE && !A.isComplex() &&
        A.numel() >= kFusionMinElems &&
        isRealDoubleScalar(B) && isRealDoubleScalar(s)) {
        out = ops::createLike(A, ValueType::DOUBLE, mr);
        if (isDiv)
            ops::fusedShiftScaleDiv(A.doubleData(), B.toScalar(), s.toScalar(),
                                    out.doubleDataMut(), A.numel());
        else
            ops::fusedShiftScaleMul(A.doubleData(), B.toScalar(), s.toScalar(),
                                    out.doubleDataMut(), A.numel());
        return true;
    }
    // complex: (z - c) .* s / (z - c) ./ d, z complex array, c/s/d real or complex.
    Complex cB, cs;
    if (isFusibleComplexArray(A) && asComplexScalar(B, cB) && asComplexScalar(s, cs)) {
        out = ops::createLike(A, ValueType::COMPLEX, mr);
        if (isDiv)
            ops::fusedShiftScaleDivCx(A.complexData(), cB, cs, out.complexDataMut(),
                                      A.numel());
        else
            ops::fusedShiftScaleMulCx(A.complexData(), cB, cs, out.complexDataMut(),
                                      A.numel());
        return true;
    }
    return false;
}

// Forward declarations of the shared inner-affine facility (defined below, after
// the matchers it reuses). `enum class InnerKind;` is a complete type (fixed
// underlying int), so these by-value-param signatures are valid here; the
// enumerators are only needed in registerFusionRules. Used by the f(inner)
// families that follow (affine-clamp, abs, sq, unary, transcendental).
enum class InnerKind;
std::optional<std::vector<const ASTNode *>> matchInner(const ASTNode *node,
                                                       InnerKind kind);
bool bindInner(const Value *ops, std::size_t n, InnerKind kind, double &scale,
               double &offset, const Value *&x);
bool bindInnerCx(const Value *ops, std::size_t n, InnerKind kind, Complex &scale,
                 Complex &offset, const Value *&x);

// ---- affine-clamp:  max(lo, min(hi, <inner>)) --------------------------
// Normalize-then-saturate in one pass — reuses fusedAffineClamp with bound
// scale/offset (plain clamp = the a=1,b=0 special case). The inner affine is
// detected by reusing matchAffineAdd/matchAffineSub.

// operands = [lo, hi, <inner-ops>] — lo/hi FIRST (fixed positions) so the
// variable-arity inner (matchInner) follows. expr = clamp(<inner>) to [lo,hi].
std::optional<std::vector<const ASTNode *>>
matchAffineClamp(const ASTNode *node, InnerKind kind, bool minOuter) {
    auto cs = peelClamp(node, minOuter);
    if (!cs || !isPureLeaf(cs->lo) || !isPureLeaf(cs->hi)) return std::nullopt;
    auto inner = matchInner(cs->inner, kind);
    if (!inner) return std::nullopt;
    std::vector<const ASTNode *> ops{cs->lo, cs->hi};
    ops.insert(ops.end(), inner->begin(), inner->end());
    return ops;                               // [lo, hi, <inner-ops>]
}

// Same bit-exactness as affine (the inner) composed with the clamp kernel's
// fmin/fmax (validated against per-op min/max by the plain-clamp rule).
bool execAffineClamp(const Value *ops, std::size_t n, Value &out,
                     std::pmr::memory_resource *mr, InnerKind kind, bool minOuter) {
    if (n < 3) return false;                  // lo, hi, + >=1 inner operand
    const Value &lo = ops[0], &hi = ops[1];
    if (!isRealDoubleScalar(lo) || !isRealDoubleScalar(hi)) return false;
    double scale = 0.0, offset = 0.0;
    const Value *x = nullptr;
    if (!bindInner(&ops[2], n - 2, kind, scale, offset, x)) return false;

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

// abs of any affine inner (product/shift/neg), via matchInner on abs's argument.
std::optional<std::vector<const ASTNode *>> matchAbsAffine(const ASTNode *node,
                                                           InnerKind kind) {
    const ASTNode *arg = absArg(node);
    if (!arg) return std::nullopt;
    return matchInner(arg, kind);
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

// abs(<inner>) → |scale*x + offset|, inner decoded by bindInner.
bool execAbsAffine(const Value *ops, std::size_t n, Value &out,
                   std::pmr::memory_resource *mr, InnerKind kind) {
    double scale = 0.0, offset = 0.0;
    const Value *x = nullptr;
    if (bindInner(ops, n, kind, scale, offset, x)) {
        out = ops::createLike(*x, ValueType::DOUBLE, mr);
        ops::fusedAbsAffine(x->doubleData(), scale, offset, out.doubleDataMut(),
                            x->numel());
        return true;
    }
    Complex cscale, coffset;
    const Value *cx = nullptr;
    if (bindInnerCx(ops, n, kind, cscale, coffset, cx)) {  // |a.*z+b| → real out
        out = ops::createLike(*cx, ValueType::DOUBLE, mr);
        ops::fusedAbsAffineCx(cx->complexData(), cscale, coffset,
                              out.doubleDataMut(), cx->numel());
        return true;
    }
    return false;
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
    // complex |z - w| for two complex arrays → real magnitude (array-scalar
    // complex declines: |z-c| can't reuse a scale*x kernel w/o a spurious mul-by-1).
    if (isFusibleComplexArray(A) && isFusibleComplexArray(B) && A.dims() == B.dims()) {
        out = ops::createLike(A, ValueType::DOUBLE, mr);
        ops::fusedAbsDiffCx(A.complexData(), B.complexData(), out.doubleDataMut(),
                            A.numel());
        return true;
    }
    return false;
}

// ---- shared inner-affine: the argument of f(<inner>) reduced to scale*x+offset
// Lets every f(inner) idiom (unary, transcendental, …) cover the full spread of
// affine spellings — product `a.*x`, product±leaf `a.*x±b`, and bare shift
// `x±c` — through one matcher + one decoder, instead of a rule per spelling.
// Each kind is structurally disjoint (the matcher distinguishes product vs leaf
// children), so a given inner node matches at most one kind.
enum class InnerKind { ProductAdd, ProductSub, Product, ShiftAdd, ShiftSub, NegLeaf };

std::optional<std::vector<const ASTNode *>> matchInner(const ASTNode *node,
                                                       InnerKind kind) {
    switch (kind) {
        case InnerKind::ProductAdd: return matchAffineAdd(node);     // a.*x+b / b+a.*x
        case InnerKind::ProductSub: return matchAffineSub(node);     // a.*x-b
        case InnerKind::Product: {                                   // a.*x (no offset)
            const ASTNode *p = asPureProduct(node);
            if (!p) return std::nullopt;
            return std::vector<const ASTNode *>{p->children[0].get(),
                                                p->children[1].get()};
        }
        case InnerKind::ShiftAdd: {                                  // x+c / c+x
            const ASTNode *a = asBinOp(node, "+");
            if (!a || !isPureLeaf(a->children[0].get()) ||
                !isPureLeaf(a->children[1].get()))
                return std::nullopt;
            return std::vector<const ASTNode *>{a->children[0].get(),
                                                a->children[1].get()};
        }
        case InnerKind::ShiftSub: {                                  // x-c / c-x
            const ASTNode *s = asPureSub(node);
            if (!s) return std::nullopt;
            return std::vector<const ASTNode *>{s->children[0].get(),
                                                s->children[1].get()};
        }
        case InnerKind::NegLeaf: {                                   // -x (unary minus)
            if (node->type != NodeType::UNARY_OP || node->strValue != "-" ||
                node->children.size() != 1 ||
                !isPureLeaf(node->children[0].get()))
                return std::nullopt;
            return std::vector<const ASTNode *>{node->children[0].get()};
        }
    }
    return std::nullopt;
}

// Decode evaluated operands for `kind` into (scale, offset, x). Every mapping is
// bit-exact with the per-op spelling: ×1/+0 are exact, ±c is exact negation,
// and `c-x` → scale -1 reproduces `c - x` exactly (negation + commute).
bool bindInner(const Value *ops, std::size_t n, InnerKind kind, double &scale,
               double &offset, const Value *&x) {
    switch (kind) {
        case InnerKind::ProductAdd:
        case InnerKind::ProductSub: {
            if (n != 3 || !isRealDoubleScalar(ops[2])) return false;
            if (!bindAffineProduct(ops[0], ops[1], scale, x)) return false;
            offset = (kind == InnerKind::ProductSub ? -1.0 : 1.0) * ops[2].toScalar();
            return true;
        }
        case InnerKind::Product: {
            if (n != 2 || !bindAffineProduct(ops[0], ops[1], scale, x)) return false;
            offset = 0.0;
            return true;
        }
        case InnerKind::ShiftAdd: {
            if (n != 2) return false;
            const Value &u = ops[0], &v = ops[1];
            if (isFusibleArray(u) && isRealDoubleScalar(v)) { x = &u; scale = 1.0; offset = v.toScalar(); return true; }
            if (isFusibleArray(v) && isRealDoubleScalar(u)) { x = &v; scale = 1.0; offset = u.toScalar(); return true; }
            return false;
        }
        case InnerKind::ShiftSub: {
            if (n != 2) return false;
            const Value &u = ops[0], &v = ops[1];  // expr = u - v
            if (isFusibleArray(u) && isRealDoubleScalar(v)) { x = &u; scale = 1.0;  offset = -v.toScalar(); return true; }
            if (isFusibleArray(v) && isRealDoubleScalar(u)) { x = &v; scale = -1.0; offset = u.toScalar();  return true; }
            return false;
        }
        case InnerKind::NegLeaf: {                  // -x → scale -1, offset 0
            if (n != 1 || !isFusibleArray(ops[0])) return false;
            x = &ops[0]; scale = -1.0; offset = 0.0;
            return true;
        }
    }
    return false;
}

// Complex counterpart of bindInner, used by the f(inner) families when the array
// operand is complex. ONLY the product-coefficient kinds (ProductAdd/Sub/Product)
// are supported: there the per-op genuinely multiplies x by the complex
// coefficient, so the kernel's scale*x+offset matches bit-for-bit. The shift/neg
// kinds (scale ±1) are declined — multiplying a complex x by (±1+0i) is a complex
// MULTIPLY, and 0*Inf=NaN would diverge from the per-op bare add/sub/negate on a
// non-finite x. The offset add is bit-exact (t-b == t+(-b)); the Product +0 only
// flips a -0 component, which isequaln treats as equal.
bool bindInnerCx(const Value *ops, std::size_t n, InnerKind kind, Complex &scale,
                 Complex &offset, const Value *&x) {
    switch (kind) {
        case InnerKind::ProductAdd:
        case InnerKind::ProductSub: {
            if (n != 3) return false;
            Complex off;
            if (!asComplexScalar(ops[2], off)) return false;
            if (!bindAffineProductCx(ops[0], ops[1], scale, x)) return false;
            offset = (kind == InnerKind::ProductSub) ? -off : off;
            return true;
        }
        case InnerKind::Product: {
            if (n != 2 || !bindAffineProductCx(ops[0], ops[1], scale, x)) return false;
            offset = Complex(0.0, 0.0);
            return true;
        }
        default:
            return false;  // shift/neg: decline → per-op (see note above)
    }
}

// All InnerKinds, for registering f(inner) rules across every affine spelling.
constexpr InnerKind kAllInnerKinds[] = {
    InnerKind::ProductAdd, InnerKind::ProductSub, InnerKind::Product,
    InnerKind::ShiftAdd, InnerKind::ShiftSub, InnerKind::NegLeaf};

// Same minus ShiftSub — for the sq/abs families, whose `_diff` rule already owns
// the leaf-leaf subtract `(A-B)` (covering BOTH two arrays and array-scalar). If
// sq/abs also took ShiftSub it would structurally match `(x-y).^2`/`abs(x-y)`
// then decline at runtime (two arrays, no scalar), and first-match-declines
// would block the `_diff` rule. So they skip it and stay disjoint.
constexpr InnerKind kInnerKindsNoShiftSub[] = {
    InnerKind::ProductAdd, InnerKind::ProductSub, InnerKind::Product,
    InnerKind::ShiftAdd, InnerKind::NegLeaf};

// ---- div-inner: f(x./d) / f((x-c)./d). A separate facility from the affine
// InnerKinds — x./d is NOT scale*x+offset bit-exactly (1/d would round), so it
// routes to the dedicated fusedUnaryShiftDiv / fusedTransShiftDiv kernels.

// `(A-B)./d` → [A,B,d]; `x./d` → [x,d]. `/` accepted (scalar d ≡ `./`).
std::optional<std::vector<const ASTNode *>> matchDivArg(const ASTNode *node) {
    const ASTNode *div = asBinOp(node, "./");
    if (!div) div = asBinOp(node, "/");
    if (!div) return std::nullopt;
    const ASTNode *dd = div->children[1].get();
    if (!isPureLeaf(dd)) return std::nullopt;
    const ASTNode *left = div->children[0].get();
    if (const ASTNode *s = asPureSub(left))
        return std::vector<const ASTNode *>{s->children[0].get(),
                                            s->children[1].get(), dd};
    if (isPureLeaf(left))
        return std::vector<const ASTNode *>{left, dd};
    return std::nullopt;
}

// Decode → (sub, div, x) for the (x-sub)/div kernel. `(c-x)/d` → (x-c)/(-d)
// (exact: sign moves through the division).
bool bindDivInner(const Value *ops, std::size_t n, double &sub, double &div,
                  const Value *&x) {
    if (n == 3) {
        const Value &A = ops[0], &B = ops[1], &d = ops[2];
        if (!isRealDoubleScalar(d)) return false;
        if (isFusibleArray(A) && isRealDoubleScalar(B)) { x = &A; sub = B.toScalar(); div = d.toScalar();  return true; }
        if (isFusibleArray(B) && isRealDoubleScalar(A)) { x = &B; sub = A.toScalar(); div = -d.toScalar(); return true; }
        return false;
    }
    if (n == 2) {
        const Value &X = ops[0], &d = ops[1];
        if (isFusibleArray(X) && isRealDoubleScalar(d)) { x = &X; sub = 0.0; div = d.toScalar(); return true; }
        return false;
    }
    return false;
}

// Complex counterpart of bindDivInner. Only the array-on-the-left forms (z./d,
// (z-c)./d) are taken; the reversed (c-z)./d is declined for complex (it would
// rely on complex-division negation symmetry — per-op handles it). The subtract
// and divide are genuine (no spurious mul-by-1), so they mirror per-op exactly.
bool bindDivInnerCx(const Value *ops, std::size_t n, Complex &sub, Complex &div,
                    const Value *&x) {
    if (n == 3) {
        const Value &A = ops[0], &B = ops[1], &d = ops[2];
        Complex cb, cd;
        if (isFusibleComplexArray(A) && asComplexScalar(B, cb) &&
            asComplexScalar(d, cd)) {
            x = &A; sub = cb; div = cd; return true;
        }
        return false;
    }
    if (n == 2) {
        const Value &X = ops[0], &d = ops[1];
        Complex cd;
        if (isFusibleComplexArray(X) && asComplexScalar(d, cd)) {
            x = &X; sub = Complex(0.0, 0.0); div = cd; return true;
        }
        return false;
    }
    return false;
}

// Any (x[i]-sub)/div < 0 — the decline predicate for sqrt/log of a div-inner.
bool shiftDivAnyNegative(const double *x, double sub, double div, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
        if ((x[i] - sub) / div < 0.0) return true;
    return false;
}

// max(lo,min(hi,(x-c)./d)) / min(hi,max(lo,x./d)) — clamp of a divide-inner,
// the canonical rescale-then-saturate `max(0,min(1,(x-lo)./range))`. Peel the
// clamp, match the inner as a div-arg → ops = [lo, hi, <div-arg-ops>]. matchInner
// rejects `./`, so the affine-clamp rules decline it and these own it.
std::optional<std::vector<const ASTNode *>>
matchAffineClampDiv(const ASTNode *node, bool minOuter) {
    auto cs = peelClamp(node, minOuter);
    if (!cs || !isPureLeaf(cs->lo) || !isPureLeaf(cs->hi)) return std::nullopt;
    auto inner = matchDivArg(cs->inner);
    if (!inner) return std::nullopt;
    std::vector<const ASTNode *> ops{cs->lo, cs->hi};
    ops.insert(ops.end(), inner->begin(), inner->end());
    return ops;                                // [lo, hi, <div-arg-ops>]
}

bool execAffineClampDiv(const Value *ops, std::size_t n, Value &out,
                        std::pmr::memory_resource *mr, bool minOuter) {
    if (n < 3) return false;                   // lo, hi, + >=1 div-arg operand
    const Value &lo = ops[0], &hi = ops[1];
    if (!isRealDoubleScalar(lo) || !isRealDoubleScalar(hi)) return false;
    double sub = 0.0, div = 0.0;
    const Value *x = nullptr;
    if (!bindDivInner(&ops[2], n - 2, sub, div, x)) return false;
    out = ops::createLike(*x, ValueType::DOUBLE, mr);
    auto kernel = minOuter ? ops::fusedAffineClampMinOuterShiftDiv
                           : ops::fusedAffineClampShiftDiv;
    kernel(x->doubleData(), sub, div, lo.toScalar(), hi.toScalar(),
           out.doubleDataMut(), x->numel());
    return true;
}

// ---- unary-affine:  f(<inner>),  f ∈ {sqrt, floor, ceil} ---------------
// A unary whose SIMD form is bit-identical to libm (sqrt correctly-rounded,
// floor/ceil exact). exp/log/sin… are NOT here — Highway's polynomial differs
// from libm by a few ULP (the transcendental kernel handles those separately).

// fname(arg) single-arg call → arg node, else null.
const ASTNode *unaryCallArg(const ASTNode *node, const char *fname) {
    const ASTNode *fn = calleeName(node);
    if (!fn || fn->strValue != fname || node->children.size() != 2) return nullptr;
    return node->children[1].get();
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
                     std::pmr::memory_resource *mr, InnerKind kind,
                     numkit::ops::UnaryAffineFn fn) {
    double scale = 0.0, offset = 0.0;
    const Value *x = nullptr;
    if (bindInner(ops, n, kind, scale, offset, x)) {
        const std::size_t N = x->numel();
        if (fn == numkit::ops::UnaryAffineFn::Sqrt &&
            affineAnyNegative(x->doubleData(), scale, offset, N))
            return false;  // negative real → complex per-op path
        out = ops::createLike(*x, ValueType::DOUBLE, mr);
        ops::fusedUnaryAffine(x->doubleData(), scale, offset, fn,
                              out.doubleDataMut(), N);
        return true;
    }
    // complex array: only sqrt (std::sqrt(z), total — no domain decline);
    // floor/ceil/fix/round on complex are declined → per-op.
    if (fn == numkit::ops::UnaryAffineFn::Sqrt) {
        Complex cscale, coffset;
        const Value *cx = nullptr;
        if (bindInnerCx(ops, n, kind, cscale, coffset, cx)) {
            out = ops::createLike(*cx, ValueType::COMPLEX, mr);
            ops::fusedSqrtAffineCx(cx->complexData(), cscale, coffset,
                                   out.complexDataMut(), cx->numel());
            return true;
        }
    }
    return false;
}

// f((x-sub)/div) for f ∈ {sqrt, floor, ceil}. sqrt declines on a negative inner.
bool execUnaryDiv(const Value *ops, std::size_t n, Value &out,
                  std::pmr::memory_resource *mr, numkit::ops::UnaryAffineFn fn) {
    double sub = 0.0, div = 0.0;
    const Value *x = nullptr;
    if (bindDivInner(ops, n, sub, div, x)) {
        const std::size_t N = x->numel();
        if (fn == numkit::ops::UnaryAffineFn::Sqrt &&
            shiftDivAnyNegative(x->doubleData(), sub, div, N))
            return false;
        out = ops::createLike(*x, ValueType::DOUBLE, mr);
        ops::fusedUnaryShiftDiv(x->doubleData(), sub, div, fn,
                                out.doubleDataMut(), N);
        return true;
    }
    if (fn == numkit::ops::UnaryAffineFn::Sqrt) {     // complex sqrt(z./d / (z-c)./d)
        Complex csub, cdiv;
        const Value *cx = nullptr;
        if (bindDivInnerCx(ops, n, csub, cdiv, cx)) {
            out = ops::createLike(*cx, ValueType::COMPLEX, mr);
            ops::fusedSqrtShiftDivCx(cx->complexData(), csub, cdiv,
                                     out.complexDataMut(), cx->numel());
            return true;
        }
    }
    return false;
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

// (<inner>).^2 — square of any affine inner (product/shift/neg), via matchInner.
std::optional<std::vector<const ASTNode *>> matchSqAffine(const ASTNode *node,
                                                          InnerKind kind) {
    const ASTNode *base = asSquare(node);
    if (!base) return std::nullopt;
    return matchInner(base, kind);
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
                  std::pmr::memory_resource *mr, InnerKind kind) {
    double scale = 0.0, offset = 0.0;
    const Value *x = nullptr;
    if (bindInner(ops, n, kind, scale, offset, x)) {
        out = ops::createLike(*x, ValueType::DOUBLE, mr);
        ops::fusedSqAffine(x->doubleData(), scale, offset, out.doubleDataMut(),
                           x->numel());
        return true;
    }
    Complex cscale, coffset;
    const Value *cx = nullptr;
    if (bindInnerCx(ops, n, kind, cscale, coffset, cx)) {     // (a.*z ± b).^2
        out = ops::createLike(*cx, ValueType::COMPLEX, mr);
        ops::fusedSqAffineCx(cx->complexData(), cscale, coffset,
                             out.complexDataMut(), cx->numel());
        return true;
    }
    return false;
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
    // complex (z - w).^2 for two complex arrays (array-scalar declines → per-op:
    // (z-c).^2 can't reuse a scale*x kernel without a spurious mul-by-1).
    if (isFusibleComplexArray(A) && isFusibleComplexArray(B) && A.dims() == B.dims()) {
        out = ops::createLike(A, ValueType::COMPLEX, mr);
        ops::fusedSqDiffCx(A.complexData(), B.complexData(), out.complexDataMut(),
                           A.numel());
        return true;
    }
    return false;
}

// (x./d).^2 / ((x-c)./d).^2 — square of a divide-inner (squared z-score). The
// div-arg (matchDivArg) is structurally disjoint from matchInner (asPureProduct
// rejects `./`), so sq_div never collides with sq_affine/sq_diff.
bool execSqDivInner(const Value *ops, std::size_t n, Value &out,
                    std::pmr::memory_resource *mr) {
    double sub = 0.0, div = 0.0;
    const Value *x = nullptr;
    if (!bindDivInner(ops, n, sub, div, x)) return false;
    out = ops::createLike(*x, ValueType::DOUBLE, mr);
    ops::fusedSqShiftDiv(x->doubleData(), sub, div, out.doubleDataMut(),
                         x->numel());
    return true;
}

// abs(x./d) / abs((x-c)./d) — abs of a divide-inner (the abs counterpart of
// execSqDivInner; defined here, next to it, because both need bindDivInner from
// the div-inner facility above). matchDivArg is disjoint from matchInner /
// asPureSub, so abs_div never collides with abs_affine / abs_diff.
bool execAbsDivInner(const Value *ops, std::size_t n, Value &out,
                     std::pmr::memory_resource *mr) {
    double sub = 0.0, div = 0.0;
    const Value *x = nullptr;
    if (bindDivInner(ops, n, sub, div, x)) {
        out = ops::createLike(*x, ValueType::DOUBLE, mr);
        ops::fusedAbsShiftDiv(x->doubleData(), sub, div, out.doubleDataMut(),
                              x->numel());
        return true;
    }
    Complex csub, cdiv;
    const Value *cx = nullptr;
    if (bindDivInnerCx(ops, n, csub, cdiv, cx)) {   // |z./d| → real out
        out = ops::createLike(*cx, ValueType::DOUBLE, mr);
        ops::fusedAbsShiftDivCx(cx->complexData(), csub, cdiv, out.doubleDataMut(),
                                cx->numel());
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
    if (!isRealDoubleScalar(t)) return false;        // threshold is real
    if (x.type() == ValueType::DOUBLE && !x.isComplex() &&
        x.numel() >= kFusionMinElems) {
        out = ops::createLike(x, ValueType::DOUBLE, mr);
        ops::fusedSoftThreshold(x.doubleData(), t.toScalar(), out.doubleDataMut(),
                                x.numel());
        return true;
    }
    if (isFusibleComplexArray(x)) {                  // sign(z).*max(0,|z|-t) → complex
        out = ops::createLike(x, ValueType::COMPLEX, mr);
        ops::fusedSoftThresholdCx(x.complexData(), t.toScalar(),
                                  out.complexDataMut(), x.numel());
        return true;
    }
    return false;
}

// ---- transcendental-affine:  f(<inner>) ---------------------------------
// Always-real f (exp/expm1/sin/cos/tan/tanh/sinh/cosh/atan/asinh) fuse
// unconditionally; the complex-promoting f decline on the offending range so the
// per-op (complex) path runs. The kernel mirrors numkit's transcendental loop
// for bit-exactness (see fused_trans_affine_highway.cpp).

// True if scalar v is OUTSIDE f's real domain (→ MATLAB complex). Mirrors
// numkit's promotion predicates exactly (same comparisons on the same value, so
// the decline boundary aligns with where the per-op path goes complex). NaN
// fails every comparison → stays "in domain" → f(NaN)=NaN, matching per-op.
bool transOutsideRealDomain(numkit::ops::TransAffineFn fn, double v) {
    using TF = numkit::ops::TransAffineFn;
    switch (fn) {
        case TF::Log: case TF::Log2: case TF::Log10: return v < 0.0;
        case TF::Log1p:  return v < -1.0;
        case TF::Acosh:  return v < 1.0;
        case TF::Asin: case TF::Acos: case TF::Atanh: return v < -1.0 || v > 1.0;
        default: return false;  // exp/expm1/sin/cos/tan/tanh/sinh/cosh/atan/asinh
    }
}

// Whether f promotes to complex anywhere — a fast skip so the common always-real
// transcendentals never scan the array.
bool transDomainRestricted(numkit::ops::TransAffineFn fn) {
    using TF = numkit::ops::TransAffineFn;
    switch (fn) {
        case TF::Log: case TF::Log2: case TF::Log10:
        case TF::Log1p: case TF::Acosh:
        case TF::Asin: case TF::Acos: case TF::Atanh: return true;
        default: return false;
    }
}

// Any scale*x[i]+offset outside f's real domain (the affine the kernel computes).
bool affineAnyOutsideDomain(numkit::ops::TransAffineFn fn, double scale,
                            double offset, const double *x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
        if (transOutsideRealDomain(fn, scale * x[i] + offset)) return true;
    return false;
}

// Any (x[i]-sub)/div outside f's real domain.
bool shiftDivAnyOutsideDomain(numkit::ops::TransAffineFn fn, double sub,
                              double div, const double *x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
        if (transOutsideRealDomain(fn, (x[i] - sub) / div)) return true;
    return false;
}

bool execTransAffine(const Value *ops, std::size_t n, Value &out,
                     std::pmr::memory_resource *mr, InnerKind kind,
                     numkit::ops::TransAffineFn fn) {
    double scale = 0.0, offset = 0.0;
    const Value *x = nullptr;
    if (bindInner(ops, n, kind, scale, offset, x)) {
        if (transDomainRestricted(fn) &&
            affineAnyOutsideDomain(fn, scale, offset, x->doubleData(), x->numel()))
            return false;  // outside real domain → MATLAB complex; per-op handles it
        out = ops::createLike(*x, ValueType::DOUBLE, mr);
        ops::fusedTransAffine(x->doubleData(), scale, offset, fn,
                              out.doubleDataMut(), x->numel());
        return true;
    }
    // complex array: f(a.*z ± b). Complex is total (no domain decline). expm1 is
    // real-only in numkit → declined (per-op).
    if (fn != numkit::ops::TransAffineFn::Expm1) {
        Complex cscale, coffset;
        const Value *cx = nullptr;
        if (bindInnerCx(ops, n, kind, cscale, coffset, cx)) {
            out = ops::createLike(*cx, ValueType::COMPLEX, mr);
            ops::fusedTransAffineCx(cx->complexData(), cscale, coffset, fn,
                                    out.complexDataMut(), cx->numel());
            return true;
        }
    }
    return false;
}

// f((x-sub)/div) for a transcendental f. log/log2/log10 decline on a negative
// inner (MATLAB complex). Mirrors fusedTransAffine's bit-exactness.
bool execTransDiv(const Value *ops, std::size_t n, Value &out,
                  std::pmr::memory_resource *mr, numkit::ops::TransAffineFn fn) {
    double sub = 0.0, div = 0.0;
    const Value *x = nullptr;
    if (bindDivInner(ops, n, sub, div, x)) {
        if (transDomainRestricted(fn) &&
            shiftDivAnyOutsideDomain(fn, sub, div, x->doubleData(), x->numel()))
            return false;
        out = ops::createLike(*x, ValueType::DOUBLE, mr);
        ops::fusedTransShiftDiv(x->doubleData(), sub, div, fn, out.doubleDataMut(),
                                x->numel());
        return true;
    }
    if (fn != numkit::ops::TransAffineFn::Expm1) {     // complex f(z./d / (z-c)./d)
        Complex csub, cdiv;
        const Value *cx = nullptr;
        if (bindDivInnerCx(ops, n, csub, cdiv, cx)) {
            out = ops::createLike(*cx, ValueType::COMPLEX, mr);
            ops::fusedTransShiftDivCx(cx->complexData(), csub, cdiv, fn,
                                      out.complexDataMut(), cx->numel());
            return true;
        }
    }
    return false;
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

    // `leaf - prod`: x - b.*y (implicit-1 axpby) / c - a.*x (negated-scale affine).
    FusionRule negProd;
    negProd.name  = "negprod";
    negProd.match = matchNegProd;
    negProd.execute = execNegProd;
    engine.addFusionRule(std::move(negProd));

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

    // affine-clamp max(lo,min(hi,<inner>)) / min(hi,max(lo,<inner>)): every
    // inner spelling × both clamp orders.
    for (bool minOuter : {false, true}) {
        for (InnerKind kind : kAllInnerKinds) {
            FusionRule r;
            r.name  = minOuter ? "affine_clamp_min_outer" : "affine_clamp";
            r.match = [kind, minOuter](const ASTNode *node) {
                return matchAffineClamp(node, kind, minOuter);
            };
            r.execute = [kind, minOuter](const Value *ops, std::size_t n, Value &out,
                                         std::pmr::memory_resource *mr) {
                return execAffineClamp(ops, n, out, mr, kind, minOuter);
            };
            engine.addFusionRule(std::move(r));
        }
    }

    // affine-clamp of a divide-inner: max(lo,min(hi,(x-c)./d)) / min-outer
    // (rescale-then-saturate). Both clamp orders; the dedicated shift-div kernel.
    for (bool minOuter : {false, true}) {
        FusionRule r;
        r.name  = minOuter ? "affine_clamp_div_min_outer" : "affine_clamp_div";
        r.match = [minOuter](const ASTNode *node) {
            return matchAffineClampDiv(node, minOuter);
        };
        r.execute = [minOuter](const Value *ops, std::size_t n, Value &out,
                               std::pmr::memory_resource *mr) {
            return execAffineClampDiv(ops, n, out, mr, minOuter);
        };
        engine.addFusionRule(std::move(r));
    }

    // abs(<inner>): every inner spelling except ShiftSub (abs_diff owns it).
    for (InnerKind kind : kInnerKindsNoShiftSub) {
        FusionRule r;
        r.name  = "abs_affine";
        r.match = [kind](const ASTNode *node) { return matchAbsAffine(node, kind); };
        r.execute = [kind](const Value *ops, std::size_t n, Value &out,
                           std::pmr::memory_resource *mr) {
            return execAbsAffine(ops, n, out, mr, kind);
        };
        engine.addFusionRule(std::move(r));
    }

    FusionRule absDiff;
    absDiff.name  = "abs_diff";
    absDiff.match = matchAbsDiff;
    absDiff.execute = execAbsDiff;
    engine.addFusionRule(std::move(absDiff));

    // abs(x./d) / abs((x-c)./d) — divide-inner abs (dedicated shift-div kernel).
    FusionRule absDiv;
    absDiv.name  = "abs_div";
    absDiv.match = [](const ASTNode *node)
                   -> std::optional<std::vector<const ASTNode *>> {
        const ASTNode *arg = absArg(node);
        if (!arg) return std::nullopt;
        return matchDivArg(arg);
    };
    absDiv.execute = execAbsDivInner;
    engine.addFusionRule(std::move(absDiv));

    // unary-affine f(<inner>): one rule per (function, inner spelling). The
    // function-id and inner kind are baked into the captured closures, so every
    // affine spelling (a.*x±b, a.*x, x±c) of the argument fuses.
    using UF = numkit::ops::UnaryAffineFn;
    auto addUnaryAffine = [&engine](const char *name, const char *fname, UF fn) {
        for (InnerKind kind : kAllInnerKinds) {
            FusionRule r;
            r.name  = name;
            r.match = [fname, kind](const ASTNode *node)
                      -> std::optional<std::vector<const ASTNode *>> {
                const ASTNode *arg = unaryCallArg(node, fname);
                if (!arg) return std::nullopt;
                return matchInner(arg, kind);
            };
            r.execute = [kind, fn](const Value *ops, std::size_t n, Value &out,
                                   std::pmr::memory_resource *mr) {
                return execUnaryAffine(ops, n, out, mr, kind, fn);
            };
            engine.addFusionRule(std::move(r));
        }
    };
    addUnaryAffine("sqrt_affine",  "sqrt",  UF::Sqrt);
    addUnaryAffine("floor_affine", "floor", UF::Floor);
    addUnaryAffine("ceil_affine",  "ceil",  UF::Ceil);
    addUnaryAffine("fix_affine",   "fix",   UF::Fix);    // trunc toward zero (exact)
    addUnaryAffine("round_affine", "round", UF::Round);  // half-away (CopySign+Trunc)

    // f(x./d) / f((x-c)./d) — the divide-inner variant (separate kernel).
    auto addUnaryDiv = [&engine](const char *name, const char *fname, UF fn) {
        FusionRule r;
        r.name  = name;
        r.match = [fname](const ASTNode *node)
                  -> std::optional<std::vector<const ASTNode *>> {
            const ASTNode *arg = unaryCallArg(node, fname);
            if (!arg) return std::nullopt;
            return matchDivArg(arg);
        };
        r.execute = [fn](const Value *ops, std::size_t n, Value &out,
                         std::pmr::memory_resource *mr) {
            return execUnaryDiv(ops, n, out, mr, fn);
        };
        engine.addFusionRule(std::move(r));
    };
    addUnaryDiv("sqrt_div",  "sqrt",  UF::Sqrt);
    addUnaryDiv("floor_div", "floor", UF::Floor);
    addUnaryDiv("ceil_div",  "ceil",  UF::Ceil);
    addUnaryDiv("fix_div",   "fix",   UF::Fix);
    addUnaryDiv("round_div", "round", UF::Round);

    // (<inner>).^2: every inner spelling except ShiftSub (sq_diff owns it).
    for (InnerKind kind : kInnerKindsNoShiftSub) {
        FusionRule r;
        r.name  = "sq_affine";
        r.match = [kind](const ASTNode *node) { return matchSqAffine(node, kind); };
        r.execute = [kind](const Value *ops, std::size_t n, Value &out,
                           std::pmr::memory_resource *mr) {
            return execSqAffine(ops, n, out, mr, kind);
        };
        engine.addFusionRule(std::move(r));
    }

    FusionRule sqDiff;
    sqDiff.name  = "sq_diff";
    sqDiff.match = matchSqDiff;
    sqDiff.execute = execSqDiff;
    engine.addFusionRule(std::move(sqDiff));

    // (x./d).^2 / ((x-c)./d).^2 — divide-inner square (dedicated shift-div kernel).
    FusionRule sqDiv;
    sqDiv.name  = "sq_div";
    sqDiv.match = [](const ASTNode *node)
                  -> std::optional<std::vector<const ASTNode *>> {
        const ASTNode *base = asSquare(node);
        if (!base) return std::nullopt;
        return matchDivArg(base);
    };
    sqDiv.execute = execSqDivInner;
    engine.addFusionRule(std::move(sqDiv));

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

    // transcendental-affine f(<inner>), f ∈ {exp, expm1} (always-real).
    using TF = numkit::ops::TransAffineFn;
    auto addTransAffine = [&engine](const char *name, const char *fname, TF fn) {
        for (InnerKind kind : kAllInnerKinds) {
            FusionRule r;
            r.name  = name;
            r.match = [fname, kind](const ASTNode *node)
                      -> std::optional<std::vector<const ASTNode *>> {
                const ASTNode *arg = unaryCallArg(node, fname);
                if (!arg) return std::nullopt;
                return matchInner(arg, kind);
            };
            r.execute = [kind, fn](const Value *ops, std::size_t n, Value &out,
                                   std::pmr::memory_resource *mr) {
                return execTransAffine(ops, n, out, mr, kind, fn);
            };
            engine.addFusionRule(std::move(r));
        }
    };
    addTransAffine("exp_affine",   "exp",   TF::Exp);
    addTransAffine("expm1_affine", "expm1", TF::Expm1);
    addTransAffine("log_affine",   "log",   TF::Log);
    addTransAffine("log2_affine",  "log2",  TF::Log2);
    addTransAffine("log10_affine", "log10", TF::Log10);
    addTransAffine("sin_affine",   "sin",   TF::Sin);
    addTransAffine("cos_affine",   "cos",   TF::Cos);
    addTransAffine("tanh_affine",  "tanh",  TF::Tanh);
    addTransAffine("sinh_affine",  "sinh",  TF::Sinh);
    addTransAffine("atan_affine",  "atan",  TF::Atan);
    addTransAffine("asinh_affine", "asinh", TF::Asinh);
    addTransAffine("asin_affine",  "asin",  TF::Asin);   // decline |inner|>1
    addTransAffine("acos_affine",  "acos",  TF::Acos);   // decline |inner|>1
    addTransAffine("acosh_affine", "acosh", TF::Acosh);  // decline inner<1
    addTransAffine("atanh_affine", "atanh", TF::Atanh);  // decline |inner|>1
    addTransAffine("log1p_affine", "log1p", TF::Log1p);  // decline inner<-1
    addTransAffine("cosh_affine",  "cosh",  TF::Cosh);   // always-real (composed)
    addTransAffine("tan_affine",   "tan",   TF::Tan);    // always-real (TanVec mirror)

    // f(x./d) / f((x-c)./d) — divide-inner transcendentals (same f-set).
    auto addTransDiv = [&engine](const char *name, const char *fname, TF fn) {
        FusionRule r;
        r.name  = name;
        r.match = [fname](const ASTNode *node)
                  -> std::optional<std::vector<const ASTNode *>> {
            const ASTNode *arg = unaryCallArg(node, fname);
            if (!arg) return std::nullopt;
            return matchDivArg(arg);
        };
        r.execute = [fn](const Value *ops, std::size_t n, Value &out,
                         std::pmr::memory_resource *mr) {
            return execTransDiv(ops, n, out, mr, fn);
        };
        engine.addFusionRule(std::move(r));
    };
    addTransDiv("exp_div",   "exp",   TF::Exp);
    addTransDiv("expm1_div", "expm1", TF::Expm1);
    addTransDiv("log_div",   "log",   TF::Log);
    addTransDiv("log2_div",  "log2",  TF::Log2);
    addTransDiv("log10_div", "log10", TF::Log10);
    addTransDiv("sin_div",   "sin",   TF::Sin);
    addTransDiv("cos_div",   "cos",   TF::Cos);
    addTransDiv("tanh_div",  "tanh",  TF::Tanh);
    addTransDiv("sinh_div",  "sinh",  TF::Sinh);
    addTransDiv("atan_div",  "atan",  TF::Atan);
    addTransDiv("asinh_div", "asinh", TF::Asinh);
    addTransDiv("asin_div",  "asin",  TF::Asin);
    addTransDiv("acos_div",  "acos",  TF::Acos);
    addTransDiv("acosh_div", "acosh", TF::Acosh);
    addTransDiv("atanh_div", "atanh", TF::Atanh);
    addTransDiv("log1p_div", "log1p", TF::Log1p);
    addTransDiv("cosh_div",  "cosh",  TF::Cosh);
    addTransDiv("tan_div",   "tan",   TF::Tan);
}

} // namespace numkit
