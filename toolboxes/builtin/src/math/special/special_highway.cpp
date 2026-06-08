// toolboxes/builtin/src/math/special/special_highway.cpp
//
// Highway dynamic-dispatch special functions (erf, ...). Highway's
// contrib/math has no erf / gamma, so the kernels are PORTED from SLEEF's
// double-precision SIMD library (the math is transcribed into Highway ops;
// SLEEF is neither linked nor vendored). The double-double (dd) arithmetic
// below is the standard Dekker/FMA scheme used by SLEEF's src/common/dd.h;
// the polynomial coefficients are copied verbatim from SLEEF.
//
//   Math kernels ported from SLEEF (commit 7623d6c), src/libm/sleefsimddp.c
//   (xerf_u1) and src/common/dd.h. Boost Software License 1.0 — see
//   third_party/sleef/LICENSE.
//
// erf: the SLEEF u1 kernel for |x| <= 2.5 is reproduced exactly here (dd
// polynomial + 16th-power reciprocal). |x| > 2.5, |x| < 1e-8, and the
// special values (±0, ±Inf, NaN) fall back to the scalar reference
// std::erf — correct everywhere, with the common [1e-8, 2.5] range
// vectorised. The expk-based >2.5 vector branch lands together with
// erfc/gamma (which need expk anyway). Parity vs the scalar reference is
// checked in toolboxes/builtin/tests/simd_parity_test.cpp.

#include <numkit/builtin/math/special/special.hpp>

#include <numkit/value/value.hpp>
#include <numkit/ops/parallel_for.hpp>
#include <numkit/value/error.hpp>

#include "helpers.hpp"

#include <cmath>
#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "math/special/special_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::builtin {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

// ── double-double (dd) arithmetic, FMA path (mirrors SLEEF src/common/dd.h)
// V is a Highway double vector. A dd value is a hi/lo pair (x is the
// rounded value, y the error term). MulAdd(a,b,c)=a*b+c, MulSub(a,b,c)=
// a*b-c, NegMulAdd(a,b,c)=c-a*b.
template <class V>
struct DD { V x, y; };

template <class D, class V>
HWY_INLINE DD<V> dd_make(V x, V y) { (void)D(); return DD<V>{x, y}; }

// ddadd_vd2_vd_vd2 : vd + vd2  (assumes |x| >= |y.x|)
template <class V>
HWY_INLINE DD<V> dd_add_v_dd(V x, DD<V> y)
{
    V s = hn::Add(x, y.x);
    V e = hn::Add(hn::Add(hn::Sub(x, s), y.x), y.y);
    return DD<V>{s, e};
}

// ddadd_vd2_vd2_vd2 : vd2 + vd2  (assumes |x.x| >= |y.x|)
template <class V>
HWY_INLINE DD<V> dd_add_dd_dd(DD<V> x, DD<V> y)
{
    V s = hn::Add(x.x, y.x);
    V e = hn::Add(hn::Add(hn::Add(hn::Sub(x.x, s), y.x), x.y), y.y);
    return DD<V>{s, e};
}

// ddadd2_vd2_vd2_vd : vd2 + vd  (no magnitude assumption)
template <class V>
HWY_INLINE DD<V> dd_add2_dd_v(DD<V> x, V y)
{
    V s = hn::Add(x.x, y);
    V v = hn::Sub(s, x.x);
    V w = hn::Add(hn::Sub(x.x, hn::Sub(s, v)), hn::Sub(y, v));
    return DD<V>{s, hn::Add(w, x.y)};
}

// ddmul_vd2_vd2_vd : vd2 * vd
template <class V>
HWY_INLINE DD<V> dd_mul_dd_v(DD<V> x, V y)
{
    V s = hn::Mul(x.x, y);
    V e = hn::MulAdd(x.y, y, hn::MulSub(x.x, y, s));
    return DD<V>{s, e};
}

// ddsqu_vd2_vd2 : vd2 ^ 2
template <class V>
HWY_INLINE DD<V> dd_squ(DD<V> x)
{
    V s = hn::Mul(x.x, x.x);
    V e = hn::MulAdd(hn::Add(x.x, x.x), x.y, hn::MulSub(x.x, x.x, s));
    return DD<V>{s, e};
}

