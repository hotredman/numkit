// libs/wavelet/src/filter/wfilters.cpp
//
// Hard-coded scaling filter coefficients h[k] for the orthogonal
// wavelet families covered in pass 1 (haar / db / sym / coif). The
// values follow MATLAB Wavelet Toolbox conventions (Daubechies "low-D"
// version is the analysis filter — it is the time-reverse of the
// synthesis filter Lo_R).
//
// From the synthesis lowpass Lo_R we derive the rest:
//   Lo_D[k] = Lo_R[N-1-k]                  (time reversal)
//   Hi_R[k] = (-1)^k * Lo_R[N-1-k]         (QMF: odd index sign flip on flipped Lo)
//   Hi_D[k] = (-1)^(k+1) * Lo_R[k]         (QMF on the analysis side)
//
// MATLAB sign conventions verified against R2025b's `wfilters('db4')`
// and `wfilters('sym4')` outputs. Norms: sum(Lo) = sqrt(2),
// sum(Hi) = 0, ||Lo||² = 1, ||Hi||² = 1.

#include <numkit/wavelet/filter/wfilters.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace numkit::wavelet {

namespace {

const double SQRT2 = 1.41421356237309504880;
const double INV_SQRT2 = 0.70710678118654752440;

// MATLAB-conventional Lo_R (synthesis scaling filter) for each family.
// Values copied to the precision MATLAB exports (≈ 17 decimals).

// haar / db1
const double LO_R_db1[] = { INV_SQRT2, INV_SQRT2 };

// db2 (Lo_R)
const double LO_R_db2[] = {
    -0.12940952255092145,
     0.22414386804185735,
     0.83651630373746899,
     0.48296291314469025
};

// db3 (Lo_R)
const double LO_R_db3[] = {
     0.035226291882100656,
    -0.085441273882241486,
    -0.13501102001039084,
     0.45987750211933132,
     0.80689150931333875,
     0.33267055295095688
};

// db4 (Lo_R)
const double LO_R_db4[] = {
    -0.010597401784997278,
     0.032883011666982945,
     0.030841381835986965,
    -0.18703481171888114,
    -0.027983769416983849,
     0.63088076792959036,
     0.71484657055254153,
     0.23037781330885523
};

// sym2 (== db2 reversed in some formulations; MATLAB's sym2 is identical to db2)
const double LO_R_sym2[] = {
    -0.12940952255092145,
     0.22414386804185735,
     0.83651630373746899,
     0.48296291314469025
};

// sym4 (Lo_R)
const double LO_R_sym4[] = {
    -0.075765714789273325,
    -0.029635527645999019,
     0.49761866763201545,
     0.80373875180591611,
     0.29785779560527736,
    -0.099219543576847216,
    -0.012603967262037833,
     0.032223100604042702
};

// coif1 (Lo_R)
const double LO_R_coif1[] = {
    -0.015655728135465339,
    -0.072732619512853897,
     0.38486484686420286,
     0.85257202021225542,
     0.33789766245780922,
    -0.072732619512853897
};

struct Spec {
    const char *name;
    const double *Lo_R;
    int len;
};

const Spec kFamilies[] = {
    {"haar",  LO_R_db1, 2},
    {"db1",   LO_R_db1, 2},
    {"db2",   LO_R_db2, 4},
    {"db3",   LO_R_db3, 6},
    {"db4",   LO_R_db4, 8},
    {"sym2",  LO_R_sym2, 4},
    {"sym4",  LO_R_sym4, 8},
    {"coif1", LO_R_coif1, 6},
};

const Spec *findSpec(const std::string &name) {
    for (const auto &s : kFamilies)
        if (name == s.name) return &s;
    return nullptr;
}

Value vecToRow(std::pmr::memory_resource *mr, const std::vector<double> &v) {
    Value r = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), r.doubleDataMut());
    return r;
}

} // anonymous

