// toolboxes/signal/src/regress/robust_reg.cpp
//
// CallContext register half of regress/robust.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/distributions/chi2.hpp>      // chi2inv
#include <numkit/stats/regress/regress.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include "regress/robust_detail.hpp"
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::stats {

namespace detail {

void robustfit_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("robustfit: requires (X, y [, wfun [, tune]])",
                    0, 0, "robustfit", "", "numkit:robustfit:nargin");
    RobustWeight w = RobustWeight::Bisquare;
    if (args.size() >= 3 && args[2].isChar()) {
        std::string s = args[2].toString();
        for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if      (s == "andrews")  w = RobustWeight::Andrews;
        else if (s == "bisquare") w = RobustWeight::Bisquare;
        else if (s == "cauchy")   w = RobustWeight::Cauchy;
        else if (s == "fair")     w = RobustWeight::Fair;
        else if (s == "huber")    w = RobustWeight::Huber;
        else if (s == "logistic") w = RobustWeight::Logistic;
        else if (s == "ols")      w = RobustWeight::Ols;
        else if (s == "talwar")   w = RobustWeight::Talwar;
        else if (s == "welsch")   w = RobustWeight::Welsch;
        else
            throw Error("robustfit: weight must be one of 'andrews', "
                        "'bisquare', 'cauchy', 'fair', 'huber', 'logistic', "
                        "'ols', 'talwar', 'welsch'",
                        0, 0, "robustfit", "", "numkit:robustfit:badWeight");
    }
    double tune = std::numeric_limits<double>::quiet_NaN();
    if (args.size() >= 4 && !args[3].isEmpty())
        tune = args[3].toScalar();

    // MATLAB robustfit adds a constant (intercept) term by default; the
    // 5th argument 'const' = 'on' (default) | 'off' toggles it. With the
    // intercept, b = [b0; slopes...] (b0 first). numkit previously fit the
    // raw X with no intercept, returning the wrong number of coefficients.
    bool addConst = true;
    if (args.size() >= 5 && args[4].isChar()) {
        const std::string c = args[4].toString();
        if (c == "off" || c == "Off" || c == "OFF")       addConst = false;
        else if (c == "on" || c == "On" || c == "ON")     addConst = true;
        else
            throw Error("robustfit: const must be 'on' or 'off'",
                        0, 0, "robustfit", "", "numkit:robustfit:badConst");
    }

    Value Xin = args[0];
    if (addConst) {
        const std::size_t n = Xin.dims().rows();
        const std::size_t p = Xin.dims().cols();
        Value Xaug = Value::matrix(n, p + 1, ValueType::DOUBLE,
                                   ctx.engine->resource());
        double *xa = Xaug.doubleDataMut();
        for (std::size_t i = 0; i < n; ++i) xa[i] = 1.0;        // intercept col
        for (std::size_t j = 0; j < p; ++j)
            for (std::size_t i = 0; i < n; ++i)
                xa[(j + 1) * n + i] = Xin.elemAsDouble(j * n + i);
        Xin = std::move(Xaug);
    }

    auto r = robustfit(Xin, args[1], w, tune, ctx.engine->resource());
    outs[0] = std::move(r.b);
    if (nargout > 1) outs[1] = std::move(r.s);
}

void robustcov_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("robustcov: requires (X)",
                    0, 0, "robustcov", "", "numkit:robustcov:nargin");
    auto r = robustcov(args[0], ctx.engine->resource());
    outs[0] = std::move(r.sigma);
    if (nargout > 1) outs[1] = std::move(r.mu);
}

} // namespace detail

} // namespace numkit::stats