// ddrec_vd2_vd2 : 1 / vd2
template <class D, class V>
HWY_INLINE DD<V> dd_rec(D d, DD<V> a)
{
    V one = hn::Set(d, 1.0);
    V s = hn::Div(one, a.x);
    V e = hn::Mul(s, hn::NegMulAdd(a.y, s, hn::NegMulAdd(a.x, s, one)));
    return DD<V>{s, e};
}

// ddmla_vd2_vd_vd2_vd2 : z + y*x   (used by poly*dd)
template <class V>
HWY_INLINE DD<V> dd_mla_v(V x, DD<V> y, DD<V> z)
{
    return dd_add_dd_dd(z, dd_mul_dd_v(y, x));
}

// poly2dd / poly2dd_b / poly4dd (SLEEF sleefsimddp.c:2619-2622)
template <class D, class V>
HWY_INLINE DD<V> poly2dd(D d, V x, V c1, DD<V> c0)
{
    return dd_mla_v(x, DD<V>{c1, hn::Zero(d)}, c0);
}
template <class V>
HWY_INLINE DD<V> poly2dd_b(V x, DD<V> c1, DD<V> c0)
{
    return dd_mla_v(x, c1, c0);
}
template <class D, class V>
HWY_INLINE DD<V> poly4dd(D d, V x, V c3, DD<V> c2, DD<V> c1, DD<V> c0)
{
    return dd_mla_v(hn::Mul(x, x), poly2dd(d, x, c3, c2), poly2dd_b(x, c1, c0));
}

// ── Estrin polynomial helpers (mirror src/common/estrin.h). MLA == MulAdd.
template <class V> HWY_INLINE V P2(V x, V c1, V c0) { return hn::MulAdd(x, c1, c0); }
template <class V> HWY_INLINE V P4(V x, V x2, V c3, V c2, V c1, V c0)
{ return hn::MulAdd(x2, hn::MulAdd(x, c3, c2), hn::MulAdd(x, c1, c0)); }
template <class V> HWY_INLINE V P5(V x, V x2, V x4, V c4, V c3, V c2, V c1, V c0)
{ return hn::MulAdd(x4, c4, P4(x, x2, c3, c2, c1, c0)); }
template <class V> HWY_INLINE V P8(V x, V x2, V x4, V c7, V c6, V c5, V c4, V c3, V c2, V c1, V c0)
{ return hn::MulAdd(x4, P4(x, x2, c7, c6, c5, c4), P4(x, x2, c3, c2, c1, c0)); }
template <class V> HWY_INLINE V P16(V x, V x2, V x4, V x8,
        V cf, V ce, V cd, V cc, V cb, V ca, V c9, V c8,
        V c7, V c6, V c5, V c4, V c3, V c2, V c1, V c0)
{ return hn::MulAdd(x8, P8(x, x2, x4, cf, ce, cd, cc, cb, ca, c9, c8),
                        P8(x, x2, x4, c7, c6, c5, c4, c3, c2, c1, c0)); }
template <class V> HWY_INLINE V P10(V x, V x2, V x4, V x8,
        V c9, V c8, V c7, V c6, V c5, V c4, V c3, V c2, V c1, V c0)
{ return hn::MulAdd(x8, P2(x, c9, c8), P8(x, x2, x4, c7, c6, c5, c4, c3, c2, c1, c0)); }

// ddnormalize_vd2_vd2
template <class V>
HWY_INLINE DD<V> dd_normalize(DD<V> t)
{
    V s = hn::Add(t.x, t.y);
    return DD<V>{s, hn::Add(hn::Sub(t.x, s), t.y)};
}

// ldexp(u, q): u * 2^q, single-step (q is integer-valued, modest range here).
template <class D, class V>
HWY_INLINE V ldexpk(D d, V u, V qd)
{
    const hn::RebindToSigned<D> di;
    auto qi = hn::ConvertTo(di, qd);
    auto bits = hn::ShiftLeft<52>(hn::Add(qi, hn::Set(di, 1023)));
    return hn::Mul(u, hn::BitCast(d, bits));
}