FilterBank wavelet_filters(const std::string &name) {
    const Spec *s = findSpec(name);
    if (!s)
        throw Error("wfilters: unsupported wavelet name '" + name +
                    "' (try haar, db1..db4, sym2, sym4, coif1)",
                    0, 0, "wfilters", "", "m:wfilters:name");
    const int N = s->len;

    FilterBank fb;
    fb.Lo_R.assign(s->Lo_R, s->Lo_R + N);
    fb.Lo_D.resize(N);
    fb.Hi_R.resize(N);
    fb.Hi_D.resize(N);
    for (int k = 0; k < N; ++k) {
        // analysis lowpass = time-reversed synthesis lowpass
        fb.Lo_D[k] = fb.Lo_R[N - 1 - k];
        // synthesis highpass: QMF, alternating-sign reversal
        const double sgnR = (k % 2 == 0) ? 1.0 : -1.0;
        fb.Hi_R[k] = sgnR * fb.Lo_R[N - 1 - k];
        // analysis highpass: QMF on Lo_R direct
        const double sgnD = (k % 2 == 0) ? -1.0 : 1.0;
        fb.Hi_D[k] = sgnD * fb.Lo_R[k];
    }
    return fb;
}

void wfilters(std::pmr::memory_resource *mr,
              const std::string &name, const std::string &kind,
              Value *o0, Value *o1, Value *o2, Value *o3)
{
    auto fb = wavelet_filters(name);
    if (kind.empty()) {
        // Full quadruple, MATLAB order: Lo_D, Hi_D, Lo_R, Hi_R.
        if (o0) *o0 = vecToRow(mr, fb.Lo_D);
        if (o1) *o1 = vecToRow(mr, fb.Hi_D);
        if (o2) *o2 = vecToRow(mr, fb.Lo_R);
        if (o3) *o3 = vecToRow(mr, fb.Hi_R);
    } else if (kind == "d") {
        if (o0) *o0 = vecToRow(mr, fb.Lo_D);
        if (o1) *o1 = vecToRow(mr, fb.Hi_D);
    } else if (kind == "r") {
        if (o0) *o0 = vecToRow(mr, fb.Lo_R);
        if (o1) *o1 = vecToRow(mr, fb.Hi_R);
    } else if (kind == "l") {
        if (o0) *o0 = vecToRow(mr, fb.Lo_D);
        if (o1) *o1 = vecToRow(mr, fb.Lo_R);
    } else if (kind == "h") {
        if (o0) *o0 = vecToRow(mr, fb.Hi_D);
        if (o1) *o1 = vecToRow(mr, fb.Hi_R);
    } else {
        throw Error("wfilters: unknown kind '" + kind +
                    "' (expected 'd', 'r', 'l' or 'h')",
                    0, 0, "wfilters", "", "m:wfilters:kind");
    }
}

namespace detail {

static std::string argString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("wavelet: expected a string argument",
                    0, 0, "", "", "m:wavelet:type");
    return v.toString();
}

void wfilters_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw Error("wfilters: requires the wavelet name",
                    0, 0, "wfilters", "", "m:wfilters:nargin");
    const std::string name = argString(args[0]);
    std::string kind;
    if (args.size() >= 2) kind = argString(args[1]);

    auto *mr = ctx.engine->resource();
    Value a, b, c, d;
    wfilters(mr, name, kind, &a, &b, &c, &d);

    // Number of outputs depends on `kind`.
    if (kind.empty()) {
        if (outs.size() >= 1) outs[0] = a;
        if (outs.size() >= 2) outs[1] = b;
        if (outs.size() >= 3) outs[2] = c;
        if (outs.size() >= 4) outs[3] = d;
        (void)nargout;
    } else {
        if (outs.size() >= 1) outs[0] = a;
        if (outs.size() >= 2) outs[1] = b;
    }
}

} // namespace detail

} // namespace numkit::wavelet