// expk : exp of a double-double argument (SLEEF sleefsimddp.c:1561).
template <class D, class V>
HWY_INLINE V expk(D d, DD<V> a)
{
    const double R_LN2 = 1.442695040888963407359924681001892137426645954152985934135449406931;
    const double L2U   = 0.69314718055966295651160180568695068359375;
    const double L2L   = 0.28235290563031577122588448175013436025525412068e-12;

    V u  = hn::Mul(hn::Add(a.x, a.y), hn::Set(d, R_LN2));
    V dq = hn::Round(u);

    DD<V> s = dd_add2_dd_v(a, hn::Mul(dq, hn::Set(d, -L2U)));
    s = dd_add2_dd_v(s, hn::Mul(dq, hn::Set(d, -L2L)));
    s = dd_normalize(s);

    V s2 = hn::Mul(s.x, s.x), s4 = hn::Mul(s2, s2), s8 = hn::Mul(s4, s4);
    u = P10(s.x, s2, s4, s8,
            hn::Set(d, 2.51069683420950419527139e-08),
            hn::Set(d, 2.76286166770270649116855e-07),
            hn::Set(d, 2.75572496725023574143864e-06),
            hn::Set(d, 2.48014973989819794114153e-05),
            hn::Set(d, 0.000198412698809069797676111),
            hn::Set(d, 0.0013888888939977128960529),
            hn::Set(d, 0.00833333333332371417601081),
            hn::Set(d, 0.0416666666665409524128449),
            hn::Set(d, 0.166666666666666740681535),
            hn::Set(d, 0.500000000000000999200722));

    DD<V> t = dd_add_v_dd(hn::Set(d, 1.0), s);
    t = dd_add_dd_dd(t, dd_mul_dd_v(dd_squ(s), u));

    u = hn::Add(t.x, t.y);
    u = ldexpk(d, u, dq);

    // exp underflows to 0 for very negative arguments.
    u = hn::IfThenZeroElse(hn::Lt(a.x, hn::Set(d, -1000.0)), u);
    return u;
}

void ErfLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    using V = decltype(hn::Zero(d));

    // SLEEF xerf_u1 — two coefficient sets, selected per lane by |x| <= 2.5.
    // Index 0..4 are the POLY21 high terms (d4..d0); 5..20 are cf..c0. All
    // copied verbatim from sleefsimddp.c (the |x|<=2.5 set and the >2.5 set).
    static const double A_lo[21] = {
        -0.2083271002525222097e-14, +0.7151909970790897009e-13, -0.1162238220110999364e-11,
        +0.1186474230821585259e-10, -0.8499973178354613440e-10, +0.4507647462598841629e-9,
        -0.1808044474288848915e-8,  +0.5435081826716212389e-8,  -0.1143939895758628484e-7,
        +0.1215442362680889243e-7,  +0.1669878756181250355e-7,  -0.9808074602255194288e-7,
        +0.1389000557865837204e-6,  +0.2945514529987331866e-6,  -0.1842918273003998283e-5,
        +0.3417987836115362136e-5,  +0.3860236356493129101e-5,  -0.3309403072749947546e-4,
        +0.1060862922597579532e-3,  +0.2323253155213076174e-3,  +0.1490149719145544729e-3};
    static const double A_hi[21] = {
        -0.4024015130752621932e-18, +0.3847193332817048172e-16, -0.1749316241455644088e-14,
        +0.5029618322872872715e-13, -0.1025221466851463164e-11, +0.1573695559331945583e-10,
        -0.1884658558040203709e-9,  +0.1798167853032159309e-8,  -0.1380745342355033142e-7,
        +0.8525705726469103499e-7,  -0.4160448058101303405e-6,  +0.1517272660008588485e-5,
        -0.3341634127317201697e-5,  -0.2515023395879724513e-5,  +0.6539731269664907554e-4,
        -0.3551065097428388658e-3,  +0.1210736097958368864e-2,  -0.2605566912579998680e-2,
        +0.1252823202436093193e-2,  +0.1820191395263313222e-1,  -0.1021557155453465954e+0};

    const V one    = hn::Set(d, 1.0);
    const V negOne = hn::Set(d, -1.0);
    const V c2spi  = hn::Set(d, 1.12837916709551262756245475959); // 2/sqrt(pi)

    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        V a    = hn::LoadU(d, in + i);
        V absx = hn::Abs(a);
        // Clamp the kernel input to 6 so x^16 can't overflow; |x|>=6 lanes
        // are overwritten with 1 below (erf(>=6) == 1 to double precision).
        V x  = hn::Min(absx, hn::Set(d, 6.0));
        V x2 = hn::Mul(x, x), x4 = hn::Mul(x2, x2);
        V x8 = hn::Mul(x4, x4), x16 = hn::Mul(x8, x8);

        auto o25 = hn::Le(x, hn::Set(d, 2.5));
        V c[21];
        for (int k = 0; k < 21; ++k)
            c[k] = hn::IfThenElse(o25, hn::Set(d, A_lo[k]), hn::Set(d, A_hi[k]));

        V t = hn::MulAdd(x16, P5(x, x2, x4, c[0], c[1], c[2], c[3], c[4]),
                              P16(x, x2, x4, x8,
                                  c[5], c[6], c[7], c[8], c[9], c[10], c[11], c[12],
                                  c[13], c[14], c[15], c[16], c[17], c[18], c[19], c[20]));

        DD<V> dc2 = DD<V>{hn::IfThenElse(o25, hn::Set(d, 0.0092877958392275604405), hn::Set(d, -0.63691044383641748361)),
                          hn::IfThenElse(o25, hn::Set(d, 7.9287559463961107493e-19), hn::Set(d, -2.4249477526539431839e-17))};
        DD<V> dc1 = DD<V>{hn::IfThenElse(o25, hn::Set(d, 0.042275531758784692937), hn::Set(d, -1.1282926061803961737)),
                          hn::IfThenElse(o25, hn::Set(d, 1.3785226620501016138e-19), hn::Set(d, -6.2970338860410996505e-17))};
        DD<V> dc0 = DD<V>{hn::IfThenElse(o25, hn::Set(d, 0.07052369794346953491), hn::Set(d, -1.2261313785184804967e-05)),
                          hn::IfThenElse(o25, hn::Set(d, 9.5846628070792092842e-19), hn::Set(d, -5.5329707514490107044e-22))};

        DD<V> tp = poly4dd(d, x, t, dc2, dc1, dc0);

        // |x| <= 2.5 reconstruction: (1 + tp*x)^16, reciprocated.
        DD<V> s2 = dd_add_v_dd(one, dd_mul_dd_v(tp, x));
        s2 = dd_squ(s2); s2 = dd_squ(s2); s2 = dd_squ(s2); s2 = dd_squ(s2);
        s2 = dd_rec(d, s2);
        // |x| > 2.5: exp of the dd polynomial.
        DD<V> t2 = DD<V>{hn::IfThenElse(o25, s2.x, expk(d, tp)),
                         hn::IfThenElse(o25, s2.y, hn::Zero(d))};

        t2 = dd_add2_dd_v(t2, negOne);
        V z = hn::Neg(hn::Add(t2.x, t2.y));
        z = hn::IfThenElse(hn::Lt(absx, hn::Set(d, 1e-8)), hn::Mul(absx, c2spi), z);
        z = hn::IfThenElse(hn::Ge(absx, hn::Set(d, 6.0)), one, z);
        z = hn::IfThenElse(hn::IsInf(a), one, z);
        z = hn::IfThenElse(hn::Eq(a, hn::Zero(d)), hn::Zero(d), z);
        z = hn::CopySign(z, a);
        hn::StoreU(z, d, out + i);
    }
    for (; i < n; ++i) out[i] = std::erf(in[i]);
}

} // namespace HWY_NAMESPACE
} // namespace numkit::builtin
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace numkit::builtin {

HWY_EXPORT(ErfLoop);

Value erf(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex() || x.isScalar() || x.type() != ValueType::DOUBLE)
        return unaryDouble(x, [](double v) { return std::erf(v); }, mr);

    Value r = createLike(x, ValueType::DOUBLE, mr);
    if (x.numel() == 0)
        return r;
    const double *in  = x.doubleData();
    double       *out = r.doubleDataMut();
    numkit::detail::parallel_for(x.numel(), numkit::detail::kTranscendentalThreshold,
        [=](std::size_t s, std::size_t e) {
            HWY_DYNAMIC_DISPATCH(ErfLoop)(in + s, out + s, e - s);
        });
    return r;
}

} // namespace numkit::builtin

#endif // HWY_ONCE
